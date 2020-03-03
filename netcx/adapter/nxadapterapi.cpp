// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the "C" interface for the NxAdapter object.

--*/

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxAdapterApi.tmh"

#include "NxAdapter.hpp"
#include "NetPacketExtensionPrivate.h"
#include "NxConfiguration.hpp"
#include "NxDevice.hpp"
#include "NxDriver.hpp"
#include "NxWake.hpp"
#include "verifier.hpp"
#include "version.hpp"

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterDeviceInitConfig)(
    _In_    NET_DRIVER_GLOBALS * Globals,
    _Inout_ WDFDEVICE_INIT * DeviceInit
)
/*++
Routine Description:

    The client driver calls this API from its EvtDriverDeviceAdd
    to let NetAdapterCx modify WDFDEVICE_INIT.

Arguments:

    Globals -
    DeviceInit - Device initialization parameters received in EvtDriverDeviceAdd

Return Value:

    STATUS_SUCCESS or appropriate error value

--*/
{
    NTSTATUS status;
    WDF_REMOVE_LOCK_OPTIONS  removeLockOptions;
    WDFCXDEVICE_INIT * cxDeviceInit;

    //
    // Validate Parameters
    //

    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    //
    // Register the client driver with NDIS if needed
    //
    status = NxDriver::_CreateAndRegisterIfNeeded(pNxPrivateGlobals);

    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    cxDeviceInit = WdfCxDeviceInitAllocate(DeviceInit);

    if (cxDeviceInit == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    status = WdfCxDeviceInitAssignPreprocessorRoutines(cxDeviceInit);

    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

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
    WDF_REMOVE_LOCK_OPTIONS_INIT(&removeLockOptions, WDF_REMOVE_LOCK_OPTION_ACQUIRE_FOR_IO);
    WdfDeviceInitSetRemoveLockOptions(DeviceInit, &removeLockOptions);

    //
    // Set WDF Cx PnP and Power callbacks
    //
    SetCxPnpPowerCallbacks(cxDeviceInit);

Exit:

    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NETADAPTER_INIT *
NETEXPORT(NetAdapterInitAllocate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ WDFDEVICE Device
)
/*++
Routine Description:

    WDF client driver calls this API to allocate an opaque init object for adapters
    layered on top of a PnP device.

Arguments:

    Globals - Client driver's globals
    Device - Device to which an adapter will be layered on top off

Return Value:

    An pointer to an init object.

Remarks

    The init object's lifetime is owned by the WDF client driver. After using it to create
    an adapter it has to call NetAdapterInitFree.

--*/
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, Device);

    auto adapterInit = wil::make_unique_nothrow<AdapterInit>();

    if (!adapterInit)
    {
        return nullptr;
    }

    adapterInit->Device = Device;

    return reinterpret_cast<NETADAPTER_INIT *>(adapterInit.release());
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterInitFree)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER_INIT * AdapterInit
)
/*++
Routine Description:

    Frees the AdapterInit object.

Arguments:

    Globals - Client driver's globals
    AdapterInit - Opaque adapter init object

Return Value:

    None

--*/
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, AdapterInit);

    auto adapterInit = GetAdapterInitFromHandle(privateGlobals, AdapterInit);

    adapterInit->~AdapterInit();
    ExFreePool(adapterInit);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterInitSetDatapathCallbacks)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTER_INIT * AdapterInit,
    _In_ NET_ADAPTER_DATAPATH_CALLBACKS * DatapathCallbacks
)
/*++
Routine Description:

    Sets the datapath callbacks for the adapter that will be created with this AdapterInit.

Arguments:

    Globals - Client driver's globals
    AdapterInit - Adapter init object
    DatapathCallbacks - An initialize structure containing the adapter's datapath callbacks

Return Value:

    None

--*/
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, DatapathCallbacks);
    Verifier_VerifyDatapathCallbacks(privateGlobals, DatapathCallbacks);

    auto adapterInit = GetAdapterInitFromHandle(privateGlobals, AdapterInit);

    RtlCopyMemory(
        &adapterInit->DatapathCallbacks,
        DatapathCallbacks,
        DatapathCallbacks->Size);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterInitSetNetRequestAttributes)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTER_INIT * AdapterInit,
    _In_ WDF_OBJECT_ATTRIBUTES * NetRequestAttributes
)
/*++
Routine Description:

    Sets the attributes of NETREQUEST objects created by NetAdapterCx.

Arguments:

    Globals - Client driver's globals
    AdapterInit - Adapter init object
    NetRequestAttributes - Initialized WDF object attributes

Return Value:

    None

--*/
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, AdapterInit);
    Verifier_VerifyNotNull(privateGlobals, NetRequestAttributes);

    auto adapterInit = GetAdapterInitFromHandle(privateGlobals, AdapterInit);

    Verifier_VerifyAdapterInitNotUsed(privateGlobals, adapterInit);

    RtlCopyMemory(
        &adapterInit->NetRequestAttributes,
        NetRequestAttributes,
        NetRequestAttributes->Size);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterInitSetNetPowerSettingsAttributes)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTER_INIT * AdapterInit,
    _In_ WDF_OBJECT_ATTRIBUTES * NetPowerSettingsAttributes
)
/*++
Routine Description:

    Sets the attributes of the NETPOWERSETTINGS object created by NetAdapterCx

Arguments:

    Globals - Client driver's globals
    AdapterInit - Adapter init object
    NetPowerSettingsAttributes - Initialized WDF object attributes

Return Value:

    None

--*/
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, AdapterInit);
    Verifier_VerifyNotNull(privateGlobals, NetPowerSettingsAttributes);

    auto adapterInit = GetAdapterInitFromHandle(privateGlobals, AdapterInit);

    Verifier_VerifyAdapterInitNotUsed(privateGlobals, adapterInit);

    RtlCopyMemory(
        &adapterInit->NetPowerSettingsAttributes,
        NetPowerSettingsAttributes,
        NetPowerSettingsAttributes->Size);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterCreate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER_INIT * AdapterInit,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * AdapterAttributes,
    _Out_ NETADAPTER* Adapter
)
/*++
Routine Description:
    The client driver calls this method to create an NETADAPTER
    object

    The client driver must call this from its EvtDriverDeviceAdd routine after
    the client has successfully create a WDFDEVICE object.

Arguments:

    Device : A handle to a WDFDEVICE object that represents the device.
    This Device would be the parent of the new NETADAPTER object

    AdapterInit : A pointer to an NETADAPTER_INIT structure that the client
    allocated in a previous call to NetAdapterInitAllocate.
    This structure is not visible to the client, but hold important context
    information for NetAdapterCx suchs as the client's pnp power callbacks.

    AdapterAttributes : Pointer to the WDF_OBJECT_ATTRIBUTES structure. This is
    the attributes structure that a Wdf client can pass at the time of
    creating any wdf object.

    The ParentObject of this structure should be NULL as NETADAPTER object
    is parented to the the WDFDEVICE passed in.

    Config : Pointer to a structure containing configuration parameters.
    This contains client callbacks.

    Adapter : ouptut NETADAPTER object

Return Value:

    STATUS_SUCCESS upon success.
    Returns other NTSTATUS values.

--*/
{
    *Adapter = nullptr;

    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto adapterInit = GetAdapterInitFromHandle(pNxPrivateGlobals, AdapterInit);

    Verifier_VerifyAdapterInitNotUsed(pNxPrivateGlobals, adapterInit);

    // We don't yet support software network interfaces
    NT_ASSERT(adapterInit->Device != nullptr);

    // Client is not allowed to specify a parent for a NETADAPTER object
    Verifier_VerifyObjectAttributesParentIsNull(pNxPrivateGlobals, AdapterAttributes);

    // Create a NxDevice context only if needed
    auto nxDevice = GetNxDeviceFromHandle(adapterInit->Device);
    if (nxDevice == nullptr)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NxDevice::_Create(
                pNxPrivateGlobals,
                adapterInit->Device,
                &nxDevice),
            "Failed to create the NxDevice context. WDFDEVICE=%p",
            adapterInit->Device);
    }

    NxAdapter * nxAdapter;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxAdapter::_Create(
            *pNxPrivateGlobals,
            *adapterInit,
            AdapterAttributes,
            &nxAdapter),
        "Failed to create the NxAdapter context. WDFDEVICE=%p",
        adapterInit->Device);

    //
    // Do not introduce failures after this point. See note at the end of
    // NxAdapter::_Create that explains this.
    //

    *Adapter = nxAdapter->GetFxObject();

    adapterInit->CreatedAdapter = *Adapter;

    return STATUS_SUCCESS;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterStart)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter
)
{
    NX_PRIVATE_GLOBALS *privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);

    NxAdapter *adapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(privateGlobals, adapter);

    return adapter->ClientStart();
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterStop)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter
)
{
    NX_PRIVATE_GLOBALS *privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);

    NxAdapter *adapter = GetNxAdapterFromHandle(Adapter);
    adapter->ClientStop();
}

