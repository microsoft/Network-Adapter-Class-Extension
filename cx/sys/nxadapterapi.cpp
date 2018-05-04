// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the "C" interface for the NxAdapter object.

--*/

#include "Nx.hpp"
#include "NetPacketExtensionPrivate.h"

#include "NxAdapterApi.tmh"

//
// extern the whole file
//
extern "C" {

NTSTATUS
AssignFdoName(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

NTSTATUS
SetPreprocessorRoutines(
    _Inout_ PWDFCXDEVICE_INIT CxDeviceInit
    );

VOID
SetCxPnpPowerCallbacks(
    _Inout_ PWDFCXDEVICE_INIT CxDeviceInit
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterDeviceInitConfig)(
    _In_    PNET_DRIVER_GLOBALS             Globals,
    _Inout_ PWDFDEVICE_INIT                 DeviceInit
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
    FuncEntry(FLAG_ADAPTER);

    NTSTATUS status;
    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;
    WDF_REMOVE_LOCK_OPTIONS  removeLockOptions;
    PWDFCXDEVICE_INIT cxDeviceInit;

    //
    // Validate Parameters
    //

    pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    cxDeviceInit = WdfCxDeviceInitAllocate(DeviceInit);

    if (cxDeviceInit == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    status = AssignFdoName(DeviceInit);

    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    //
    // This SDDL is enough to let ndispnp.lib successfully open a handle to a network
    // adapter (without changing their code).
    //
    // Note: If we want to give a more restrictive default SDDL to the FDO we need to find applications
    // that open handles to NDIS and make sure they work with the new SDDL.
    //
    status = WdfDeviceInitAssignSDDLString(DeviceInit, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R);

    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    status = SetPreprocessorRoutines(cxDeviceInit);

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

    FuncExit(FLAG_ADAPTER);
    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterCreate)(
    _In_     PNET_DRIVER_GLOBALS         Globals,
    _In_     WDFDEVICE                   Device,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES      AdapterAttributes,
    _In_     PNET_ADAPTER_CONFIG         Configuration,
    _Out_    NETADAPTER*                 Adapter
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
    FuncEntry(FLAG_ADAPTER);
    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;
    NTSTATUS                 status;
    PNxAdapter               nxAdapter;
    PNxDevice                nxDevice;

    *Adapter = NULL;

    pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    Verifier_VerifyTypeSize(pNxPrivateGlobals, Configuration);
    // Client is not allowed to specify a parent for a NETADAPTER object
    Verifier_VerifyObjectAttributesParentIsNull(pNxPrivateGlobals, AdapterAttributes);
    Verifier_VerifyNetAdapterConfig(pNxPrivateGlobals, Configuration);

    //
    // First see if we need to register with NDIS
    //
    status = NxDriver::_CreateAndRegisterIfNeeded(WdfDeviceGetDriver(Device),
        pNxPrivateGlobals);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    // Create a NxDevice context only if needed
    nxDevice = GetNxDeviceFromHandle(Device);

    if (nxDevice == nullptr)
    {
        status = NxDevice::_Create(pNxPrivateGlobals,
            Device,
            &nxDevice);

        if (!NT_SUCCESS(status)) {
            goto Exit;
        }
    }

    status = NxAdapter::_Create(pNxPrivateGlobals,
                                Device,
                                AdapterAttributes,
                                Configuration,
                                &nxAdapter);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    //
    // Do not introduce failures after this point. See note at the end of
    // NxAdapter::_Create that explains this.
    //

    *Adapter = nxAdapter->GetFxObject();

Exit:

    FuncExit(FLAG_ADAPTER);
    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterStart)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter
    )
{
    FuncEntry(FLAG_ADAPTER);
    NX_PRIVATE_GLOBALS *privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);

    NxAdapter *adapter = GetNxAdapterFromHandle(Adapter);
    NTSTATUS status = adapter->ApiStart();

    FuncExit(FLAG_ADAPTER);
    return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterStop)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter
    )
{
    FuncEntry(FLAG_ADAPTER);
    NX_PRIVATE_GLOBALS *privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);

    NxAdapter *adapter = GetNxAdapterFromHandle(Adapter);
    adapter->Stop();

    FuncExit(FLAG_ADAPTER);
}

