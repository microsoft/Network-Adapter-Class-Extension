// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxDriver.hpp"
#include "NxAdapter.hpp"
#include "NxConfiguration.hpp"
#include "NxDevice.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include "NxAdapterApi.tmh"

extern
WDFWAITLOCK
    g_RegistrationLock;

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
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, Device);

    auto adapterInit = wil::make_unique_nothrow<AdapterInit>();

    if (!adapterInit)
    {
        return nullptr;
    }

    if (! adapterInit->TxDemux.reserve(3))
    {
        return nullptr;
    }

    adapterInit->ParentDevice = Device;
    adapterInit->ExtensionType = MediaExtensionTypeFromClientGlobals(nxPrivateGlobals->ClientDriverGlobals);

    // Create the object now, but hold creation of NxAdapter context until NetAdapterCreate is called
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    auto const ntStatus = WdfObjectCreate(
        &attributes,
        &adapterInit->Object);

    CX_LOG_IF_NOT_NT_SUCCESS_MSG(ntStatus, "Failed to create NETADAPTER object");

    if (ntStatus != STATUS_SUCCESS)
    {
        return nullptr;
    }

    return reinterpret_cast<NETADAPTER_INIT *>(adapterInit.release());
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
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
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, AdapterInit);

    auto adapterInit = GetAdapterInitFromHandle(nxPrivateGlobals, AdapterInit);

    adapterInit->~AdapterInit();
    ExFreePool(adapterInit);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
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
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, DatapathCallbacks);
    Verifier_VerifyDatapathCallbacks(nxPrivateGlobals, DatapathCallbacks);

    auto adapterInit = GetAdapterInitFromHandle(nxPrivateGlobals, AdapterInit);

    RtlCopyMemory(
        &adapterInit->DatapathCallbacks,
        DatapathCallbacks,
        DatapathCallbacks->Size);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterCreate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER_INIT * AdapterInit,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * AdapterAttributes,
    _Out_ NETADAPTER * Adapter
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

    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto adapterInit = GetAdapterInitFromHandle(nxPrivateGlobals, AdapterInit);

    Verifier_VerifyAdapterInitNotUsed(nxPrivateGlobals, adapterInit);

    auto device = adapterInit->ParentDevice;
    auto nxDevice = GetNxDeviceFromHandle(device);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        nxDevice->EnsureInitialized(device),
        "Failed to initialize NxDevice. WDFDEVICE=%p", device);

    // Client is not allowed to specify a parent for a NETADAPTER object
    Verifier_VerifyObjectAttributesParentIsNull(nxPrivateGlobals, AdapterAttributes);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxAdapter::_Create(
            nxPrivateGlobals,
            adapterInit),
        "Failed to initialize NETADAPTER. NETADAPTER=%p",
        adapterInit->Object.get());

    auto nxAdapter = GetNxAdapterFromHandle(adapterInit->Object.get());

    if (AdapterAttributes != WDF_NO_OBJECT_ATTRIBUTES)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfObjectAllocateContext(
                nxAdapter->GetFxObject(),
                AdapterAttributes,
                nullptr),
            "WdfObjectAllocateContext for ClientAttributes. NETADAPTER=%p",
            nxAdapter->GetFxObject());
    }

    //
    // Don't fail after this point or else the client's Cleanup / Destroy
    // callbacks can get called.
    //

    adapterInit->CreatedAdapter = nxAdapter->GetFxObject();

    //
    // Object is created and initialized, move ownership of the handle from
    // AdapterInit to the client driver
    //

    *Adapter = reinterpret_cast<NETADAPTER>(adapterInit->Object.release());

    CX_RETURN_STATUS_SUCCESS_MSG(
        "NETADAPTER %p",
        *Adapter);
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
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    NxAdapter *adapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, adapter);

    return adapter->PnpPower.NetAdapterStart(nxPrivateGlobals);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetAdapterStop)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    NxAdapter *adapter = GetNxAdapterFromHandle(Adapter);
    adapter->PnpPower.NetAdapterStop(nxPrivateGlobals);
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
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

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
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

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
    _Out_    NETCONFIGURATION *                  Configuration
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

    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    *Configuration = NULL;

    NxConfiguration * nxConfiguration;
    status = NxConfiguration::_Create<NETADAPTER>(nxPrivateGlobals,
                                      Adapter,
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
void
NETEXPORT(NetAdapterSetDataPathCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_TX_CAPABILITIES * TxCapabilities,
    _In_ NET_ADAPTER_RX_CAPABILITIES * RxCapabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    Verifier_VerifyTypeSize(nxPrivateGlobals, TxCapabilities);
    Verifier_VerifyTypeSize(nxPrivateGlobals, RxCapabilities);

    Verifier_VerifyNetAdapterTxCapabilities(nxPrivateGlobals, TxCapabilities);
    Verifier_VerifyNetAdapterRxCapabilities(nxPrivateGlobals, RxCapabilities);

    if (TxCapabilities->DmaCapabilities != nullptr)
    {
        Verifier_VerifyTypeSize(nxPrivateGlobals, TxCapabilities->DmaCapabilities);
    }

    if (RxCapabilities->DmaCapabilities != nullptr)
    {
        Verifier_VerifyTypeSize(nxPrivateGlobals, RxCapabilities->DmaCapabilities);
    }

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->SetDatapathCapabilities(
        TxCapabilities,
        RxCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterSetReceiveScalingCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, Capabilities);
    Verifier_VerifyReceiveScalingCapabilities(nxPrivateGlobals, Capabilities);

    auto const nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->RxScaling.SetCapabilities(*Capabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
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
    void

Remarks:

    If called during execution of EvtAdapterSetCapabilities, no MTU change is indicated
    to NDIS.

    If called after EvtAdapterSetCapabilities, NDIS will receive a status indication
    of the change and all of the adapter's Tx/Rx queues will be destroyed then reinitialized.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyMtuSize(nxPrivateGlobals, MtuSize);

    auto const nxAdapter = GetNxAdapterFromHandle(Adapter);

    nxAdapter->SetMtuSize(MtuSize);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
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
    void

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
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, LinkLayerCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->SetLinkLayerCapabilities(*LinkLayerCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NETEXPORT(NetAdapterSetReceiveFilterCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_RECEIVE_FILTER_CAPABILITIES * ReceiveFilter
)
{
    auto nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, ReceiveFilter);

    Verifier_VerifyReceiveFilterCapabilities(nxPrivateGlobals, ReceiveFilter);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->SetReceiveFilterCapabilities(*ReceiveFilter);
}

WDFAPI
_IRQL_requires_max_(DISPATCH_LEVEL)
NET_PACKET_FILTER_FLAGS
NETEXPORT(NetReceiveFilterGetPacketFilter)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETRECEIVEFILTER ReceiveFilter
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const receiveFilter = reinterpret_cast<NET_RECEIVE_FILTER const *>(ReceiveFilter);

    Verifier_VerifyReceiveFilterHandle(nxPrivateGlobals, receiveFilter);

    return receiveFilter->PacketFilter;
}

WDFAPI
_IRQL_requires_max_(DISPATCH_LEVEL)
SIZE_T
NETEXPORT(NetReceiveFilterGetMulticastAddressCount)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETRECEIVEFILTER ReceiveFilter
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const receiveFilter = reinterpret_cast<NET_RECEIVE_FILTER const *>(ReceiveFilter);

    Verifier_VerifyReceiveFilterHandle(nxPrivateGlobals, receiveFilter);

    return receiveFilter->MulticastAddressCount;
}

WDFAPI
_IRQL_requires_max_(DISPATCH_LEVEL)
NET_ADAPTER_LINK_LAYER_ADDRESS const *
NETEXPORT(NetReceiveFilterGetMulticastAddressList)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETRECEIVEFILTER ReceiveFilter
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const receiveFilter = reinterpret_cast<NET_RECEIVE_FILTER const *>(ReceiveFilter);

    Verifier_VerifyReceiveFilterHandle(nxPrivateGlobals, receiveFilter);

    return reinterpret_cast<NET_ADAPTER_LINK_LAYER_ADDRESS const *>(
        receiveFilter->MulticastAddressList);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
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

    void

Remarks:

    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

    Subsequently when the NetAdapter driver returns from its
    EvtAdapterSetCapabilities, NetAdapterCx would fail the
    EvtDevicePrepareHardware.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);
    Verifier_VerifyLinkLayerAddress(nxPrivateGlobals, LinkLayerAddress);

    nxAdapter->SetPermanentLinkLayerAddress(LinkLayerAddress);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
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

    void