WDFAPI
NDIS_HANDLE
NETEXPORT(NetAdapterWdmGetNdisHandle)(
    _In_ NET_DRIVER_GLOBALS *              Globals,
    _In_ NETADAPTER                        Adapter
)
/*++
Routine Description:


Arguments:


Return Value:

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxAdapterFromHandle(Adapter)->GetNdisHandle();
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
NET_LUID
NETEXPORT(NetAdapterGetNetLuid)(
    _In_     NET_DRIVER_GLOBALS *                Globals,
    _In_     NETADAPTER                          Adapter
)
/*++
Routine Description:

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

Returns:

    The adapter's NET_LUID.

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    return GetNxAdapterFromHandle(Adapter)->GetNetLuid();
}

WDFAPI
_Must_inspect_result_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NETEXPORT(NetAdapterOpenConfiguration)(
    _In_     NET_DRIVER_GLOBALS *                Globals,
    _In_     NETADAPTER                          Adapter,
    _In_opt_ WDF_OBJECT_ATTRIBUTES *             ConfigurationAttributes,
    _Out_    NETCONFIGURATION*                   Configuration
)
/*++
Routine Description:

    This routine opens the default configuration of the Adapter object
Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    ConfigurationAttributes - A pointer to a WDF_OBJECT_ATTRIBUTES structure
        that contains driver-supplied attributes for the new configuration object.
        This parameter is optional and can be WDF_NO_OBJECT_ATTRIBUTES.

        The Configuration object is parented to the Adapter object thus
        the caller must not specify a parent object.

    Configuration - (output) Address of a location that receives a handle to the
        adapter configuration object.

Returns:
    NTSTATUS

--*/
{
    NTSTATUS status;

    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    *Configuration = NULL;

    NxConfiguration * nxConfiguration;
    status = NxConfiguration::_Create(pNxPrivateGlobals,
                                      GetNxAdapterFromHandle(Adapter),
                                      NULL,
                                      &nxConfiguration);
    if (!NT_SUCCESS(status)){
        return status;
    }

    status = nxConfiguration->Open();

    if (!NT_SUCCESS(status)){

        nxConfiguration->DeleteFromFailedOpen();

        return status;
    }

    //
    // Add The Client's Attributes as the last step, no failure after that point
    //
    if (ConfigurationAttributes != WDF_NO_OBJECT_ATTRIBUTES){

        status = nxConfiguration->AddAttributes(ConfigurationAttributes);

        if (!NT_SUCCESS(status)){
            nxConfiguration->Close();

            return status;
        }
    }

    //
    // *** NOTE: NO FAILURE AFTER THIS. **
    // We dont want clients Cleanup/Destroy callbacks
    // to be called unless this call succeeds.
    //
    *Configuration = nxConfiguration->GetFxObject();

    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetDataPathCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_TX_CAPABILITIES * TxCapabilities,
    _In_ NET_ADAPTER_RX_CAPABILITIES * RxCapabilities
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    Verifier_VerifyTypeSize(pNxPrivateGlobals, TxCapabilities);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, RxCapabilities);

    Verifier_VerifyNetAdapterTxCapabilities(pNxPrivateGlobals, TxCapabilities);
    Verifier_VerifyNetAdapterRxCapabilities(pNxPrivateGlobals, RxCapabilities);

    if (TxCapabilities->DmaCapabilities != nullptr)
    {
        Verifier_VerifyTypeSize(pNxPrivateGlobals, TxCapabilities->DmaCapabilities);
    }

    if (RxCapabilities->DmaCapabilities != nullptr)
    {
        Verifier_VerifyTypeSize(pNxPrivateGlobals, RxCapabilities->DmaCapabilities);
    }

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(pNxPrivateGlobals, nxAdapter);

    nxAdapter->SetDatapathCapabilities(
        TxCapabilities,
        RxCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetReceiveScalingCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const * Capabilities
)
{
    auto const pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyNotNull(pNxPrivateGlobals, Capabilities);
    Verifier_VerifyReceiveScalingCapabilities(pNxPrivateGlobals, Capabilities);

    auto const nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(pNxPrivateGlobals, nxAdapter);

    nxAdapter->SetReceiveScalingCapabilities(*Capabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetLinkLayerMtuSize)(
    _In_ NET_DRIVER_GLOBALS *                   DriverGlobals,
    _In_ NETADAPTER                             Adapter,
    _In_ ULONG                                  MtuSize
)
/*++
Routine Description:

    This routine changes the MTU size at runtime.

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    MtuSize - The adapter's new MTU size

Returns:
    VOID

Remarks:

    If called during execution of EvtAdapterSetCapabilities, no MTU change is indicated
    to NDIS.

    If called after EvtAdapterSetCapabilities, NDIS will receive a status indication
    of the change and all of the adapter's Tx/Rx queues will be destroyed then reinitialized.

--*/
{
    auto const pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyMtuSize(pNxPrivateGlobals, MtuSize);

    auto const nxAdapter = GetNxAdapterFromHandle(Adapter);

    nxAdapter->SetMtuSize(MtuSize);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetLinkLayerCapabilities)(
    _In_ NET_DRIVER_GLOBALS *                   DriverGlobals,
    _In_ NETADAPTER                             Adapter,
    _In_ NET_ADAPTER_LINK_LAYER_CAPABILITIES *  LinkLayerCapabilities
)
/*++
Routine Description:

    This routine sets the Mac Capabilities of the Network Adapter.
    The caller must call this method from its EvtAdapterSetCapabilities callback

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    LinkLayerCapabilities - Pointer to a initialized NET_ADAPTER_LINK_LAYER_CAPABILITIES
    structure

Returns:
    VOID

Remarks:

    This function must only be called from within EvtAdapterSetCapabilities
    callback.

    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

    Subsequently when the NetAdapter driver returns from its
    EvtAdapterSetCapabilities, NetAdapterCx would fail the
    EvtDevicePrepareHardware.

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, LinkLayerCapabilities);
    Verifier_VerifyLinkLayerCapabilities(pNxPrivateGlobals, LinkLayerCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(pNxPrivateGlobals, nxAdapter);

    nxAdapter->SetLinkLayerCapabilities(*LinkLayerCapabilities);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetPermanentLinkLayerAddress)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS * LinkLayerAddress
)
/*++
Routine Description:

    This routine sets the permanent link layer address of the
    network adapter, which is typically read from hardware.

    The caller must call this method from its EvtAdapterSetCapabilities
    callback.

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    LinkLayerAddress - Pointer to an initialized NET_ADAPTER_LINK_LAYER_ADDRESS
    structure

Returns:

    VOID

Remarks:

    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

    Subsequently when the NetAdapter driver returns from its
    EvtAdapterSetCapabilities, NetAdapterCx would fail the
    EvtDevicePrepareHardware.

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(pNxPrivateGlobals, nxAdapter);
    Verifier_VerifyLinkLayerAddress(pNxPrivateGlobals, LinkLayerAddress);

    nxAdapter->SetPermanentLinkLayerAddress(LinkLayerAddress);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetCurrentLinkLayerAddress)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS * LinkLayerAddress
)
/*++

Routine Description:

    This routine sets the current link layer address of the
    network adapter, which can be equal to the permanent
    address or something else.

    The client can also call this API at a later time to inform
    that it has changed its current link layer address, doing so
    will cause the data path queues to be destroyed and created
    again.

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    LinkLayerAddress - Pointer to an initialized NET_ADAPTER_LINK_LAYER_ADDRESS
    structure

Returns:

    VOID

Remarks:

    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

    Subsequently when the NetAdapter driver returns from its
    EvtAdapterSetCapabilities, NetAdapterCx would fail the
    EvtDevicePrepareHardware.

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyLinkLayerAddress(pNxPrivateGlobals, LinkLayerAddress);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    nxAdapter->SetCurrentLinkLayerAddress(LinkLayerAddress);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetPowerCapabilities)(
    _In_ NET_DRIVER_GLOBALS *            DriverGlobals,
    _In_ NETADAPTER                      Adapter,
    _In_ NET_ADAPTER_POWER_CAPABILITIES * PowerCapabilities
)
/*++
Routine Description:

    This routine sets the Power Capabilities of the Network Adapter.

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    PowerCapabilities - Pointer to a initialized NET_ADAPTER_POWER_CAPABILITIES
    structure

Returns:
    VOID

Remarks:
    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

    Subsequently when the NetAdapter driver returns from its
    EvtAdapterSetCapabilities, NetAdapterCx would fail the
    EvtDevicePrepareHardware.

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, PowerCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyPowerCapabilities(pNxPrivateGlobals,
        *nxAdapter,
        *PowerCapabilities);

    nxAdapter->SetPowerCapabilities(*PowerCapabilities);
}

WDFAPI
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NETEXPORT(NetAdapterSetLinkState)(
    _In_ NET_DRIVER_GLOBALS *    DriverGlobals,
    _In_ NETADAPTER              Adapter,
    _In_ NET_ADAPTER_LINK_STATE * State
)
/*++
Routine Description:

    This routine sets the Current Link State of the Network Adapter.

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    State - Pointer to a initialized NET_ADAPTER_LINK_STATE structure
    representing the current Link State.

Returns:
    VOID

Remarks:

    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);
    Verifier_VerifyTypeSize<NET_ADAPTER_LINK_STATE>(pNxPrivateGlobals, State);
    Verifier_VerifyCurrentLinkState(pNxPrivateGlobals, State);

    GetNxAdapterFromHandle(Adapter)->SetCurrentLinkState(*State);
}

WDFAPI
_IRQL_requires_max_(DISPATCH_LEVEL)
NETPOWERSETTINGS
NETEXPORT(NetAdapterGetPowerSettings)(
    _In_ NET_DRIVER_GLOBALS *   Globals,
    _In_ NETADAPTER             Adapter
)
/*++
Routine Description:
    Returns the NETPOWERSETTINGS associated with the NETADAPTER.

Arguments:

    Globals -
    Adapter - The NETADAPTER handle

Return Value:

    NETPOWERSETTINGS
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);

    return GetNxAdapterFromHandle(Adapter)->m_NxWake->GetFxObject();
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterRegisterPacketExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_PACKET_EXTENSION const * ExtensionToRegister
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, ExtensionToRegister);
    Verifier_VerifyNetPacketExtension(pNxPrivateGlobals, ExtensionToRegister);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(pNxPrivateGlobals, nxAdapter);

    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = ExtensionToRegister->Name;
    extensionPrivate.Size = ExtensionToRegister->ExtensionSize;
    extensionPrivate.Version = ExtensionToRegister->Version;
    //
    // CAUTION:WDF Public API uses FILE_XXX_ALIGNMENT, which is like 0xf,
    // but rest of CX code uses regular windows alignment convention
    //
    extensionPrivate.NonWdfStyleAlignment = ExtensionToRegister->Alignment + 1;

    //
    // CX would allocate a new buffer to store extension information
    // and as for now, free them during adapter halting. Caller
    // can free/repurpose the memory used for NET_PACKET_EXTENSION
    //

    return nxAdapter->RegisterPacketExtension(&extensionPrivate);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterQueryRegisteredPacketExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_PACKET_EXTENSION_QUERY const * ExtensionToQuery
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, ExtensionToQuery);
    Verifier_VerifyNetPacketExtensionQuery(pNxPrivateGlobals, ExtensionToQuery);

    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = ExtensionToQuery->Name;
    extensionPrivate.Version = ExtensionToQuery->Version;

    return GetNxAdapterFromHandle(Adapter)->QueryRegisteredPacketExtension(&extensionPrivate);
}


_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetDeviceSetResetConfig)(
    _In_
    NET_DRIVER_GLOBALS * DriverGlobals,
    _In_
    WDFDEVICE WdfDevice,
    _In_
    NET_DEVICE_RESET_CONFIG * ResetConfig
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, ResetConfig);

    auto nxDevice = GetNxDeviceFromHandle(WdfDevice);
    nxDevice->SetEvtDeviceResetCallback(ResetConfig->EvtDeviceReset);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterOffloadSetChecksumCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES * HardwareCapabilities
)
/*++
Routine Description:

    This routine sets the Checksum hardware capabilities of the Network Adapter and provides
    a callback to notify active checksum capabilities to the client driver.

    The client driver must call this method before calling NetAdapterStart

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    HardwareCapabilities - Pointer to a initialized NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES
    structure representing the hardware capabilities of network adapter

Returns:
    VOID
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, HardwareCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(pNxPrivateGlobals, nxAdapter);

    nxAdapter->m_NxOffloadManager->SetChecksumHardwareCapabilities(HardwareCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsChecksumIPv4Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, capabilities);

    return capabilities->IPv4;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsChecksumTcpEnabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, capabilities);

    return capabilities->Tcp;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsChecksumUdpEnabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, capabilities);

    return capabilities->Udp;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterOffloadSetLsoCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES * HardwareCapabilities
)
/*++
Routine Description:

    This routine sets the LSO hardware capabilities of the Network Adapter and provides
    a callback to notify active LSO capabilities to the client driver.

    The client driver must call this method before calling NetAdapterStart

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    HardwareCapabilities - Pointer to a initialized NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES
    structure representing the hardware capabilities of network adapter

Returns:
    VOID
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, HardwareCapabilities);
    Verifier_VerifyLsoCapabilities(pNxPrivateGlobals, HardwareCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(pNxPrivateGlobals, nxAdapter);

    nxAdapter->m_NxOffloadManager->SetLsoHardwareCapabilities(HardwareCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsLsoIPv4Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, capabilities);
    Verifier_VerifyLsoCapabilities(pNxPrivateGlobals, capabilities);

    return capabilities->IPv4;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsLsoIPv6Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, capabilities);
    Verifier_VerifyLsoCapabilities(pNxPrivateGlobals, capabilities);

    return capabilities->IPv6;
}