WDFAPI
NDIS_HANDLE
NETEXPORT(NetAdapterWdmGetNdisHandle)(
    _In_ PNET_DRIVER_GLOBALS               Globals,
    _In_ NETADAPTER                        Adapter
    )
/*++
Routine Description:


Arguments:


Return Value:

--*/
{
    FuncEntry(FLAG_ADAPTER);

    NDIS_HANDLE              ndisAdapterHandle;
    PNxAdapter               nxAdapter;

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    nxAdapter = GetNxAdapterFromHandle(Adapter);

    ndisAdapterHandle = nxAdapter->m_NdisAdapterHandle;

    FuncExit(FLAG_ADAPTER);

    return ndisAdapterHandle;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
NET_LUID
NETEXPORT(NetAdapterGetNetLuid)(
    _In_     PNET_DRIVER_GLOBALS                 Globals,
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
    FuncEntry(FLAG_ADAPTER);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    PNxAdapter nxAdapter = GetNxAdapterFromHandle(Adapter);
    NET_LUID   netLuid;

    netLuid = nxAdapter->GetNetLuid();

    FuncExit(FLAG_ADAPTER);

    return netLuid;
}

WDFAPI
_Must_inspect_result_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NETEXPORT(NetAdapterOpenConfiguration)(
    _In_     PNET_DRIVER_GLOBALS                 Globals,
    _In_     NETADAPTER                          Adapter,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES              ConfigurationAttributes,
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
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter               nxAdapter;
    PNxConfiguration         nxConfiguration;
    NTSTATUS                 status;
    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;

    pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    *Configuration = NULL;

    nxAdapter = GetNxAdapterFromHandle(Adapter);

    status = NxConfiguration::_Create(pNxPrivateGlobals,
                                      nxAdapter,
                                      NULL,
                                      &nxConfiguration);
    if (!NT_SUCCESS(status)){
        FuncExit(FLAG_ADAPTER);
        return status;
    }

    status = nxConfiguration->Open();

    if (!NT_SUCCESS(status)){

        nxConfiguration->DeleteFromFailedOpen();
        FuncExit(FLAG_ADAPTER);
        return status;
    }

    //
    // Add The Client's Attributes as the last step, no failure after that point
    //
    if (ConfigurationAttributes != WDF_NO_OBJECT_ATTRIBUTES){

        status = nxConfiguration->AddAttributes(ConfigurationAttributes);

        if (!NT_SUCCESS(status)){
            nxConfiguration->Close();
            FuncExit(FLAG_ADAPTER);
            return status;
        }
    }

    //
    // *** NOTE: NO FAILURE AFTER THIS. **
    // We dont want clients Cleanup/Destroy callbacks
    // to be called unless this call succeeds.
    //
    *Configuration = nxConfiguration->GetFxObject();
    FuncExit(FLAG_ADAPTER);

    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetDataPathCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ PNET_ADAPTER_TX_CAPABILITIES TxCapabilities,
    _In_ PNET_ADAPTER_RX_CAPABILITIES RxCapabilities
    )
{
    FuncEntry(FLAG_ADAPTER);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

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

    PNxAdapter nxAdapter;
    nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyEvtAdapterSetCapabilitiesInProgress(pNxPrivateGlobals, nxAdapter);

    nxAdapter->SetDatapathCapabilities(
        TxCapabilities,
        RxCapabilities);

    FuncExit(FLAG_ADAPTER);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetReceiveScalingCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const * Capabilities
    )
{
    FuncEntry(FLAG_ADAPTER);

    auto const pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyNotNull(pNxPrivateGlobals, Capabilities);
    Verifier_VerifyReceiveScalingCapabilities(pNxPrivateGlobals, Capabilities);

    auto const nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyEvtAdapterSetCapabilitiesInProgress(pNxPrivateGlobals, nxAdapter);

    nxAdapter->SetReceiveScalingCapabilities(*Capabilities);

    FuncExit(FLAG_ADAPTER);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
PNET_ADAPTER_RECEIVE_SCALING_HASH_SECRET_KEY
NETEXPORT(NetAdapterGetReceiveScalingHashSecretKey)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter
    )
{
    UNREFERENCED_PARAMETER(Adapter);

    FuncEntry(FLAG_ADAPTER);

    auto const pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    FuncExit(FLAG_ADAPTER);

    return nullptr;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE
NETEXPORT(NetAdapterGetReceiveScalingProtocolTypes)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter
    )
{
    UNREFERENCED_PARAMETER(Adapter);

    FuncEntry(FLAG_ADAPTER);

    auto const pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    FuncExit(FLAG_ADAPTER);

    return GetNxAdapterFromHandle(Adapter)->m_ReceiveScaling.ProtocolType;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE
NETEXPORT(NetAdapterGetReceiveScalingHashType)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter
    )
{
    UNREFERENCED_PARAMETER(Adapter);

    FuncEntry(FLAG_ADAPTER);

    auto const pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    FuncExit(FLAG_ADAPTER);

    return GetNxAdapterFromHandle(Adapter)->m_ReceiveScaling.HashType;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetLinkLayerMtuSize)(
    _In_ PNET_DRIVER_GLOBALS                    DriverGlobals,
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
    FuncEntry(FLAG_ADAPTER);

    auto const pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyMtuSize(pNxPrivateGlobals, MtuSize);

    auto const nxAdapter = GetNxAdapterFromHandle(Adapter);

    nxAdapter->SetMtuSize(MtuSize);

    FuncExit(FLAG_ADAPTER);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetLinkLayerCapabilities)(
    _In_ PNET_DRIVER_GLOBALS                    DriverGlobals,
    _In_ NETADAPTER                             Adapter,
    _In_ PNET_ADAPTER_LINK_LAYER_CAPABILITIES   LinkLayerCapabilities
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
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter               nxAdapter;
    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;

    pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, LinkLayerCapabilities);
    Verifier_VerifyLinkLayerCapabilities(pNxPrivateGlobals, LinkLayerCapabilities);

    nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyEvtAdapterSetCapabilitiesInProgress(pNxPrivateGlobals, nxAdapter);

    RtlCopyMemory(&nxAdapter->m_LinkLayerCapabilities,
                  LinkLayerCapabilities,
                  sizeof(NET_ADAPTER_LINK_LAYER_CAPABILITIES));

    FuncExit(FLAG_ADAPTER);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetPermanentLinkLayerAddress)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ PNET_ADAPTER_LINK_LAYER_ADDRESS LinkLayerAddress
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
    FuncEntry(FLAG_ADAPTER);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    PNxAdapter nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyEvtAdapterSetCapabilitiesInProgress(pNxPrivateGlobals, nxAdapter);
    Verifier_VerifyLinkLayerAddress(pNxPrivateGlobals, LinkLayerAddress);

    nxAdapter->SetPermanentLinkLayerAddress(LinkLayerAddress);

    FuncExit(FLAG_ADAPTER);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetCurrentLinkLayerAddress)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ PNET_ADAPTER_LINK_LAYER_ADDRESS LinkLayerAddress
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
    FuncEntry(FLAG_ADAPTER);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    PNxAdapter nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyLinkLayerAddress(pNxPrivateGlobals, LinkLayerAddress);

    nxAdapter->SetCurrentLinkLayerAddress(LinkLayerAddress);

    if (nxAdapter->m_Flags.SetGeneralAttributesInProgress)
    {
        //
        // SetGeneralAttributes is in progress, nothing additional needs to be
        // done
        //

        goto Exit;
    }

    nxAdapter->IndicateCurrentLinkLayerAddressToNdis();

