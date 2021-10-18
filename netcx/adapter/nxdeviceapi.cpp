// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxDevice.hpp"
#include "NxAdapterExtension.hpp"
#include "NxConfiguration.hpp"
#include "NxDriver.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include "NxDeviceApi.tmh"

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NTAPI
NETEXPORT(NetDeviceInitConfig)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ WDFDEVICE_INIT * DeviceInit
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    //
    // Register the client driver with NDIS if needed
    //
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxDriver::_CreateAndRegisterIfNeeded(nxPrivateGlobals),
        "Failed to register as NDIS driver");

    auto cxDeviceInit = WdfCxDeviceInitAllocate(DeviceInit);

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INSUFFICIENT_RESOURCES,
        cxDeviceInit == nullptr,
        "Failed to allocate WDFCXDEVICE_INIT object");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxDevice::_Create(
            nxPrivateGlobals,
            DeviceInit),
        "Failed to allocate NxDevice context");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfCxDeviceInitAssignPreprocessorRoutines(cxDeviceInit),
        "Failed to set IRP preprocessor routines");

    //
    // NDIS powers up the device on getting remove irp unless it has already received surprise
    // remove  irp. WDF however may invoke EvtReleaseHardware (which acts as remove event for NDIs-WDF)
    // even before sr has been received, e.g., in case of power up/down failure.
    // and there is no way to make a disticntion among various reasons for calling releaseHw. The
    // question is, is there a need to making such distinction. One potential reason could be
    // to maintain legacy behavior.
    // The below call will ensure that WDF receives Surprise remove irp before calling release hardware.
    //
    WdfDeviceInitSetReleaseHardwareOrderOnFailure(
        DeviceInit,
        WdfReleaseHardwareOrderOnFailureAfterDescendants);

    //
    // Set the DeviceObject features as done by Ndis.sys for its miniports
    //
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_PHYSICAL_NETCARD);
    WdfDeviceInitSetCharacteristics(DeviceInit, FILE_DEVICE_SECURE_OPEN, TRUE);
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);
    WdfDeviceInitSetPowerPageable(DeviceInit);

    //
    // Opt in for wdf remove locks.
    //
    WDF_REMOVE_LOCK_OPTIONS  removeLockOptions;
    WDF_REMOVE_LOCK_OPTIONS_INIT(&removeLockOptions, WDF_REMOVE_LOCK_OPTION_ACQUIRE_FOR_IO);
    WdfDeviceInitSetRemoveLockOptions(DeviceInit, &removeLockOptions);

    //
    // Set WDF Cx PnP and Power callbacks
    //
    SetCxPnpPowerCallbacks(cxDeviceInit);

    //
    // Set WDF Cx Power Policy callbacks
    //
    SetCxPowerPolicyCallbacks(cxDeviceInit);

    return STATUS_SUCCESS;
}

WDFAPI
_Must_inspect_result_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NETEXPORT(NetDeviceOpenConfiguration)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ WDFDEVICE Device,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * ConfigurationAttributes,
    _Out_ NETCONFIGURATION * Configuration
)
/*++
Routine Description:

    This routine opens the default configuration of the Device object
Arguments:

    Device - Pointer to the Device created in a prior call

    ConfigurationAttributes - A pointer to a WDF_OBJECT_ATTRIBUTES structure
        that contains driver-supplied attributes for the new configuration object.
        This parameter is optional and can be WDF_NO_OBJECT_ATTRIBUTES.

        The Configuration object is parented to the Device object thus
        the caller must not specify a parent object.

    Configuration - (output) Address of a location that receives a handle to the
        device configuration object.

Returns:
     NTSTATUS

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    *Configuration = nullptr;

    NxConfiguration * nxConfiguration;
    CX_RETURN_IF_NOT_NT_SUCCESS(NxConfiguration::_Create<WDFDEVICE>(nxPrivateGlobals,
        Device,
        NULL,
        &nxConfiguration));

    NTSTATUS status = nxConfiguration->Open();

    if (!NT_SUCCESS(status)){

        nxConfiguration->Close();
        return status;
    }

    if (ConfigurationAttributes != WDF_NO_OBJECT_ATTRIBUTES){

        status = nxConfiguration->AddAttributes(ConfigurationAttributes);

        if (!NT_SUCCESS(status)){
            nxConfiguration->Close();

            return status;
        }
    }

    *Configuration = nxConfiguration->GetFxObject();

    return status;
}

WDFAPI
_Must_inspect_result_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NETEXPORT(NetDeviceAssignSupportedOidList)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ WDFDEVICE Device,
    _In_ NDIS_OID const * SupportedOids,
    _In_ SIZE_T SupportedOidsCount
)
/*++
Routine Description:

    This routine sets the OIDs supported by the device

Arguments:

    Device - Pointer to the Device created in a prior call

    SupportedOids - A pointer to the OID list that the device supports

    SupportedOidsCount - Count of OIDs supported

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    if (!nxPrivateGlobals->IsClientVersionGreaterThanOrEqual(2, 1))
    {
        nxPrivateGlobals->ExtensionType = MediaExtensionTypeFromClientGlobals(nxPrivateGlobals->ClientDriverGlobals);
    }

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto nxDevice = GetNxDeviceFromHandle(Device);
    return nxDevice->AssignSupportedOidList(SupportedOids, SupportedOidsCount);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetDeviceInitSetResetConfig)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ PWDFDEVICE_INIT DeviceInit,
    _In_ NET_DEVICE_RESET_CONFIG * ResetConfig
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, ResetConfig);

    auto nxDevice = WdfCxDeviceInitGetTypedContext(
        DeviceInit,
        NxDevice);

    nxDevice->SetEvtDeviceResetCallback(ResetConfig->EvtDeviceReset);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetDeviceInitSetPowerPolicyEventCallbacks)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _Inout_ PWDFDEVICE_INIT DeviceInit,
    _In_ CONST NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS * Callbacks
)
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyTypeSize(privateGlobals, Callbacks);

    auto nxDevice = WdfCxDeviceInitGetTypedContext(
        DeviceInit,
        NxDevice);

    nxDevice->SetPowerPolicyEventCallbacks(Callbacks);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetDeviceInitSetResetCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ PWDFDEVICE_INIT DeviceInit,
    _In_ const NET_DEVICE_RESET_CAPABILITIES * Capabilities
)
{
    auto nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxDevice = WdfCxDeviceInitGetTypedContext(DeviceInit, NxDevice);
    nxDevice->SetResetCapabilities(Capabilities);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetDeviceStoreResetDiagnostics)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ WDFDEVICE Device,
    _In_ SIZE_T ResetDiagnosticsSize,
    _In_reads_bytes_(ResetDiagnosticsSize) const UINT8 * ResetDiagnosticsBuffer
)
{
    auto nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyResetDiagnosticsSize(nxPrivateGlobals, ResetDiagnosticsSize);

    auto nxDevice = GetNxDeviceFromHandle(Device);

    //
    // NetDeviceStoreResetDiagnostics can only be called once when client
    // driver is handling EVT_NET_DEVICE_COLLECT_RESET_DIAGNOSTICS.
    //
    Verifier_VerifyIsCollectingResetDiagnostics(nxPrivateGlobals, nxDevice);

    nxDevice->StoreResetDiagnostics(ResetDiagnosticsSize, ResetDiagnosticsBuffer);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetDeviceRequestReset)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ WDFDEVICE Device
)
{
    auto nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->PlatformLevelDeviceResetWithDiagnostics();
}