Remarks:

    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

    Subsequently when the NetAdapter driver returns from its
    EvtAdapterSetCapabilities, NetAdapterCx would fail the
    EvtDevicePrepareHardware.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyLinkLayerAddress(nxPrivateGlobals, LinkLayerAddress);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    nxAdapter->SetCurrentLinkLayerAddress(LinkLayerAddress);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetAdapterPowerOffloadSetArpCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_POWER_OFFLOAD_ARP_CAPABILITIES * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->SetPowerOffloadArpCapabilities(Capabilities);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetAdapterPowerOffloadSetNSCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_POWER_OFFLOAD_NS_CAPABILITIES * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->SetPowerOffloadNSCapabilities(Capabilities);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterWakeSetBitmapCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_WAKE_BITMAP_CAPABILITIES * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->SetWakeBitmapCapabilities(Capabilities);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterWakeSetMediaChangeCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_WAKE_MEDIA_CHANGE_CAPABILITIES * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->SetWakeMediaChangeCapabilities(Capabilities);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterWakeSetMagicPacketCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_WAKE_MAGIC_PACKET_CAPABILITIES * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->SetMagicPacketCapabilities(Capabilities);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterWakeSetEapolPacketCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_WAKE_EAPOL_PACKET_CAPABILITIES * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->SetEapolPacketCapabilities(Capabilities);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterWakeSetPacketFilterCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_WAKE_PACKET_FILTER_CAPABILITIES * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->SetWakePacketFilterCapabilities(Capabilities);
}