Exit:

    FuncExit(FLAG_ADAPTER);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetPowerCapabilities)(
    _In_ PNET_DRIVER_GLOBALS             DriverGlobals,
    _In_ NETADAPTER                      Adapter,
    _In_ PNET_ADAPTER_POWER_CAPABILITIES PowerCapabilities
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
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter               nxAdapter;
    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;
    BOOLEAN setAttributesInProgress;

    pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize<NET_ADAPTER_POWER_CAPABILITIES>(pNxPrivateGlobals, PowerCapabilities);

    nxAdapter = GetNxAdapterFromHandle(Adapter);
    setAttributesInProgress = nxAdapter->m_Flags.SetGeneralAttributesInProgress;
    Verifier_VerifyPowerCapabilities(pNxPrivateGlobals,
                    PowerCapabilities,                 // New capabilities
                    setAttributesInProgress,
                    &nxAdapter->m_PowerCapabilities);  // Existing capabilities

    RtlCopyMemory(&nxAdapter->m_PowerCapabilities,
                  PowerCapabilities,
                  sizeof(NET_ADAPTER_POWER_CAPABILITIES));

    if (setAttributesInProgress) {
        nxAdapter->m_NxWake->SetPowerPreviewEvtCallbacks(
                                PowerCapabilities->EvtAdapterPreviewWakePattern,
                                PowerCapabilities->EvtAdapterPreviewProtocolOffload);

        //
        // SetGeneralAttributes is in progress, nothing additional needs to be
        // done
        //

        goto Exit;

    }

    //
    // If driver calls this API outside of EvtAdapterSetCapabilities it must not
    // change the WoL packet filter callback.
    //

    nxAdapter->IndicatePowerCapabilitiesToNdis();

Exit:

    FuncExit(FLAG_ADAPTER);
}

WDFAPI
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
NETEXPORT(NetAdapterSetCurrentLinkState)(
    _In_ PNET_DRIVER_GLOBALS     DriverGlobals,
    _In_ NETADAPTER              Adapter,
    _In_ PNET_ADAPTER_LINK_STATE CurrentLinkState
    )
/*++
Routine Description:

    This routine sets the Current Link State of the Network Adapter.

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    CurrentLinkState - Pointer to a initialized NET_ADAPTER_LINK_STATE structure
    representing the current Link State.

Returns:
    VOID

Remarks:

    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

--*/
{
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter               nxAdapter;

    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;

    pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);
    Verifier_VerifyTypeSize<NET_ADAPTER_LINK_STATE>(pNxPrivateGlobals, CurrentLinkState);
    Verifier_VerifyCurrentLinkState(pNxPrivateGlobals, CurrentLinkState);

    nxAdapter = GetNxAdapterFromHandle(Adapter);

    auto const linkStateChanged =
        nxAdapter->m_CurrentLinkState.TxLinkSpeed != CurrentLinkState->TxLinkSpeed ||
        nxAdapter->m_CurrentLinkState.RxLinkSpeed != CurrentLinkState->RxLinkSpeed ||
        nxAdapter->m_CurrentLinkState.MediaConnectState != CurrentLinkState->MediaConnectState ||
        nxAdapter->m_CurrentLinkState.MediaDuplexState != CurrentLinkState->MediaDuplexState ||
        nxAdapter->m_CurrentLinkState.SupportedPauseFunctions != CurrentLinkState->SupportedPauseFunctions ||
        nxAdapter->m_CurrentLinkState.AutoNegotiationFlags != CurrentLinkState->AutoNegotiationFlags;

    if (linkStateChanged) {
        nxAdapter->m_CurrentLinkState = *CurrentLinkState;

        if (nxAdapter->m_Flags.SetGeneralAttributesInProgress) {

            //
            // SetGeneralAttributes is in progress, nothing additional needs to be
            // done
            //

            goto Exit;

        }

        nxAdapter->IndicateCurrentLinkStateToNdis();
    }
Exit:

    FuncExit(FLAG_ADAPTER);
}

NDIS_STRING ndisFdoDeviceStr = NDIS_STRING_CONST("\\Device\\NDMP");