WDFAPI
_IRQL_requires_max_(DISPATCH_LEVEL)
void
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
    void

Remarks:

    An error in this routine would result in capabilities not getting
    set. And a failure is explicitly reported to the PNP
    and the device will be transitioned to a failed state.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);
    Verifier_VerifyTypeSize<NET_ADAPTER_LINK_STATE>(nxPrivateGlobals, State);
    Verifier_VerifyCurrentLinkState(nxPrivateGlobals, State);

    GetNxAdapterFromHandle(Adapter)->SetCurrentLinkState(*State);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
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
    void
--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const driverVersion = MAKEVER(nxPrivateGlobals->ClientVersion.Major,
        nxPrivateGlobals->ClientVersion.Minor);

    if (driverVersion > MAKEVER(2,0))
    {
        if (KdRefreshDebuggerNotPresent())
        {
            Verifier_ReportViolation(
                nxPrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_DeprecatedFunction,
                static_cast<ULONG_PTR>(driverVersion),
                NetAdapterOffloadSetChecksumCapabilitiesTableIndex);
        }
        else
        {
            DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                "\n\n"
                "************************************************************\n"
                "* NetAdapterOffloadSetChecksumCapabilities has been         \n"
                "* deprecated. Use NetAdapterOffloadSetTxChecksumCapabilities\n"
                "* and NetAdapterOffloadSetRxChecksumCapabilities instead.   \n"
                "************************************************************\n"
                "\n");

            return;
        }
    }

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, HardwareCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->m_offloadManager->SetChecksumHardwareCapabilities(HardwareCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsChecksumIPv4Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const driverVersion = MAKEVER(nxPrivateGlobals->ClientVersion.Major,
        nxPrivateGlobals->ClientVersion.Minor);

    if (driverVersion > MAKEVER(2,0))
    {
        if (KdRefreshDebuggerNotPresent())
        {
            Verifier_ReportViolation(
                nxPrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_DeprecatedFunction,
                static_cast<ULONG_PTR>(driverVersion),
                NetOffloadIsChecksumIPv4EnabledTableIndex);
        }
        else
        {
            DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                "\n\n"
                "**********************************************************\n"
                "* NetOffloadIsChecksumIPv4Enabled has been deprecated.    \n"
                "* Use NetOffloadIsTxChecksumIPv4Enabled and               \n"
                "* NetOffloadIsRxChecksumIPv4Enabled instead.              \n"
                "**********************************************************\n"
                "\n");

            return FALSE;
        }
    }

    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

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
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const driverVersion = MAKEVER(nxPrivateGlobals->ClientVersion.Major,
        nxPrivateGlobals->ClientVersion.Minor);

    if (driverVersion > MAKEVER(2,0))
    {
        if (KdRefreshDebuggerNotPresent())
        {
            Verifier_ReportViolation(
                nxPrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_DeprecatedFunction,
                static_cast<ULONG_PTR>(driverVersion),
                NetOffloadIsChecksumTcpEnabledTableIndex);
        }
        else
        {
            DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                "\n\n"
                "**********************************************************\n"
                "* NetOffloadIsChecksumTcpEnabled has been deprecated.     \n"
                "* Use NetOffloadIsTxChecksumTcpEnabled and                \n"
                "* NetOffloadIsRxChecksumTcpEnabled instead.               \n"
                "**********************************************************\n"
                "\n");

            return FALSE;
        }
    }

    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

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
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const driverVersion = MAKEVER(nxPrivateGlobals->ClientVersion.Major,
        nxPrivateGlobals->ClientVersion.Minor);

    if (driverVersion > MAKEVER(2,0))
    {
        if (KdRefreshDebuggerNotPresent())
        {
            Verifier_ReportViolation(
                nxPrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_DeprecatedFunction,
                static_cast<ULONG_PTR>(driverVersion),
                NetOffloadIsChecksumUdpEnabledTableIndex);
        }
        else
        {
            DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                "\n\n"
                "**********************************************************\n"
                "* NetOffloadIsChecksumUdpEnabled has been deprecated.     \n"
                "* Use NetOffloadIsTxChecksumUdpEnabled and                \n"
                "* NetOffloadIsRxChecksumUdpEnabled instead.               \n"
                "**********************************************************\n"
                "\n");

            return FALSE;
        }
    }

    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

    return capabilities->Udp;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterOffloadSetTxChecksumCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES * HardwareCapabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, HardwareCapabilities);
    Verifier_VerifyTxChecksumCapabilities(nxPrivateGlobals, HardwareCapabilities);
    Verifier_VerifyTxChecksumHardwareCapabilities(nxPrivateGlobals, HardwareCapabilities);
    Verifier_VerifyTxChecksumIPv6Flags(nxPrivateGlobals, HardwareCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->m_offloadManager->SetTxChecksumHardwareCapabilities(HardwareCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterOffloadSetRxChecksumCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_OFFLOAD_RX_CHECKSUM_CAPABILITIES * Capabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->m_offloadManager->SetRxChecksumCapabilities(Capabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsTxChecksumIPv4Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_OFFLOAD_TX_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, &capabilities->IPv4Capabilities);
    Verifier_VerifyTxChecksumCapabilities(nxPrivateGlobals, &capabilities->IPv4Capabilities);

    return WI_IsFlagSet(capabilities->IPv4Capabilities.Layer3Flags, NetAdapterOffloadLayer3FlagIPv4NoOptions);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsTxChecksumTcpEnabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_OFFLOAD_TX_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, &capabilities->TcpCapabilities);
    Verifier_VerifyTxChecksumCapabilities(nxPrivateGlobals, &capabilities->TcpCapabilities);
    Verifier_VerifyTxChecksumLayer4Capabilities(nxPrivateGlobals, &capabilities->TcpCapabilities);

    return WI_IsFlagSet(capabilities->TcpCapabilities.Layer4Flags, NetAdapterOffloadLayer4FlagTcpNoOptions);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsTxChecksumUdpEnabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_OFFLOAD_TX_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, &capabilities->UdpCapabilities);
    Verifier_VerifyTxChecksumCapabilities(nxPrivateGlobals, &capabilities->UdpCapabilities);
    Verifier_VerifyTxChecksumLayer4Capabilities(nxPrivateGlobals, &capabilities->UdpCapabilities);

    return WI_IsFlagSet(capabilities->UdpCapabilities.Layer4Flags, NetAdapterOffloadLayer4FlagUdp);
}


WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsRxChecksumIPv4Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

    return capabilities->IPv4;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsRxChecksumTcpEnabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

    return capabilities->Tcp;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsRxChecksumUdpEnabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

    return capabilities->Udp;
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterOffloadSetGsoCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES * HardwareCapabilities
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, HardwareCapabilities);
    Verifier_VerifyGsoCapabilities(nxPrivateGlobals, HardwareCapabilities);
    Verifier_VerifyGsoHardwareCapabilities(nxPrivateGlobals, HardwareCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->m_offloadManager->SetGsoHardwareCapabilities(HardwareCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsLsoIPv4Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_OFFLOAD_GSO_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, &capabilities->LsoCapabilities);
    Verifier_VerifyGsoCapabilities(nxPrivateGlobals, &capabilities->LsoCapabilities);

    return WI_IsFlagSet(capabilities->LsoCapabilities.Layer3Flags, NetAdapterOffloadLayer3FlagIPv4NoOptions) &&
        WI_IsFlagSet(capabilities->LsoCapabilities.Layer4Flags, NetAdapterOffloadLayer4FlagTcpNoOptions);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsLsoIPv6Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_OFFLOAD_GSO_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, &capabilities->LsoCapabilities);
    Verifier_VerifyGsoCapabilities(nxPrivateGlobals, &capabilities->LsoCapabilities);

    return WI_IsFlagSet(capabilities->LsoCapabilities.Layer3Flags, NetAdapterOffloadLayer3FlagIPv6NoExtensions) &&
        WI_IsFlagSet(capabilities->LsoCapabilities.Layer4Flags, NetAdapterOffloadLayer4FlagTcpNoOptions);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsUsoIPv4Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_OFFLOAD_GSO_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, &capabilities->UsoCapabilities);
    Verifier_VerifyGsoCapabilities(nxPrivateGlobals, &capabilities->UsoCapabilities);

    return WI_IsFlagSet(capabilities->UsoCapabilities.Layer3Flags, NetAdapterOffloadLayer3FlagIPv4NoOptions) &&
        WI_IsFlagSet(capabilities->UsoCapabilities.Layer4Flags, NetAdapterOffloadLayer4FlagUdp);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsUsoIPv6Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_OFFLOAD_GSO_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, &capabilities->UsoCapabilities);
    Verifier_VerifyGsoCapabilities(nxPrivateGlobals, &capabilities->UsoCapabilities);

    return WI_IsFlagSet(capabilities->UsoCapabilities.Layer3Flags, NetAdapterOffloadLayer3FlagIPv6NoExtensions) &&
        WI_IsFlagSet(capabilities->UsoCapabilities.Layer4Flags, NetAdapterOffloadLayer4FlagUdp);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterOffloadSetRscCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES * HardwareCapabilities
)
/*++
Routine Description:

    This routine sets the RSC hardware capabilities of the Network Adapter and provides
    a callback to notify active RSC capabilities to the client driver.

    The client driver must call this method before calling NetAdapterStart

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    HardwareCapabilities - Pointer to a initialized NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES
    structure representing the hardware capabilities of network adapter

Returns:
    void
--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, HardwareCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->m_offloadManager->SetRscHardwareCapabilities(HardwareCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsTcpRscIPv4Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

    return WI_IsFlagSet(capabilities->Layer3Flags, NetAdapterOffloadLayer3FlagIPv4NoOptions) &&
        WI_IsFlagSet(capabilities->Layer4Flags, NetAdapterOffloadLayer4FlagTcpNoOptions);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsTcpRscIPv6Enabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

    return WI_IsFlagSet(capabilities->Layer3Flags, NetAdapterOffloadLayer3FlagIPv6NoExtensions) &&
        WI_IsFlagSet(capabilities->Layer4Flags, NetAdapterOffloadLayer4FlagTcpNoOptions);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetOffloadIsRscTcpTimestampOptionEnabled)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETOFFLOAD Offload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const capabilities = reinterpret_cast<NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES const *>(Offload);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, capabilities);

    return capabilities->TcpTimestampOption;
}

_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterReportWakeReasonPacket)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ const NET_ADAPTER_WAKE_REASON_PACKET * Reason
    )
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->ReportWakeReasonPacket(Reason);
}

_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterReportWakeReasonMediaChange)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_IF_MEDIA_CONNECT_STATE Reason
    )
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->ReportWakeReasonMediaChange(Reason);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterWifiDestroyPeerAddressDatapath)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ SIZE_T Demux
    )
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);

    GetNxAdapterFromHandle(Adapter)->WifiDestroyPeerAddressDatapath(Demux);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterInitAddTxDemux)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER_INIT * AdapterInit,
    _In_ NET_ADAPTER_TX_DEMUX const * Demux
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, AdapterInit);
    Verifier_VerifyTxDemux(nxPrivateGlobals, Demux);

    auto adapterInit = GetAdapterInitFromHandle(nxPrivateGlobals, AdapterInit);
    NT_FRE_ASSERT(adapterInit->TxDemux.append(*Demux));
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
NET_ADAPTER_TX_DEMUX const *
NETEXPORT(NetAdapterGetTxPeerAddressDemux)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    return GetNxAdapterFromHandle(Adapter)->GetTxPeerAddressDemux();
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterOffloadSetIeee8021qTagCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NET_ADAPTER_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES * HardwareCapabilities
)
/*++
Routine Description:

    This routine sets the IEEE 802.1 priority and vlan tagging hardware capabilities of the Network Adapter
    and provides a callback to notify active priority and vlan tagging capabilities to the client driver.

    The client driver must call this method before calling NetAdapterStart

Arguments:

    Adapter - Pointer to the Adapter created in a prior call to NetAdapterCreate

    HardwareCapabilities - Pointer to a initialized NET_ADAPTER_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES
    structure representing the hardware capabilities of network adapter

Returns:
    void
--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, HardwareCapabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->SetIeee8021qTagCapabiilities(*HardwareCapabilities);
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterPauseOffloadCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterStarted(nxPrivateGlobals, nxAdapter);

    nxAdapter->PauseOffloads();
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterResumeOffloadCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterStarted(nxPrivateGlobals, nxAdapter);
    Verifier_VerifyOffloadPaused(nxPrivateGlobals, nxAdapter);

    nxAdapter->ResumeOffloads();
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterInitSetSelfManagedPowerReferences)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER_INIT * AdapterInit,
    BOOLEAN SelfManagedPowerReference
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, AdapterInit);

    auto adapterInit = GetAdapterInitFromHandle(nxPrivateGlobals, AdapterInit);

    // If the adapter has indicated that it is self-managing power references, we should not acquire
    // any power references on its behalf
    adapterInit->ShouldAcquirePowerReferencesForAdapter = !SelfManagedPowerReference;
}