NTSTATUS
AssignFdoName(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++
Routine Description:

    NDIS.sys names its clients FDO in a specfic format.
    This routine does the same for a WDF Ndis Client's FDO.
    To determine the correct name to use, this routine calls into NDIS.sys
    to get a unique FDO name index and then goes ahead and creates a
    FDO name from it. It uses the same format that is used by ndis.sys.

--*/
{
    FuncEntry(FLAG_ADAPTER);

    ULONG          uFdoIndex;
    UNICODE_STRING fdoIndex;
    WCHAR          fdoIndexBuffer[20] = { 0 };
    UNICODE_STRING fdoName;
    WCHAR          fdoNameBuffer[30] = { 0 };
    NTSTATUS       status;

    //
    // Get a unique index from Ndis
    //
    uFdoIndex = NdisWdfGenerateFdoNameIndex();

    fdoIndex.Length = 0;
    fdoIndex.MaximumLength = sizeof(fdoIndexBuffer);
    fdoIndex.Buffer = fdoIndexBuffer;

    status = RtlIntegerToUnicodeString(uFdoIndex, 10, &fdoIndex);

    if (!NT_SUCCESS(status))
    {
        LogError(NULL, FLAG_ADAPTER,
            "RtlIntegerToUnicodeString failed for %d, %!STATUS!",
            uFdoIndex, status);

        goto Exit;
    }

    fdoName.Length = 0;
    fdoName.MaximumLength = sizeof(fdoNameBuffer);
    fdoName.Buffer = fdoNameBuffer;

    RtlCopyUnicodeString(&fdoName, &ndisFdoDeviceStr);

    status = RtlAppendUnicodeStringToString(&fdoName, &fdoIndex);

    if (!NT_SUCCESS(status))
    {
        LogError(NULL, FLAG_ADAPTER,
            "RtlAppendUnicodeStringToString failed to append %d \\Device\\NDMP, %!STATUS!",
            uFdoIndex, status);

        goto Exit;
    }

    status = WdfDeviceInitAssignName(DeviceInit, &fdoName);

    if (!NT_SUCCESS(status))
    {
        LogError(NULL, FLAG_ADAPTER,
            "WdfDeviceInitAssignName failed for \\Device\\NDMP%d, %!STATUS!",
            uFdoIndex, status);

        goto Exit;
    }

Exit:

    FuncExit(FLAG_ADAPTER);
    return status;
}

NTSTATUS
SetPreprocessorRoutines(
    _Inout_ PWDFCXDEVICE_INIT CxDeviceInit
    )
/*++
Routine Description:

    This routine sets the preprocessor routine for the IRPs that NetAdapterCx
    needs to directly forward to NDIS.

--*/
{
    FuncEntry(FLAG_ADAPTER);
    NTSTATUS       status;
    ULONG          i;

    // IRP_MJ codes for which we're going to register EvtWdmIrpPreprocessRoutine
    // as a preprocessor routine
    UCHAR irpCodes[] =
    {
        IRP_MJ_CREATE,
        IRP_MJ_CLOSE,
        IRP_MJ_DEVICE_CONTROL,
        IRP_MJ_INTERNAL_DEVICE_CONTROL,
        IRP_MJ_WRITE,
        IRP_MJ_READ,
        IRP_MJ_CLEANUP,
        IRP_MJ_SYSTEM_CONTROL
    };

    //
    // Set WDM irp preprocess callbacks
    //

    for (i = 0;i < ARRAYSIZE(irpCodes);i++)
    {
        status = WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
                                                    CxDeviceInit,
                                                    EvtWdmIrpPreprocessRoutine,
                                                    irpCodes[i],
                                                    NULL, // Pointer to the minor function table
                                                    0 // Number of entries in the table
                                                    );

        if (!NT_SUCCESS(status))
        {
            LogError(NULL, FLAG_ADAPTER,
                "WdfDeviceInitAssignWdmIrpPreprocessCallback failed for 0x%x, %!STATUS!", irpCodes[i], status);

            goto Exit;
        }
    }

    status = WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
                                                CxDeviceInit,
                                                EvtWdmPnpPowerIrpPreprocessRoutine,
                                                IRP_MJ_PNP,
                                                NULL, // Pointer to the minor function table
                                                0 // Number of entries in the table
                                                );
    if (!NT_SUCCESS(status)) {
        LogError(NULL, FLAG_ADAPTER,
            "WdfDeviceInitAssignWdmIrpPreprocessCallback failed for IRP_MJ_PNP %!STATUS!", status);

        goto Exit;
    }

    status = WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
                                                CxDeviceInit,
                                                EvtWdmPnpPowerIrpPreprocessRoutine,
                                                IRP_MJ_POWER,
                                                NULL, // Pointer to the minor function table
                                                0 // Number of entries in the table
                                                );

    if (!NT_SUCCESS(status)) {
        LogError(NULL, FLAG_ADAPTER,
            "WdfDeviceInitAssignWdmIrpPreprocessCallback failed for IRP_MJ_POWER %!STATUS!", status);

        goto Exit;
    }

Exit:

    FuncExit(FLAG_ADAPTER);
    return status;
}

VOID
SetCxPnpPowerCallbacks(
    _Inout_ PWDFCXDEVICE_INIT CxDeviceInit
    )
/*++
Routine Description:

    Register pre and post PnP and Power callbacks.

--*/
{
    FuncEntry(FLAG_ADAPTER);

    WDFCX_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDFCX_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

    //
    // EvtDevicePrepareHardware:
    //
    pnpCallbacks.EvtCxDevicePrePrepareHardware = NxDevice::_EvtCxDevicePrePrepareHardware;
    pnpCallbacks.EvtCxDevicePostPrepareHardware = NxDevice::_EvtCxDevicePostPrepareHardware;
    pnpCallbacks.EvtCxDevicePrePrepareHardwareFailedCleanup = NxDevice::_EvtCxDevicePrePrepareHardwareFailedCleanup;

    //
    // EvtDeviceD0Entry:
    //
    pnpCallbacks.EvtCxDevicePostD0Entry = NxDevice::_EvtCxDevicePostD0Entry;

    //
    // EvtDeviceD0Exit:
    //
    pnpCallbacks.EvtCxDevicePreD0Exit = NxDevice::_EvtCxDevicePreD0Exit;

    //
    // EvtDeviceReleaseHardware:
    //
    pnpCallbacks.EvtCxDevicePreReleaseHardware = NxDevice::_EvtCxDevicePreReleaseHardware;
    pnpCallbacks.EvtCxDevicePostReleaseHardware = NxDevice::_EvtCxDevicePostReleaseHardware;

    //
    // EvtDeviceSurpriseRemoval:
    //
    pnpCallbacks.EvtCxDevicePreSurpriseRemoval = NxDevice::_EvtCxDevicePreSurpriseRemoval;

    //
    // EvtDeviceSelfManagedIoInit:
    //
    pnpCallbacks.EvtCxDevicePostSelfManagedIoInit = NxDevice::_EvtCxDevicePostSelfManagedIoInit;

    //
    // EvtDeviceSelfManagedIoRestart:
    //
    pnpCallbacks.EvtCxDevicePostSelfManagedIoRestart = NxDevice::_EvtCxDevicePostSelfManagedIoRestart;

    //
    // EvtDeviceSelfManagedIoSuspend:
    //
    pnpCallbacks.EvtCxDevicePreSelfManagedIoSuspend = NxDevice::_EvtCxDevicePreSelfManagedIoSuspend;

    WdfCxDeviceInitSetPnpPowerEventCallbacks(CxDeviceInit, &pnpCallbacks);

    FuncExit(FLAG_ADAPTER);
}

WDFAPI
_IRQL_requires_max_(DISPATCH_LEVEL)
NETPOWERSETTINGS
NETEXPORT(NetAdapterGetPowerSettings)(
    _In_ PNET_DRIVER_GLOBALS    Globals,
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
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter               nxAdapter;
    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;

    pNxPrivateGlobals = GetPrivateGlobals(Globals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);

    nxAdapter = GetNxAdapterFromHandle(Adapter);

    FuncExit(FLAG_ADAPTER);
    return nxAdapter->m_NxWake->GetFxObject();
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterRegisterPacketExtension)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const PNET_PACKET_EXTENSION ExtensionToRegister
    )
{
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter               nxAdapter;
    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;

    pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, ExtensionToRegister);
    Verifier_VerifyNetPacketExtension(pNxPrivateGlobals, ExtensionToRegister);

    nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyEvtAdapterSetCapabilitiesInProgress(pNxPrivateGlobals, nxAdapter);

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
    FuncExit(FLAG_ADAPTER);

    return nxAdapter->RegisterPacketExtension(&extensionPrivate);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterQueryRegisteredPacketExtension)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const PNET_PACKET_EXTENSION_QUERY ExtensionToQuery
    )
{
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter               nxAdapter;
    PNX_PRIVATE_GLOBALS      pNxPrivateGlobals;

    pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, ExtensionToQuery);
    Verifier_VerifyNetPacketExtensionQuery(pNxPrivateGlobals, ExtensionToQuery);

    nxAdapter = GetNxAdapterFromHandle(Adapter);

    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = ExtensionToQuery->Name;
    extensionPrivate.Version = ExtensionToQuery->Version;

    FuncExit(FLAG_ADAPTER);

    return nxAdapter->QueryRegisteredPacketExtension(&extensionPrivate);
}

} // extern "C"
