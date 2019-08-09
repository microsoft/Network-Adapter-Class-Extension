// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:
    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include <nblutil.h>
#include <NetClientApi.h>
#include <NetClientBufferImpl.h>
#include <NetClientDriverConfigurationImpl.hpp>

#include <net/logicaladdresstypes_p.h>
#include <net/mdltypes_p.h>
#include <net/returncontexttypes_p.h>
#include <net/virtualaddresstypes_p.h>

#include "NxAdapter.tmh"
#include "NxAdapter.hpp"

#include "NxDevice.hpp"
#include "NxDriver.hpp"
#include "NxMacros.hpp"
#include "NxExtension.hpp"
#include "NxQueue.hpp"
#include "NxRequest.hpp"
#include "NxRequestQueue.hpp"
#include "NxUtility.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "verifier.hpp"
#include "version.hpp"

//
// Standard NDIS callback declaration
//

static
MINIPORT_INITIALIZE
    EvtNdisInitializeEx;

static
MINIPORT_HALT
    EvtNdisHaltEx;

static
MINIPORT_PAUSE
    EvtNdisPause;

static
MINIPORT_RESTART
    EvtNdisRestart;

static
MINIPORT_SEND_NET_BUFFER_LISTS
    EvtNdisSendNetBufferLists;

static
MINIPORT_RETURN_NET_BUFFER_LISTS
    EvtNdisReturnNetBufferLists;

static
MINIPORT_CANCEL_SEND
    EvtNdisCancelSend;

static
MINIPORT_DEVICE_PNP_EVENT_NOTIFY
    EvtNdisDevicePnpEventNotify;

static
MINIPORT_SHUTDOWN
    EvtNdisShutdownEx;

static
MINIPORT_OID_REQUEST
    EvtNdisOidRequest;

static
MINIPORT_DIRECT_OID_REQUEST
    EvtNdisDirectOidRequest;

static
MINIPORT_CANCEL_OID_REQUEST
    EvtNdisCancelOidRequest;

static
MINIPORT_CANCEL_DIRECT_OID_REQUEST
    EvtNdisCancelDirectOidRequest;

static
MINIPORT_SYNCHRONOUS_OID_REQUEST
    EvtNdisSynchronousOidRequestHandler;

//
// NDIS-WDF callback declaration
//

static
EVT_NDIS_WDF_MINIPORT_GET_NDIS_HANDLE_FROM_DEVICE_OBJECT
    EvtNdisWdmDeviceGetNdisAdapterHandle;

static
EVT_NDIS_WDF_MINIPORT_GET_DEVICE_OBJECT
    EvtNdisGetDeviceObject;

static
EVT_NDIS_WDF_MINIPORT_GET_NEXT_DEVICE_OBJECT
    EvtNdisGetNextDeviceObject;

static
EVT_NDIS_WDF_MINIPORT_GET_ASSIGNED_FDO_NAME
    EvtNdisGetAssignedFdoName;

static
EVT_NDIS_WDF_MINIPORT_POWER_REFERENCE
    EvtNdisPowerReference;

static
EVT_NDIS_WDF_MINIPORT_POWER_DEREFERENCE
    EvtNdisPowerDereference;

static
EVT_NDIS_WDF_MINIPORT_AOAC_ENGAGE
    EvtNdisAoAcEngage;

static
EVT_NDIS_WDF_MINIPORT_AOAC_DISENGAGE
    EvtNdisAoAcDisengage;

static
EVT_NDIS_WDF_MINIPORT_UPDATE_PM_PARAMETERS
    EvtNdisUpdatePMParameters;

static
EVT_NDIS_WDF_MINIPORT_ALLOCATE_BLOCK
    EvtNdisAllocateMiniportBlock;

static
EVT_NDIS_WDF_MINIPORT_COMPLETE_ADD
    EvtNdisMiniportCompleteAdd;

static
EVT_NDIS_WDF_DEVICE_START_COMPLETE
    EvtNdisDeviceStartComplete;

static
EVT_NDIS_WDF_MINIPORT_DEVICE_RESET
    EvtNdisMiniportDeviceReset;

static
EVT_NDIS_WDF_MINIPORT_QUERY_DEVICE_RESET_SUPPORT
    EvtNdisMiniportQueryDeviceResetSupport;

static
EVT_NDIS_WDF_GET_WMI_EVENT_GUID
    EvtNdisGetWmiEventGuid;

static const NET_CLIENT_DISPATCH CxDispatch =
{
    sizeof(NET_CLIENT_DISPATCH),
    &NetClientCreateBufferPool,
    &NetClientQueryDriverConfigurationUlong,
    &NetClientQueryDriverConfigurationBoolean,
};

static
void
NetClientAdapterSetDeviceFailed(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NTSTATUS Status
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->SetDeviceFailed(Status);
}

static
void
NetClientAdapterGetProperties(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Out_ NET_CLIENT_ADAPTER_PROPERTIES * Properties
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->GetProperties(Properties);
}

static
void
NetClientAdapterGetDatapathCapabilities(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Out_ NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES * Capabilities
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->GetDatapathCapabilities(Capabilities);
}

static
void
NetClientAdapterGetReceiveScalingCapabilities(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Out_ NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES * Capabilities
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->GetReceiveScalingCapabilities(Capabilities);
}

static
NTSTATUS
NetClientAdapterCreateTxQueue(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ void * ClientContext,
    _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    _Out_ NET_CLIENT_QUEUE * AdapterQueue,
    _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->CreateTxQueue(
        ClientContext,
        ClientDispatch,
        ClientQueueConfig,
        AdapterQueue,
        AdapterDispatch);
}

static
NTSTATUS
NetClientAdapterCreateRxQueue(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ void * ClientContext,
    _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    _Out_ NET_CLIENT_QUEUE * AdapterQueue,
    _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->CreateRxQueue(
        ClientContext,
        ClientDispatch,
        ClientQueueConfig,
        AdapterQueue,
        AdapterDispatch);
}

static
void
NetClientAdapterDestroyQueue(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_QUEUE Queue
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->DestroyQueue(Queue);
}

NONPAGEDX
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
static
void
NetClientReturnRxBuffer(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_FRAGMENT_RETURN_CONTEXT_HANDLE RxBufferReturnContext
)
{
    reinterpret_cast<NxAdapter*>(Adapter)->ReturnRxBuffer(RxBufferReturnContext);
}

static
NTSTATUS
NetClientAdapterRegisterExtension(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_EXTENSION const * Extension
)
{
    NET_EXTENSION_PRIVATE extenstionPrivate = {};
    extenstionPrivate.Name = Extension->Name;
    extenstionPrivate.Version = Extension->Version;
    extenstionPrivate.Size = Extension->ExtensionSize;
    extenstionPrivate.NonWdfStyleAlignment = Extension->Alignment;
    extenstionPrivate.Type = Extension->Type;

    return reinterpret_cast<NxAdapter *>(Adapter)->RegisterExtension(extenstionPrivate);
}

static
NTSTATUS
NetClientAdapterQueryExtension(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_EXTENSION const * Extension
)
{
    auto const extension = reinterpret_cast<NxAdapter *>(Adapter)->QueryExtensionLocked(
        Extension->Name,
        Extension->Version,
        Extension->Type);

    return extension != nullptr ?  STATUS_SUCCESS : STATUS_NOT_FOUND;
}

static
NTSTATUS
NetClientAdapterReceiveScalingEnable(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ ULONG HashFunction,
    _In_ ULONG HashType
)
{
    // translate ndis -> net adapter
    NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE hashType = NetAdapterReceiveScalingHashTypeNone;
    NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE protocolType = NetAdapterReceiveScalingProtocolTypeNone;

    if (HashFunction & NdisHashFunctionToeplitz)
    {
        hashType |= NetAdapterReceiveScalingHashTypeToeplitz;
    }

    if (HashType & NDIS_HASH_IPV4)
    {
        protocolType |= NetAdapterReceiveScalingProtocolTypeIPv4;
    }

    if (HashType & NDIS_HASH_IPV6)
    {
        protocolType |= NetAdapterReceiveScalingProtocolTypeIPv6;
    }

    if (HashType & NDIS_HASH_IPV6_EX)
    {
        protocolType |= NetAdapterReceiveScalingProtocolTypeIPv6Extensions;
    }

    auto const protocolTypeTcp = NDIS_HASH_TCP_IPV4 | NDIS_HASH_TCP_IPV6 | NDIS_HASH_TCP_IPV6_EX;
    if (HashType & protocolTypeTcp)
    {
        protocolType |= NetAdapterReceiveScalingProtocolTypeTcp;
    }

    return reinterpret_cast<NxAdapter *>(Adapter)->ReceiveScalingEnable(hashType, protocolType);
}

static
void
NetClientAdapterReceiveScalingDisable(
    _In_ NET_CLIENT_ADAPTER Adapter
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->ReceiveScalingDisable();
}

static
NTSTATUS
NetClientAdapterReceiveScalingSetIndirectionEntries(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Inout_ NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES * IndirectionEntries
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->ReceiveScalingSetIndirectionEntries(IndirectionEntries);
}

static
NTSTATUS
NetClientAdapterReceiveScalingSetHashSecretKey(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const * HashSecretKey
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->ReceiveScalingSetHashSecretKey(HashSecretKey);
}

static
void
NetClientAdapterGetChecksumHardwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * HardwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->GetChecksumHardwareCapabilities(HardwareCapabilities);
}

static
void
NetClientAdapterGetChecksumDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->GetChecksumDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterSetChecksumActiveCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const * ActiveCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->SetChecksumActiveCapabilities(ActiveCapabilities);
}

static
void
NetClientAdapterGetLsoHardwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_LSO_CAPABILITIES * HardwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->GetLsoHardwareCapabilities(HardwareCapabilities);
}

static
void
NetClientAdapterGetLsoDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_LSO_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->GetLsoDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterSetLsoActiveCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_OFFLOAD_LSO_CAPABILITIES const * ActiveCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->SetLsoActiveCapabilities(ActiveCapabilities);
}

static
void
NetClientAdapterGetRscHardwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * HardwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->GetRscHardwareCapabilities(HardwareCapabilities);
}

static
void
NetClientAdapterGetRscDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->GetRscDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterSetMulticastList(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ ULONG MulticastAddressCount,
    _In_ IF_PHYSICAL_ADDRESS * MulticastAddressList
)
{
    auto const adapter = reinterpret_cast<NxAdapter *>(ClientAdapter);

    adapter->m_MulticastList.EvtSetMulticastList(
        adapter->GetFxObject(),
        MulticastAddressCount,
        reinterpret_cast<NET_ADAPTER_LINK_LAYER_ADDRESS *>(MulticastAddressList));
}

static
VOID
NetClientAdapterSetRscActiveCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const * ActiveCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_NxOffloadManager->SetRscActiveCapabilities(ActiveCapabilities);
}

static
NTSTATUS
NetClientAdapterSetPacketFilter(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ ULONG PacketFilter
)
{
    auto const adapter = reinterpret_cast<NxAdapter *>(ClientAdapter);

    if (PacketFilter & ~(adapter->m_PacketFilter.SupportedPacketFilters))
    {
        return STATUS_NOT_SUPPORTED;
    }

    adapter->m_PacketFilter.EvtSetPacketFilter(
        adapter->GetFxObject(),
        static_cast<NET_PACKET_FILTER_FLAGS>(PacketFilter));

    return STATUS_SUCCESS;
}

static const NET_CLIENT_ADAPTER_DISPATCH AdapterDispatch =
{
    sizeof(NET_CLIENT_ADAPTER_DISPATCH),
    &NetClientAdapterSetDeviceFailed,
    &NetClientAdapterGetProperties,
    &NetClientAdapterGetDatapathCapabilities,
    &NetClientAdapterGetReceiveScalingCapabilities,
    &NetClientAdapterCreateTxQueue,
    &NetClientAdapterCreateRxQueue,
    &NetClientAdapterDestroyQueue,
    &NetClientReturnRxBuffer,
    &NetClientAdapterRegisterExtension,
    &NetClientAdapterQueryExtension,
    &NetClientAdapterSetPacketFilter,
    &NetClientAdapterSetMulticastList,
    {
        &NetClientAdapterReceiveScalingEnable,
        &NetClientAdapterReceiveScalingDisable,
        &NetClientAdapterReceiveScalingSetIndirectionEntries,
        &NetClientAdapterReceiveScalingSetHashSecretKey,
    },
    {
        &NetClientAdapterGetChecksumHardwareCapabilities,
        &NetClientAdapterGetChecksumDefaultCapabilities,
        &NetClientAdapterSetChecksumActiveCapabilities,
        &NetClientAdapterGetLsoHardwareCapabilities,
        &NetClientAdapterGetLsoDefaultCapabilities,
        &NetClientAdapterSetLsoActiveCapabilities,
        &NetClientAdapterGetRscHardwareCapabilities,
        &NetClientAdapterGetRscDefaultCapabilities,
        &NetClientAdapterSetRscActiveCapabilities,
    }
};

static
NTSTATUS
AllocateQueueId(
    _Inout_ Rtl::KArray<wil::unique_wdf_object> & Queues,
    _Out_ ULONG & QueueId
)
{
    for (size_t i = 0; i < Queues.count(); i++)
    {
        if (! Queues[i])
        {
            QueueId = static_cast<ULONG>(i);
            return STATUS_SUCCESS;
        }
    }

    ULONG queueId = static_cast<ULONG>(Queues.count());
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! Queues.resize(Queues.count() + 1));

    QueueId = queueId;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
AdapterInit *
GetAdapterInitFromHandle(
    NX_PRIVATE_GLOBALS *PrivateGlobals,
    NETADAPTER_INIT * Handle
)
{
    auto adapterInit = reinterpret_cast<AdapterInit *>(Handle);

    Verifier_VerifyAdapterInitSignature(PrivateGlobals, adapterInit);

    return adapterInit;
}

_Use_decl_annotations_
NxAdapter::NxAdapter(
    NX_PRIVATE_GLOBALS const & NxPrivateGlobals,
    AdapterInit const & AdapterInit,
    NETADAPTER Adapter
)
    : CFxObject(Adapter)
    , m_powerPolicy(NxPrivateGlobals.NxDriver->GetRecorderLog())
    , m_Device(AdapterInit.Device)
    , m_NxDriver(NxPrivateGlobals.NxDriver)
{
    if (AdapterInit.DatapathCallbacks.Size != 0)
    {
        RtlCopyMemory(
            &m_datapathCallbacks,
            &AdapterInit.DatapathCallbacks,
            AdapterInit.DatapathCallbacks.Size);
    }
    else
    {
        auto const defaultDatapathCallbacks = GetDefaultDatapathCallbacks();

        RtlCopyMemory(
            &m_datapathCallbacks,
            &defaultDatapathCallbacks,
            defaultDatapathCallbacks.Size);
    }

    if (AdapterInit.NetRequestAttributes.Size != 0)
    {
        RtlCopyMemory(
            &m_NetRequestObjectAttributes,
            &AdapterInit.NetRequestAttributes,
            AdapterInit.NetRequestAttributes.Size);
    }
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::Init(
    AdapterInit const *AdapterInit
)
/*++
    Since constructor cant fail have an init routine.
--*/
{
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_WORKITEM_CONFIG   stopIdleWorkItemConfig;
    PNX_STOP_IDLE_WORKITEM_CONTEXT workItemContext;

    auto offloadFacade = wil::make_unique_nothrow<NxOffloadFacade>(*this);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !offloadFacade);

    m_NxOffloadManager = wil::make_unique_nothrow<NxOffloadManager>(offloadFacade.release());
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_NxOffloadManager);

    CX_RETURN_IF_NOT_NT_SUCCESS(m_NxOffloadManager.get()->Initialize());

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = GetFxObject();

    NTSTATUS status = WdfWaitLockCreate(&objectAttributes, &m_DataPathControlLock);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfWaitLockCreate(&objectAttributes, &m_ExtensionCollectionLock);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&objectAttributes,
                                        NX_STOP_IDLE_WORKITEM_CONTEXT);
    objectAttributes.ParentObject = GetFxObject();

    WDF_WORKITEM_CONFIG_INIT(&stopIdleWorkItemConfig, _EvtStopIdleWorkItem);
    stopIdleWorkItemConfig.AutomaticSerialization = FALSE;

    status = WdfWorkItemCreate(&stopIdleWorkItemConfig,
                            &objectAttributes,
                            &m_PowerRequiredWorkItem);
    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "WdfWorkItemCreate failed (m_PowerRequiredWorkItem) %!STATUS!", status);
        return status;
    }
    workItemContext = GetStopIdleWorkItemObjectContext(m_PowerRequiredWorkItem);
    workItemContext->IsAoAcWorkItem = FALSE;

    status = WdfWorkItemCreate(&stopIdleWorkItemConfig,
                            &objectAttributes,
                            &m_AoAcDisengageWorkItem);
    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "WdfWorkItemCreate failed (m_AoAcDisengageWorkItem) %!STATUS!", status);
        return status;
    }
    workItemContext = GetStopIdleWorkItemObjectContext(m_AoAcDisengageWorkItem);
    workItemContext->IsAoAcWorkItem = TRUE;

    #ifdef _KERNEL_MODE
    StateMachineEngineConfig smConfig(WdfDeviceWdmGetDeviceObject(m_Device), NETADAPTERCX_TAG);
    #else
    StateMachineEngineConfig smConfig;
    #endif

    status = Initialize(smConfig);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "StateMachineEngine_Init failed %!STATUS!", status);
        return status;
    }

    status = InitializeDatapath();
    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "InitializeDatapath failed %!STATUS!", status);
        return status;
    }

    status = InitializeAdapterExtensions(AdapterInit);
    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "InitializeAdapterExtensions failed %!STATUS!", status);
        return status;
    }

    ClearGeneralAttributes();

    return status;
}

void
NxAdapter::PrepareForRebalance(
    void
)
{
    // Call into NDIS to put the miniport in stopped state
    NdisWdfPnpPowerEventHandler(
        GetNdisHandle(),
        NdisWdfActionPnpRebalance,
        NdisWdfActionPowerNone);

    NT_ASSERT(m_State == AdapterState::Stopped);

    m_State = AdapterState::Initialized;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::InitializeAdapterExtensions(
    AdapterInit const *AdapterInit
)
{
    for (auto& extension : AdapterInit->AdapterExtensions)
    {
        if (!m_adapterExtensions.append({ &extension }))
            return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

NxAdapter::~NxAdapter()
{
    if (m_NdisAdapterHandle != nullptr)
    {
        NTSTATUS ntStatus = NdisWdfPnpPowerEventHandler(
            m_NdisAdapterHandle,
            NdisWdfActionDeviceObjectCleanup,
            NdisWdfActionPowerNone);

        NT_FRE_ASSERT(ntStatus == STATUS_SUCCESS);
    }
}

NX_PRIVATE_GLOBALS *
NxAdapter::GetPrivateGlobals(
    void
) const
{
    // Return the private globals of the client driver that called NetAdapterCreate
    return m_NxDriver->GetPrivateGlobals();
}

NET_DRIVER_GLOBALS *
NxAdapter::GetPublicGlobals(
    void
) const
{
    return &m_NxDriver->GetPrivateGlobals()->Public;
}

_Use_decl_annotations_
void
NxAdapter::EnqueueEventSync(
    NxAdapter::Event Event
)
{
    EnqueueEvent(Event);
    m_TransitionComplete.Wait();
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::_Create(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    AdapterInit const & AdapterInit,
    WDF_OBJECT_ATTRIBUTES * ClientAttributes,
    NxAdapter ** Adapter
)
/*++
Routine Description:
    Static method that creates the NETADAPTER  object.

    This is the internal implementation of the NetAdapterCreate public API.

    Please refer to the NetAdapterCreate API for more description on this
    function and the arguments.
--*/
{
    *Adapter = nullptr;

    //
    // Create a WDFOBJECT for the NxAdapter
    //
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxAdapter);
    attributes.ParentObject = AdapterInit.Device;

    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxAdapter::_EvtCleanup;

    //
    // Ensure that the destructor would be called when this object is distroyed.
    //
    NxAdapter::_SetObjectAttributes(&attributes);

    wil::unique_wdf_any<NETADAPTER> netAdapter;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(
            &attributes,
            reinterpret_cast<WDFOBJECT *>(netAdapter.addressof())),
        "WdfObjectCreate for NetAdapter failed");

    //
    // Since we just created the netAdapter, the NxAdapter object has
    // yet not been constructed. Get the nxAdapter's memory.
    //
    void * nxAdapterMemory = GetNxAdapterFromHandle(netAdapter.get());

    //
    // Use the inplacement new and invoke the constructor on the
    // NxAdapter's memory
    //
    auto nxAdapter = new (nxAdapterMemory) NxAdapter(
        PrivateGlobals,
        AdapterInit,
        netAdapter.get());

    __analysis_assume(nxAdapter != nullptr);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        nxAdapter->Init(&AdapterInit),
        "Failed to initialize NETADAPTER. NETADAPTER=%p",
        nxAdapter->GetFxObject());

    //
    // Report the newly created device to Ndis.sys
    //
    if (AdapterInit.Device)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            nxAdapter->CreateNdisMiniport(),
            "Failed to create NDIS miniport");

        auto nxDevice = GetNxDeviceFromHandle(AdapterInit.Device);
        nxDevice->AdapterCreated(nxAdapter);
    }
    else
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (ClientAttributes != WDF_NO_OBJECT_ATTRIBUTES)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfObjectAllocateContext(
                netAdapter.get(),
                ClientAttributes,
                nullptr),
            "WdfObjectAllocateContext for ClientAttributes. NETADAPTER=%p",
            nxAdapter->GetFxObject());
    }

    //
    // Dont Fail after this point or else the client's Cleanup / Destroy
    // callbacks can get called.
    //

    nxAdapter->m_NblDatapath.SetNdisHandle(nxAdapter->GetNdisHandle());

    *Adapter = GetNxAdapterFromHandle(netAdapter.release());

    return STATUS_SUCCESS;
}

NDIS_MEDIUM
NxAdapter::GetMediaType(
    void
) const
{
    return m_MediaType;
}

_Use_decl_annotations_
void
NxAdapter::DispatchOidRequest(
    NDIS_OID_REQUEST * NdisOidRequest,
    DispatchContext * Context
)
{
    auto const nextHandler = Context->NextHandlerIndex++;
    auto const frameworkHandlerIndex = m_adapterExtensions.count();

    //
    // Note: It is not safe to use 'NdisOidRequest' or 'Context' after either one of
    // InvokeOidPreprocessCallback, FrameworkOidHandler or DispatchOidRequestToAdapter
    // has returned, since those calls might complete the OID request before returning
    //

    if (nextHandler < frameworkHandlerIndex)
    {
        auto const & extension = m_adapterExtensions[nextHandler];
        extension.InvokeOidPreprocessCallback(GetFxObject(), NdisOidRequest, Context);
    }
    else if (nextHandler == frameworkHandlerIndex)
    {
        FrameworkOidHandler(NdisOidRequest, Context);
    }
    else
    {
        NT_ASSERT(nextHandler == frameworkHandlerIndex + 1);
        DispatchOidRequestToAdapter(NdisOidRequest);
    }
}

_Use_decl_annotations_
void
NxAdapter::FrameworkOidHandler(
    NDIS_OID_REQUEST * NdisOidRequest,
    DispatchContext * Context
)
{
    switch (NdisOidRequest->RequestType)
    {
        case NdisRequestSetInformation:
            FrameworkSetOidHandler(NdisOidRequest, Context);
            break;

        default:
            //
            // Others types of OID requests are not handled by the framework,
            // dispatch to the next handler
            //
            DispatchOidRequest(NdisOidRequest, Context);
            break;
    }
}

_Use_decl_annotations_
void
NxAdapter::FrameworkSetOidHandler(
    NDIS_OID_REQUEST * NdisOidRequest,
    DispatchContext * Context
)
{
    auto Set = &NdisOidRequest->DATA.SET_INFORMATION;

    //
    // All internal OID handling is synchronous, we must either complete the request
    // (NdisMOidRequestComplete) or let the framework keep processing it (DispatchOidRequest)
    //

    switch(Set->Oid)
    {
        case OID_PM_PARAMETERS:
        {
            auto ndisStatus = m_powerPolicy.SetParameters(Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }
        case OID_PM_ADD_WOL_PATTERN:
        {
            auto ndisStatus = m_powerPolicy.AddWakePattern(GetFxObject(), Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }

        case OID_PM_REMOVE_WOL_PATTERN:
        {
            auto ndisStatus = m_powerPolicy.RemoveWakePattern(Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }

        case OID_PM_ADD_PROTOCOL_OFFLOAD:
        {
            auto ndisStatus = m_powerPolicy.AddProtocolOffload(GetFxObject(), Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }
        case OID_PM_REMOVE_PROTOCOL_OFFLOAD:
        {
            auto ndisStatus = m_powerPolicy.RemoveProtocolOffload(Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }
        default:
        {
            DispatchOidRequest(NdisOidRequest, Context);
            break;
        }
    }
}

_Use_decl_annotations_
void
NxAdapter::DispatchOidRequestToAdapter(
    NDIS_OID_REQUEST * NdisOidRequest
)
{
    //
    // Create the NxRequest object from the tranditional NDIS_OID_REQUEST
    //
    NxRequest * nxRequest;
    auto ndisStatus = NdisConvertNtStatusToNdisStatus(
        NxRequest::_Create(
            GetPrivateGlobals(),
            this,
            NdisOidRequest,
            &nxRequest));

    if (ndisStatus != NDIS_STATUS_SUCCESS)
    {
        //
        // _Create failed, so fail the NDIS request.
        //
        NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
        return;
    }

    //
    // Dispatch the OID to the client driver
    //

    if (m_DefaultRequestQueue == nullptr)
    {
        LogWarning(GetRecorderLog(), FLAG_ADAPTER,
            "Rejecting OID, client didn't create a default queue");

        nxRequest->Complete(STATUS_NOT_SUPPORTED);
        return;
    }

    m_DefaultRequestQueue->QueueRequest(nxRequest->GetFxObject());
}

NDIS_HANDLE
NxAdapter::GetNdisHandle(
    void
) const
{
    return m_NdisAdapterHandle;
}

_Use_decl_annotations_
void
NxAdapter::Refresh(
    DeviceState DeviceState
)
{
    if (m_Flags.StartPending)
    {
        StartForClient(DeviceState);
    }
    else if (m_Flags.StopPending)
    {
        StopForClient(DeviceState);
    }
}

_Use_decl_annotations_
void
NdisWdfCxCharacteristicsInitialize(
    NDIS_WDF_CX_CHARACTERISTICS & Characteristics
)
{
    NDIS_WDF_CX_CHARACTERISTICS_INIT(&Characteristics);

    Characteristics.EvtCxGetDeviceObject = EvtNdisGetDeviceObject;
    Characteristics.EvtCxGetNextDeviceObject = EvtNdisGetNextDeviceObject;
    Characteristics.EvtCxGetAssignedFdoName = EvtNdisGetAssignedFdoName;
    Characteristics.EvtCxPowerReference = EvtNdisPowerReference;
    Characteristics.EvtCxPowerDereference = EvtNdisPowerDereference;
    Characteristics.EvtCxPowerAoAcEngage = EvtNdisAoAcEngage;
    Characteristics.EvtCxPowerAoAcDisengage = EvtNdisAoAcDisengage;
    Characteristics.EvtCxGetNdisHandleFromDeviceObject = EvtNdisWdmDeviceGetNdisAdapterHandle;
    Characteristics.EvtCxUpdatePMParameters = EvtNdisUpdatePMParameters;
    Characteristics.EvtCxAllocateMiniportBlock = EvtNdisAllocateMiniportBlock;
    Characteristics.EvtCxMiniportCompleteAdd = EvtNdisMiniportCompleteAdd;
    Characteristics.EvtCxDeviceStartComplete = EvtNdisDeviceStartComplete;
    Characteristics.EvtCxMiniportDeviceReset = EvtNdisMiniportDeviceReset;
    Characteristics.EvtCxMiniportQueryDeviceResetSupport = EvtNdisMiniportQueryDeviceResetSupport;
    Characteristics.EvtCxGetWmiEventGuid = EvtNdisGetWmiEventGuid;
}

_Use_decl_annotations_
DEVICE_OBJECT *
EvtNdisGetDeviceObject(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the WDM Device Object for the clients FDO

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    return WdfDeviceWdmGetDeviceObject(static_cast<NxAdapter *>(Adapter)->GetDevice());
}

_Use_decl_annotations_
DEVICE_OBJECT *
EvtNdisGetNextDeviceObject(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the attached WDM Device Object for the clients FDO

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    return WdfDeviceWdmGetAttachedDevice(static_cast<NxAdapter *>(Adapter)->GetDevice());
}

_Use_decl_annotations_
NTSTATUS
EvtNdisGetAssignedFdoName(
    NDIS_HANDLE Adapter,
    UNICODE_STRING * FdoName
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the name NetAdapterCx assigned to this device. NDIS
    needs to know this name to be able to create a symbolic link for the miniport.

Arguments:
    Adapter            - The NxAdapter object reported as context to NDIS
    FdoName            - UNICODE_STRING that will receive the FDO name
--*/
{
    NTSTATUS       status;
    WDFSTRING      fdoName;
    UNICODE_STRING usFdoName;

    status = WdfStringCreate(
        NULL,
        WDF_NO_OBJECT_ATTRIBUTES,
        &fdoName);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = WdfDeviceRetrieveDeviceName(static_cast<NxAdapter *>(Adapter)->GetDevice(), fdoName);

    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    WdfStringGetUnicodeString(fdoName, &usFdoName);

    if (FdoName->MaximumLength < usFdoName.Length)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    RtlCopyUnicodeString(FdoName, &usFdoName);

Exit:

    WdfObjectDelete(fdoName);
    return status;
}

_Use_decl_annotations_
void
EvtNdisAoAcEngage(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    Called by NDIS to engage AoAC when NIC goes quiet (no AoAC ref counts).
    While AoAC is engaged selective suspend must always be stopped. This means
    we will not see power refs/derefs from selective suspend until AoAC is
    disengaged.

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS

--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->EngageAoAc();
}

_Use_decl_annotations_
NTSTATUS
EvtNdisAoAcDisengage(
    NDIS_HANDLE Adapter,
    BOOLEAN WaitForD0
)
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    return nxAdapter->DisengageAoAc(!!WaitForD0);
}

_Use_decl_annotations_
NTSTATUS
EvtNdisPowerReference(
    NDIS_HANDLE Adapter,
    BOOLEAN WaitForD0,
    BOOLEAN InvokeCompletionCallback
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine is called to acquire a power reference on the miniport.
    Every call to EvtNdisPowerReference must be matched by a call
    to EvtNdisPowerDereference. However, this function
    keeps tracks of failures from the underlying call to WdfDeviceStopIdle. So the
    caller only needs to make sure they always call power EvtNdisPowerDereference
    after calling EvtNdisPowerReference without keeping track of returned failures.

Arguments:
    Adapter   - The NxAdapter object reported as context to NDIS
    WaitForD0 - If WaitFromD0 is false and STATUS_PENDING status is returned
        the Cx will call back into NDIS once power up is complete.
    InvokeCompletionCallback - IFF WaitForD0 is FALSE (i.e. Async power ref),
        then the caller can ask for a callback when power up is complete
--*/
{
    NT_ASSERTMSG("Invalid parameter to NdisPowerRef",
                !(WaitForD0 && InvokeCompletionCallback));

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    auto nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    if (InvokeCompletionCallback)
    {
        // If NDIS wants us to invoke a completion callback let's
        // take the power reference in a separate thread
        nxAdapter->QueuePowerReferenceWorkItem();

        return STATUS_PENDING;
    }
    else
    {
        return nxDevice->PowerReference(
            WaitForD0,
            PTR_TO_TAG(EvtNdisPowerReference));
    }
}

void
NxAdapter::_EvtStopIdleWorkItem(
    _In_ WDFWORKITEM WorkItem
)
/*++
Routine Description:
    A shared WDF work item callback for processing an asynchronous
    EvtNdisPowerReference and EvtNdisAoAcDisengage callback.
    The workitem's WDF object context has a boolean to distinguish the two work items.

Arguments:
    WDFWORKITEM - The WDFWORKITEM object that is parented to NxAdapter.
--*/
{
    NTSTATUS status;
    PNX_STOP_IDLE_WORKITEM_CONTEXT pWorkItemContext = GetStopIdleWorkItemObjectContext(WorkItem);
    WDFOBJECT nxAdapterWdfHandle = WdfWorkItemGetParentObject(WorkItem);
    auto nxAdapter = GetNxAdapterFromHandle((NETADAPTER)nxAdapterWdfHandle);
    auto nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    status = nxDevice->PowerReference(
        TRUE,
        PTR_TO_TAG(_EvtStopIdleWorkItem));

    NdisWdfAsyncPowerReferenceCompleteNotification(nxAdapter->m_NdisAdapterHandle,
                                                status,
                                                pWorkItemContext->IsAoAcWorkItem); // IsAOACNotification

    if (pWorkItemContext->IsAoAcWorkItem) {
        nxAdapter->m_AoAcDisengageWorkItemPending = FALSE;
        nxAdapter->m_AoAcEngaged = FALSE; // Diagnostics
    }
    else {
        nxAdapter->m_PowerRequiredWorkItemPending = FALSE;
    }
}

_Use_decl_annotations_
NTSTATUS
EvtNdisAllocateMiniportBlock(
    NDIS_HANDLE Adapter,
    ULONG Size,
    void ** MiniportBlock
)
/*++
Routine Description:
    This function will allocate the memory for the adapter's miniport block as
    a context in the NETADAPTER object. This way we can ensure the miniport block
    will be valid memory during the adapter's life time.
--*/
{

    NTSTATUS status = STATUS_SUCCESS;

    WDF_OBJECT_ATTRIBUTES contextAttributes;

    // Initialize contextAttributes with our dummy type
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &contextAttributes,
        NDIS_MINIPORT_BLOCK_TYPE);

    // Override the memory size to allocate with what NDIS told us
    contextAttributes.ContextSizeOverride = Size;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    status = WdfObjectAllocateContext(nxAdapter->GetFxObject(), &contextAttributes, MiniportBlock);

    if (!NT_SUCCESS(status))
    {
        LogError(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
            "WdfObjectAllocateContext failed to allocate the miniport block, %!STATUS!", status);

        return status;
    }

    return status;
}

_Use_decl_annotations_
void
EvtNdisMiniportCompleteAdd(
    NDIS_HANDLE Adapter,
    NDIS_WDF_COMPLETE_ADD_PARAMS * Params
)
/*++
Routine Description:
    NDIS uses this callback to report certain adapter parameters to the NIC
    after the Miniport side of device add has completed.
--*/
{

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->NdisMiniportCreationComplete(*Params);
}

_Use_decl_annotations_
void
EvtNdisDeviceStartComplete(
    NDIS_HANDLE Adapter
)
/*
Description:

    NDIS uses this callback to indicate the underlying device has completed
    the start IRP, but because NDIS does not distinguish between device and
    adapter it reports for only one of a device's child adapters.
*/
{
    UNREFERENCED_PARAMETER(Adapter);
}


_Use_decl_annotations_
NTSTATUS
EvtNdisMiniportDeviceReset(
    NDIS_HANDLE Adapter,
    NET_DEVICE_RESET_TYPE NetDeviceResetType
)
/*
Routine Description:

    NDIS uses this callback to indicate the underlying device is in a bad state
    and it needs to be reset in order to recover.

Arguments:
    Adapter            - The NxAdapter object reported as context to NDIS
    NetDeviceResetType - The type of reset operation requested (by default this is PLDR)
*/
{
    DEVICE_RESET_TYPE DeviceResetType;
    NxAdapter *nxAdapter = static_cast<NxAdapter *>(Adapter);
    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    nxDevice->SetFailingDeviceRequestingResetFlag();

    // Trigger reset of the device as we are told to do so
    switch (NetDeviceResetType)
    {
    case FunctionLevelReset:
        DeviceResetType = FunctionLevelDeviceReset;
        break;
    case PlatformLevelReset:
        DeviceResetType = PlatformLevelDeviceReset;
        break;
    default:
        DeviceResetType = PlatformLevelDeviceReset;
        break;
    };

    return nxDevice->DispatchDeviceReset(DeviceResetType);
}

_Use_decl_annotations_
NTSTATUS
EvtNdisMiniportQueryDeviceResetSupport(
    NDIS_HANDLE Adapter,
    PULONG SupportedNetDeviceResetTypes
)
/*
Routine Description:

    NDIS uses this callback to query whether Device Reset is supported by the underlying
    device for which there is a NetAdapter based driver present

Arguments:
    Adapter                      - The NxAdapter object reported as context to NDIS
    SupportedNetDeviceResetTypes - Out parameter which will have the information about what all reset types is supported
*/
{
    NxAdapter *nxAdapter = static_cast<NxAdapter *>(Adapter);
    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    if (nxDevice->GetSupportedDeviceResetType(SupportedNetDeviceResetTypes))
    {
        return STATUS_SUCCESS;
    }
    else
    {
        return STATUS_NOT_SUPPORTED;
    }
}

_Use_decl_annotations_
void
EvtNdisPowerDereference(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine is called to release a power reference on the
    miniport that it previously acquired. Every call to EvtNdisPowerReference
    MUST be matched by a call to EvtNdisPowerDereference.
    However, since failures are tracked from calls to EvtNdisPowerReference, the
    caller can call EvtNdisPowerDereference without having to keep track of
    failures from calls to EvtNdisPowerReference.



Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    auto nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    nxDevice->PowerDereference(PTR_TO_TAG(EvtNdisPowerDereference));
}

_Use_decl_annotations_
NTSTATUS
EvtNdisGetWmiEventGuid(
    NDIS_HANDLE Adapter,
    NTSTATUS GuidStatus,
    NDIS_GUID ** Guid
)
{
    auto const nxAdapter = static_cast<NxAdapter *>(Adapter);
    auto const nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    return nxDevice->WmiGetEventGuid(GuidStatus, Guid);
}

NTSTATUS
NxAdapter::SetRegistrationAttributes(
    void
)
{
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES adapterRegistration = {0};
    NTSTATUS                status;
    NDIS_STATUS             ndisStatus;

    //
    // Set the registration attributes for the network adapter.
    //
    // Note: Unlike a traditional NDIS miniport, the NetAdapterCx client
    // must not set the adapterRegistration.MiniportAdapterContext value.
    //

    adapterRegistration.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    adapterRegistration.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;
    adapterRegistration.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;

    adapterRegistration.AttributeFlags =
        NDIS_MINIPORT_ATTRIBUTES_SURPRISE_REMOVE_OK |
        NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM |
        NDIS_MINIPORT_ATTRIBUTES_NO_PAUSE_ON_SUSPEND;

    adapterRegistration.InterfaceType = NdisInterfacePNPBus;

    ndisStatus = NdisMSetMiniportAttributes(
        m_NdisAdapterHandle,
        (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&adapterRegistration);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
                 "NdisMSetMiniportAttributes failed, %!NDIS_STATUS!", ndisStatus);
        goto Exit;
    }

Exit:

    return status;
}


void
NxAdapter::SetPermanentLinkLayerAddress(
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS *LinkLayerAddress
)
{
    NT_ASSERT(LinkLayerAddress->Length <= NDIS_MAX_PHYS_ADDRESS_LENGTH);

    RtlCopyMemory(
        &m_PermanentLinkLayerAddress,
        LinkLayerAddress,
        sizeof(m_PermanentLinkLayerAddress));
}

void
NxAdapter::SetCurrentLinkLayerAddress(
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS *LinkLayerAddress
)
{
    NT_ASSERT(LinkLayerAddress->Length <= NDIS_MAX_PHYS_ADDRESS_LENGTH);

    RtlCopyMemory(
        &m_CurrentLinkLayerAddress,
        LinkLayerAddress,
        sizeof(m_CurrentLinkLayerAddress));

    if (m_Flags.GeneralAttributesSet)
    {
        IndicateCurrentLinkLayerAddressToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetReceiveScalingCapabilities(
    NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const & Capabilities
)
{
    ASSERT(sizeof(m_ReceiveScalingCapabilities) >= Capabilities.Size);

    RtlCopyMemory(
        &m_ReceiveScalingCapabilities,
        &Capabilities,
        Capabilities.Size);
}

static
void
RecvScaleCapabilitiesInit(
    _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const & Capabilities,
    _Out_ NDIS_RECEIVE_SCALE_CAPABILITIES & RecvScaleCapabilities
)
{
    NDIS_RSS_CAPS_FLAGS rssCapsFlags = {
        NDIS_RSS_CAPS_SUPPORTS_INDEPENDENT_ENTRY_MOVE
    };

    if (Capabilities.ReceiveScalingHashTypes & NetAdapterReceiveScalingHashTypeToeplitz)
    {
        rssCapsFlags |= NdisHashFunctionToeplitz;
    }

    if ((Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv4) ||
        ((Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv4) &&
        (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv4Options)))
    {
        if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeTcp)
        {
            rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV4;
        }

        if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeUdp)
        {
            rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_UDP_IPV4;
        }
    }

    if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv6)
    {
        if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeTcp)
        {
            rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV6;
            if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv6Extensions)
            {
                rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV6_EX;
            }
        }

        if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeUdp)
        {
            rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_UDP_IPV6;
            if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv6Extensions)
            {
                rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_UDP_IPV6_EX;
            }
        }
    }

    RecvScaleCapabilities = {
        {
            NDIS_OBJECT_TYPE_RSS_CAPABILITIES,
            NDIS_RECEIVE_SCALE_CAPABILITIES_REVISION_3,
            sizeof(RecvScaleCapabilities),
        },
        rssCapsFlags,
        1, // NumberOfInterruptMessages, not used internally
        static_cast<ULONG>(Capabilities.NumberOfQueues),
        static_cast<USHORT>(Capabilities.IndirectionTableSize),
    };
}

NTSTATUS
NxAdapter::SetGeneralAttributes(
    void
)
{
    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES adapterGeneral = {0};
    NDIS_PM_CAPABILITIES                     pmCapabilities;
    NTSTATUS                                 status = STATUS_INVALID_PARAMETER;
    NDIS_STATUS                              ndisStatus;

    if (m_RxCapabilities.Size == 0 || m_TxCapabilities.Size == 0)
    {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "Client missed or incorrectly called NetAdapterSetDatapathCapabilities");
        goto Exit;
    }

    if (m_LinkLayerCapabilities.Size == 0) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
                 "Client missed or incorrectly called NetAdapterSetLinkLayerCapabilities");
        goto Exit;
    }

    if (m_MtuSize == 0) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "Client missed or incorrectly called NetAdapterSetMtuSize");
        goto Exit;
    }

    // If the client set either only the permanent or the current link layer
    // address, use the one set as both
    if (HasPermanentLinkLayerAddress() && !HasCurrentLinkLayerAddress())
    {
        RtlCopyMemory(
            &m_CurrentLinkLayerAddress,
            &m_PermanentLinkLayerAddress,
            sizeof(m_CurrentLinkLayerAddress));
    }
    else if (!HasPermanentLinkLayerAddress() && HasCurrentLinkLayerAddress())
    {
        RtlCopyMemory(
            &m_PermanentLinkLayerAddress,
            &m_CurrentLinkLayerAddress,
            sizeof(m_PermanentLinkLayerAddress));
    }

    // In NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES the length of both the permanent and the
    // current link layer addresses are represented by MacAddressLength, which implies that if the
    // client set link layer addresses in EvtAdapterSetCapabilities both need to have the same
    // length
    if (m_CurrentLinkLayerAddress.Length != m_PermanentLinkLayerAddress.Length)
    {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "The client set permanent and current link layer addresses with different lengths.");
        goto Exit;
    }

    adapterGeneral.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
    adapterGeneral.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_2;
    adapterGeneral.Header.Revision = NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_2;


    //
    // Specify the Link Capabilities
    //
    adapterGeneral.MtuSize          = m_MtuSize;
    adapterGeneral.MaxXmitLinkSpeed = m_LinkLayerCapabilities.MaxTxLinkSpeed;
    adapterGeneral.MaxRcvLinkSpeed  = m_LinkLayerCapabilities.MaxRxLinkSpeed;

    //
    // Specify the current Link State
    //
    adapterGeneral.XmitLinkSpeed           = m_CurrentLinkState.TxLinkSpeed;
    adapterGeneral.RcvLinkSpeed            = m_CurrentLinkState.RxLinkSpeed;
    adapterGeneral.MediaDuplexState        = m_CurrentLinkState.MediaDuplexState;
    adapterGeneral.MediaConnectState       = m_CurrentLinkState.MediaConnectState;
    adapterGeneral.AutoNegotiationFlags    = m_CurrentLinkState.AutoNegotiationFlags;
    adapterGeneral.SupportedPauseFunctions = m_CurrentLinkState.SupportedPauseFunctions;

    //
    // Specify the Mac Capabilities
    //

    adapterGeneral.SupportedPacketFilters = m_PacketFilter.SupportedPacketFilters;
    adapterGeneral.MaxMulticastListSize = m_MulticastList.MaxMulticastAddresses;
    adapterGeneral.SupportedStatistics =  NDIS_STATISTICS_XMIT_OK_SUPPORTED |
        NDIS_STATISTICS_RCV_OK_SUPPORTED |
        NDIS_STATISTICS_GEN_STATISTICS_SUPPORTED;

    // Set the MacAddressLength to be the length of the permanent link layer address,
    // this is fine since we require that both the permanent and current addresses
    // have the same length during EvtAdapterSetCapabilities.
    //
    // It is legal to have a zero length link layer address.
    //
    // Note: We don't need to enforce the same behavior if the client tries to change
    // its current address later. NDIS only enforces this behavior in NdisMSetMiniportAttributes.
    adapterGeneral.MacAddressLength = m_PermanentLinkLayerAddress.Length;

    C_ASSERT(sizeof(m_PermanentLinkLayerAddress.Address) == sizeof(adapterGeneral.PermanentMacAddress));
    C_ASSERT(sizeof(m_CurrentLinkLayerAddress.Address) == sizeof(adapterGeneral.CurrentMacAddress));

    if (adapterGeneral.MacAddressLength > 0)
    {
        RtlCopyMemory(
            adapterGeneral.CurrentMacAddress,
            m_CurrentLinkLayerAddress.Address,
            sizeof(adapterGeneral.CurrentMacAddress));

        RtlCopyMemory(
            adapterGeneral.PermanentMacAddress,
            m_PermanentLinkLayerAddress.Address,
            sizeof(adapterGeneral.PermanentMacAddress));
    }

    //
    // Set the power management capabilities.
    //
    NDIS_PM_CAPABILITIES_INIT_FROM_NET_ADAPTER_POWER_CAPABILITIES(
        &pmCapabilities,
        GetCombinedMediaSpecificWakeUpEvents(),
        &m_powerOffloadArpCapabilities,
        &m_powerOffloadNSCapabilities,
        &m_wakeBitmapCapabilities,
        &m_magicPacketCapabilities,
        &m_wakeMediaChangeCapabilities,
        &m_wakePacketFilterCapabilities);

    adapterGeneral.PowerManagementCapabilitiesEx = &pmCapabilities;

    auto nxDevice = GetNxDeviceFromHandle(GetDevice());
    adapterGeneral.SupportedOidList = const_cast<NDIS_OID *>(nxDevice->GetOidList());
    adapterGeneral.SupportedOidListLength = (ULONG) (nxDevice->GetOidListCount() * sizeof(NDIS_OID));

    // translate net adapter -> ndis
    NDIS_RECEIVE_SCALE_CAPABILITIES RecvScaleCapabilities;
    RecvScaleCapabilitiesInit(m_ReceiveScalingCapabilities, RecvScaleCapabilities);
    adapterGeneral.RecvScaleCapabilities = &RecvScaleCapabilities;

    //
    // The Remaining General Attributes Fields are not used for
    // a WDF NDIS model. Some of them come from Inf, some of them are
    // deprecated. Just set them to invalid values or default
    // values for diagnaosability purposes.
    //
    adapterGeneral.MediaType = (NDIS_MEDIUM)-1;
    adapterGeneral.PhysicalMediumType = NdisPhysicalMediumUnspecified;
    adapterGeneral.LookaheadSize = 0;
    adapterGeneral.MacOptions = 0;

    adapterGeneral.IfType = (NET_IFTYPE)-1;
    adapterGeneral.IfConnectorPresent = FALSE;

    ndisStatus = NdisMSetMiniportAttributes(
        m_NdisAdapterHandle,
        (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&adapterGeneral);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
                 "NdisMSetMiniportAttributes failed, %!NDIS_STATUS!", ndisStatus);
        goto Exit;
    }






    m_Flags.GeneralAttributesSet = TRUE;

Exit:

    return status;
}

void
NxAdapter::QueuePowerReferenceWorkItem(
    void
)
{
    m_PowerRequiredWorkItemPending = TRUE;
    WdfWorkItemEnqueue(m_PowerRequiredWorkItem);
}

void
NxAdapter::EngageAoAc(
    void
)
{
    auto nxDevice = GetNxDeviceFromHandle(GetDevice());

    ASSERT(!m_AoAcEngaged);

    nxDevice->PowerDereference(PTR_TO_TAG(EvtNdisAoAcEngage));

    m_AoAcEngaged = TRUE;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::DisengageAoAc(
    bool WaitForD0
)
{
    auto nxDevice = GetNxDeviceFromHandle(GetDevice());

    NTSTATUS status;

    ASSERT(m_AoAcEngaged);

    //
    // If NDIS is asking for a synchronous power reference we call
    // stop idle from this thread, otherwise we queue a work item
    //
    if (WaitForD0)
    {
        status = nxDevice->PowerReference(
            WaitForD0,
            PTR_TO_TAG(EvtNdisAoAcDisengage));

        if (!NT_SUCCESS(status))
        {
            m_AoAcEngaged = FALSE;
        }
    }
    else
    {
        m_AoAcDisengageWorkItemPending = TRUE;
        WdfWorkItemEnqueue(m_AoAcDisengageWorkItem);

        status = STATUS_PENDING;
    }

    return status;
}

void
NxAdapter::ClearGeneralAttributes(
    void
)
{
    //
    // Clear the Capabilities that are mandatory for the NetAdapter driver to set
    //
    RtlZeroMemory(&m_LinkLayerCapabilities,     sizeof(NET_ADAPTER_LINK_LAYER_CAPABILITIES));
    m_MtuSize = 0;
    m_PacketFilter = {};
    m_MulticastList = {};

    //
    // Initialize the optional capabilities to defaults.
    //
    NET_ADAPTER_LINK_STATE_INIT_DISCONNECTED(&m_CurrentLinkState);
}

void
NxAdapter::SetMtuSize(
    _In_ ULONG mtuSize
)
{
    m_MtuSize = mtuSize;

    if (m_Flags.GeneralAttributesSet)
    {
        LogVerbose(GetRecorderLog(), FLAG_ADAPTER, "Updated MTU Size to %lu", mtuSize);

        IndicateMtuSizeChangeToNdis();
    }
}

ULONG
NxAdapter::GetMtuSize(
    void
) const
/*++
Routine Description:

    Get the Mtu of the given NETADAPTER.

Returns:
    The Mtu of the given NETADAPTER
--*/
{
    return m_MtuSize;
}

ULONG
NxAdapter::GetCombinedMediaSpecificWakeUpEvents(
    void
) const
{
    ULONG mediaSpecificWakeUpEvents = 0;

    for (auto const & extension : m_adapterExtensions)
    {
        mediaSpecificWakeUpEvents |= extension.GetNdisPmCapabilities().MediaSpecificWakeUpEvents;
    }

    return mediaSpecificWakeUpEvents;
}

void
NxAdapter::IndicatePowerCapabilitiesToNdis(
    void
)
/*++
Routine Description:

    Sends a Power Capabilities status indication to NDIS.

--*/
{
    NDIS_PM_CAPABILITIES   pmCapabilities;

    NDIS_PM_CAPABILITIES_INIT_FROM_NET_ADAPTER_POWER_CAPABILITIES(
        &pmCapabilities,
        GetCombinedMediaSpecificWakeUpEvents(),
        &m_powerOffloadArpCapabilities,
        &m_powerOffloadNSCapabilities,
        &m_wakeBitmapCapabilities,
        &m_magicPacketCapabilities,
        &m_wakeMediaChangeCapabilities,
        &m_wakePacketFilterCapabilities);

    auto statusIndication =
        MakeNdisStatusIndication(
            m_NdisAdapterHandle, NDIS_STATUS_PM_HARDWARE_CAPABILITIES, pmCapabilities);

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &statusIndication);
}

void
NxAdapter::IndicateCurrentLinkStateToNdis(
    void
)
/*++
Routine Description:

    Sends a Link status indication to NDIS.

--*/
{
    NDIS_LINK_STATE linkState = {0};

    linkState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    linkState.Header.Size = NDIS_SIZEOF_LINK_STATE_REVISION_1;
    linkState.Header.Revision = NDIS_LINK_STATE_REVISION_1;

    linkState.MediaConnectState    = m_CurrentLinkState.MediaConnectState;
    linkState.MediaDuplexState     = m_CurrentLinkState.MediaDuplexState;
    linkState.XmitLinkSpeed        = m_CurrentLinkState.TxLinkSpeed;
    linkState.RcvLinkSpeed         = m_CurrentLinkState.RxLinkSpeed;
    linkState.AutoNegotiationFlags = m_CurrentLinkState.AutoNegotiationFlags;
    linkState.PauseFunctions       =
        (NDIS_SUPPORTED_PAUSE_FUNCTIONS) m_CurrentLinkState.SupportedPauseFunctions;

    auto statusIndication =
        MakeNdisStatusIndication(
            m_NdisAdapterHandle, NDIS_STATUS_LINK_STATE, linkState);

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &statusIndication);
}

void
NxAdapter::IndicateMtuSizeChangeToNdis(
    void
)
{
    auto statusIndication =
        MakeNdisStatusIndication(
            m_NdisAdapterHandle, NDIS_STATUS_L2_MTU_SIZE_CHANGE, m_MtuSize);

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &statusIndication);
}

void
NxAdapter::IndicateCurrentLinkLayerAddressToNdis(
    void
)
{
    auto statusIndication = MakeNdisStatusIndication(
        m_NdisAdapterHandle,
        NDIS_STATUS_CURRENT_MAC_ADDRESS_CHANGE,
        m_CurrentLinkLayerAddress);

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &statusIndication);
}

_Use_decl_annotations_
void
NdisMiniportDriverCharacteristicsInitialize(
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS & Characteristics
)
{
    Characteristics = {};

    Characteristics.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    Characteristics.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3;
    Characteristics.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3;
    Characteristics.MajorNdisVersion = NDIS_MINIPORT_MAJOR_VERSION;
    Characteristics.MinorNdisVersion = NDIS_MINIPORT_MINOR_VERSION;

    //
    // NDIS should not hold driver version information of WDF managed miniports
    //
    Characteristics.MajorDriverVersion = 0;
    Characteristics.MinorDriverVersion = 0;

    Characteristics.Flags = 0;
    Characteristics.Flags |= NDIS_WDF_PNP_POWER_HANDLING; // prevent NDIS from owning the driver dispatch table
    Characteristics.Flags |= NDIS_DRIVER_POWERMGMT_PROXY; // this is a virtual device so need updated capabilities

    Characteristics.InitializeHandlerEx = EvtNdisInitializeEx;
    Characteristics.HaltHandlerEx = EvtNdisHaltEx;
    Characteristics.PauseHandler = EvtNdisPause;
    Characteristics.RestartHandler = EvtNdisRestart;
    Characteristics.OidRequestHandler = EvtNdisOidRequest;
    Characteristics.DirectOidRequestHandler = EvtNdisDirectOidRequest;
    Characteristics.SendNetBufferListsHandler = EvtNdisSendNetBufferLists;
    Characteristics.ReturnNetBufferListsHandler = EvtNdisReturnNetBufferLists;
    Characteristics.CancelSendHandler = EvtNdisCancelSend;
    Characteristics.DevicePnPEventNotifyHandler = EvtNdisDevicePnpEventNotify;
    Characteristics.ShutdownHandlerEx = EvtNdisShutdownEx;
    Characteristics.CancelOidRequestHandler = EvtNdisCancelOidRequest;
    Characteristics.CancelDirectOidRequestHandler = EvtNdisCancelDirectOidRequest;
    Characteristics.SynchronousOidRequestHandler = EvtNdisSynchronousOidRequestHandler;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisInitializeEx(
    NDIS_HANDLE NdisAdapterHandle,
    NDIS_HANDLE UnusedContext,
    NDIS_MINIPORT_INIT_PARAMETERS * MiniportInitParameters
)
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to initialize a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called ndisPnPStartDevice routine.

Arguments:

    NdisAdapterHandle - Opaque Ndis handle for the Adapter

Return Value:

    NDIS_STATUS_xxx code

--*/
{
    UNREFERENCED_PARAMETER((UnusedContext, MiniportInitParameters));

    auto nxAdapter = static_cast<NxAdapter *>(
        NdisWdfGetAdapterContextFromAdapterHandle(NdisAdapterHandle));

    return NdisConvertNtStatusToNdisStatus(nxAdapter->NdisInitialize());
}

_Use_decl_annotations_
void
EvtNdisHaltEx(
    NDIS_HANDLE Adapter,
    NDIS_HALT_ACTION HaltAction
)
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to halt a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called NdisWdfPnpAddDevice routine.

Arguments:

    Adapter - NxAdapter reported as Context to NDIS.sys

--*/
{
    UNREFERENCED_PARAMETER(HaltAction);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportHalt);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisPause(
    NDIS_HANDLE Adapter,
    NDIS_MINIPORT_PAUSE_PARAMETERS * PauseParameters
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to pause the adapter.
    NetAdapterCx in response calls the clients EvtAdapterPause callback.

--*/
{
    UNREFERENCED_PARAMETER(PauseParameters);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportPause);

    return NDIS_STATUS_PENDING;
}

NET_LUID
NxAdapter::GetNetLuid(
    void
)
{
    return m_NetLuid;
}

bool
NxAdapter::StartCalled(
    void
) const
{
    return !!m_Flags.StartCalled;
}

UNICODE_STRING const &
NxAdapter::GetBaseName(
    void
) const
{
    return m_BaseName;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisRestart(
    NDIS_HANDLE Adapter,
    NDIS_MINIPORT_RESTART_PARAMETERS * RestartParameters
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to start (restart) the adapter.
    NetAdapterCx in response calls the clients EvtAdapterStart callback.

--*/
{
    UNREFERENCED_PARAMETER(RestartParameters);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportRestart);

    return NDIS_STATUS_PENDING;
}

_Use_decl_annotations_
void
EvtNdisSendNetBufferLists(
    NDIS_HANDLE Adapter,
    NET_BUFFER_LIST * NetBufferLists,
    NDIS_PORT_NUMBER PortNumber,
    ULONG SendFlags
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to send data over the network the adapter.

    NetAdapterCx in response calls the clients EvtAdapterSendNetBufferLists callback.

--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->SendNetBufferLists(
        NetBufferLists,
        PortNumber,
        SendFlags);
}

_Use_decl_annotations_
void
EvtNdisReturnNetBufferLists(
    NDIS_HANDLE Adapter,
    NET_BUFFER_LIST * NetBufferLists,
    ULONG ReturnFlags
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to return a NBL to the miniport.

    NetAdapterCx in response calls the clients EvtAdapterReturnNetBufferLists callback.

--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->ReturnNetBufferLists(
        NetBufferLists,
        ReturnFlags);
}

_Use_decl_annotations_
void
EvtNdisCancelSend(
    NDIS_HANDLE Adapter,
    void * CancelId
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to cancel a send NBL request.

--*/
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(CancelId);

    NT_ASSERTMSG("Missing Implementation", FALSE);
}

_Use_decl_annotations_
void
EvtNdisDevicePnpEventNotify(
    NDIS_HANDLE Adapter,
    NET_DEVICE_PNP_EVENT * NetDevicePnPEvent
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to report a Pnp Event.

--*/
{
    auto recorderLog = static_cast<NxAdapter *>(Adapter)->GetRecorderLog();

    if (NetDevicePnPEvent->DevicePnPEvent == NdisDevicePnPEventPowerProfileChanged) {
        if (NetDevicePnPEvent->InformationBufferLength == sizeof(ULONG)) {
            ULONG NdisPowerProfile = *((PULONG)NetDevicePnPEvent->InformationBuffer);

            if (NdisPowerProfile == NdisPowerProfileBattery) {
                LogInfo(recorderLog, FLAG_ADAPTER, "On NdisPowerProfileBattery");
            } else if (NdisPowerProfile == NdisPowerProfileAcOnLine) {
                LogInfo(recorderLog, FLAG_ADAPTER, "On NdisPowerProfileAcOnLine");
            } else {
                LogError(recorderLog, FLAG_ADAPTER, "Unexpected PowerProfile %d", NdisPowerProfile);
                NT_ASSERTMSG("Unexpected PowerProfile", FALSE);
            }
        }
    } else {
        LogInfo(recorderLog, FLAG_ADAPTER, "PnpEvent %!NDIS_DEVICE_PNP_EVENT! from Ndis.sys", NetDevicePnPEvent->DevicePnPEvent);
    }
}

_Use_decl_annotations_
void
EvtNdisShutdownEx(
    NDIS_HANDLE Adapter,
    NDIS_SHUTDOWN_ACTION ShutdownAction
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(ShutdownAction);

    NT_ASSERTMSG("Missing Implementation", FALSE);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisOidRequest(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * NdisOidRequest
)
{
    NDIS_STATUS ndisStatus;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    // intercept and forward to the NDIS translator.
    if (nxAdapter->NetClientOidPreProcess(*NdisOidRequest, ndisStatus))
    {
        return ndisStatus;
    }

    auto dispatchContext = new (&NdisOidRequest->MiniportReserved[0]) DispatchContext();
    nxAdapter->DispatchOidRequest(NdisOidRequest, dispatchContext);

    return NDIS_STATUS_PENDING;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisDirectOidRequest(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * Request
)
{
    NDIS_STATUS ndisStatus;

    // intercept and forward to the NDIS translator.
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    if (nxAdapter->NetClientDirectOidPreProcess(*Request, ndisStatus))
    {
        return ndisStatus;
    }

    //
    // Create the NxRequest object from the tranditional NDIS_OID_REQUEST
    //
    NxRequest * nxRequest;
    ndisStatus = NdisConvertNtStatusToNdisStatus(
        NxRequest::_Create(
            nxAdapter->GetPrivateGlobals(),
            nxAdapter,
            Request,
            &nxRequest));

    if (ndisStatus != NDIS_STATUS_SUCCESS)
    {
        //
        // _Create failed, so fail the NDIS request.
        //
        return ndisStatus;
    }

    if (nxAdapter->m_DefaultDirectRequestQueue == NULL) {
        LogWarning(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
                   "Rejecting OID, client didn't create a default direct queue");
        return NDIS_STATUS_NOT_SUPPORTED;
    }

    nxAdapter->m_DefaultDirectRequestQueue->QueueRequest(nxRequest->GetFxObject());

    ndisStatus = NDIS_STATUS_PENDING;

    return ndisStatus;
}

_Use_decl_annotations_
void
EvtNdisCancelOidRequest(
    NDIS_HANDLE Adapter,
    void * RequestId
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(RequestId);
}

_Use_decl_annotations_
void
EvtNdisCancelDirectOidRequest(
    NDIS_HANDLE Adapter,
    void * RequestId
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(RequestId);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisSynchronousOidRequestHandler(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * Request
)
{
    NDIS_STATUS ndisStatus;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    // intercept and forward to the NDIS translator.
    if (nxAdapter->NetClientSynchrohousOidPreProcess(*Request, ndisStatus))
    {
        return ndisStatus;
    }

    return NdisConvertNtStatusToNdisStatus(STATUS_NOT_SUPPORTED);
}

NTSTATUS
NxAdapter::InitializeDatapath(
    void
)
{
    CxDriverContext *driverContext = GetCxDriverContextFromHandle(WdfGetDriver());

    void * client;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        driverContext->TranslationAppFactory.CreateApp(
        &CxDispatch,
        reinterpret_cast<NET_CLIENT_ADAPTER>(this),
        &AdapterDispatch,
        &client,
        &m_ClientDispatch),
        "Create NxApp failed. NxAdapter=%p", this);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_Apps.append(wistd::unique_ptr<INxApp> { static_cast<INxApp *>(client) }));

    return STATUS_SUCCESS;
}

NTSTATUS
NxAdapter::CreateNdisMiniport(
    void
)
{
    NT_FRE_ASSERT(m_NdisAdapterHandle == nullptr);

    auto device = GetDevice();
    auto driver = WdfDeviceGetDriver(device);

    return NdisWdfPnPAddDevice(
        WdfDriverWdmGetDriverObject(driver),
        WdfDeviceWdmGetPhysicalDevice(device),
        &m_NdisAdapterHandle,
        reinterpret_cast<NDIS_HANDLE>(this));
}

_Use_decl_annotations_
void
NxAdapter::NdisMiniportCreationComplete(
    NDIS_WDF_COMPLETE_ADD_PARAMS const & Parameters
)
{
    m_NetLuid = Parameters.NetLuid;
    m_MediaType = Parameters.MediaType;

    // The buffer pointed by Parameters.BaseName.Buffer is part of the NDIS_MINIPORT_BLOCK,
    // which is a context of the NETADAPTER object. So we don't need to make a
    // deep copy here
    m_BaseName = Parameters.BaseName;
    m_InstanceName = Parameters.AdapterInstanceName;
}

_Use_decl_annotations_
void
NxAdapter::SendNetBufferLists(
    NET_BUFFER_LIST * NetBufferLists,
    NDIS_PORT_NUMBER PortNumber,
    ULONG SendFlags
)
{
    m_NblDatapath.SendNetBufferLists(
        NetBufferLists,
        PortNumber,
        SendFlags);
}

_Use_decl_annotations_
void
NxAdapter::ReturnNetBufferLists(
    NET_BUFFER_LIST * NetBufferLists,
    ULONG ReturnFlags
)
{
    m_NblDatapath.ReturnNetBufferLists(
        NetBufferLists,
        ReturnFlags);
}

_Use_decl_annotations_
bool
NxAdapter::NetClientOidPreProcess(
    NDIS_OID_REQUEST & Request,
    NDIS_STATUS & NdisStatus
) const
{
    for (auto & app : m_Apps)
    {
        if (m_ClientDispatch->NdisOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_ClientDispatch->NdisOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}

_Use_decl_annotations_
bool
NxAdapter::NetClientDirectOidPreProcess(
    NDIS_OID_REQUEST & Request,
    NDIS_STATUS & NdisStatus
) const
{
    for (size_t i = 0; i < m_Apps.count(); i++)
    {
        auto & app = m_Apps[i];

        if (m_ClientDispatch->NdisDirectOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_ClientDispatch->NdisDirectOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}

_Use_decl_annotations_
bool
NxAdapter::NetClientSynchrohousOidPreProcess(
    NDIS_OID_REQUEST & Request,
    NDIS_STATUS & NdisStatus
) const
{
    for (size_t i = 0; i < m_Apps.count(); i++)
    {
        auto & app = m_Apps[i];

        if (m_ClientDispatch->NdisSynchronousOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_ClientDispatch->NdisSynchronousOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}

NTSTATUS
NxAdapter::ClientStart(
    void
)
{
    NT_ASSERT(m_Flags.StartPending == false);

    m_Flags.StartCalled = TRUE;
    m_Flags.StartPending = true;

    NET_EXTENSION_PRIVATE const mdlExtension = {
        NET_FRAGMENT_EXTENSION_MDL_NAME,
        NET_FRAGMENT_EXTENSION_MDL_VERSION_1_SIZE,
        NET_FRAGMENT_EXTENSION_MDL_VERSION_1,
        alignof(NET_FRAGMENT_MDL),
        NetExtensionTypeFragment,
    };
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RegisterExtension(mdlExtension));

    NET_EXTENSION_PRIVATE const virtualExtension = {
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1_SIZE,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
        alignof(NET_FRAGMENT_VIRTUAL_ADDRESS),
        NetExtensionTypeFragment,
    };
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RegisterExtension(virtualExtension));

    if (m_RxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriver &&
        m_RxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver)
    {
        NET_EXTENSION_PRIVATE const returnExtension = {
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_NAME,
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_VERSION_1,
            alignof(NET_FRAGMENT_RETURN_CONTEXT),
            NetExtensionTypeFragment,
        };
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(returnExtension));
    }

    if (m_TxCapabilities.DmaCapabilities != nullptr || m_RxCapabilities.DmaCapabilities != nullptr)
    {
        NET_EXTENSION_PRIVATE const logicalExtension = {
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
            alignof(NET_FRAGMENT_LOGICAL_ADDRESS),
            NetExtensionTypeFragment,
        };
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(logicalExtension));
    }

    ULONG wakeReasonSize =
        ALIGN_UP_BY(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8) +
        ALIGN_UP_BY(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8) +
        ALIGN_UP_BY(MAX_ETHERNET_PACKET_SIZE, 8);

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = GetFxObject();

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfMemoryCreate(&objectAttributes,
            NonPagedPoolNx,
            NETADAPTERCX_TAG,
            wakeReasonSize,
            &m_WakeReasonMemory,
            nullptr));

    auto nxDevice = GetNxDeviceFromHandle(m_Device);
    nxDevice->EnqueueEvent(NxDevice::Event::RefreshAdapterList);

    return m_StartHandled.Wait();
}

_Use_decl_annotations_
void
NxAdapter::StartForClient(
    DeviceState DeviceState
)
{
    auto ntStatus = StartForClientInner(DeviceState);

    m_Flags.StartPending = false;
    m_StartHandled.Set(ntStatus);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::StartForClientInner(
    DeviceState DeviceState
)
{
    // Don't start adapters if the device is being removed
    if (DeviceState > DeviceState::Started)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NdisWdfPnpPowerEventHandler(
            GetNdisHandle(),
            NdisWdfActionPnpStart,
            NdisWdfActionPowerNone),
        "NdisWdfPnpPowerEventHandler(NdisWdfActionPnpStart) failed. NETADAPTER=%p", GetFxObject());

    m_State = AdapterState::Started;

    if (DeviceState == DeviceState::SelfManagedIoInitialized)
    {
        InitializeSelfManagedIO();
    }
    else if (DeviceState == DeviceState::Started)
    {
        InitializeSelfManagedIO();
        PnPStartComplete();
    }

    return STATUS_SUCCESS;
}

void
NxAdapter::ClientStop(
    void
)
{
    NT_ASSERT(m_Flags.StopPending == false);

    m_Flags.StopPending = true;

    auto nxDevice = GetNxDeviceFromHandle(m_Device);
    nxDevice->EnqueueEvent(NxDevice::Event::RefreshAdapterList);

    m_StopHandled.Wait();

    NT_ASSERT(m_State >= AdapterState::Stopped);
}

void
NxAdapter::StopPhase1(
    void
)
/*++

Routine description:

    This function calls into NDIS to perform phase 1 of miniport
    removal. This is either called as a result of a NetAdapterStop
    or because the device is being removed.

--*/
{
    if (m_State >= AdapterState::Started &&
        m_State < AdapterState::Stopping)
    {
        LogVerbose(
            GetRecorderLog(),
            FLAG_ADAPTER,
            "Calling NdisWdfPnpPowerEventHandler(PreRelease). NETADAPTER=%p", GetFxObject());

        NdisWdfPnpPowerEventHandler(
            GetNdisHandle(),
            NdisWdfActionPreReleaseHardware,
            NdisWdfActionPowerNone);

        m_State = AdapterState::Stopping;
    }
}

void
NxAdapter::StopPhase2(
    void
)
/*++

Routine description:

    This function calls into NDIS to perform phase 2 of miniport
    removal. This is either called as a result of a NetAdapterStop
    or because the device is being removed.

--*/
{
    if (m_State >= AdapterState::Started &&
        m_State < AdapterState::Stopped)
    {
        LogVerbose(
            GetRecorderLog(),
            FLAG_ADAPTER,
            "Calling NdisWdfPnpPowerEventHandler(PostRelease). NETADAPTER=%p", GetFxObject());

        NdisWdfPnpPowerEventHandler(
            GetNdisHandle(),
            NdisWdfActionPostReleaseHardware,
            NdisWdfActionPowerNone);

        m_State = AdapterState::Stopped;
    }
}

_Use_decl_annotations_
void
NxAdapter::FullStop(
    DeviceState DeviceState
)
/*++

Routine description:

    This function takes whatever steps are necessary to bring an
    adapter to a halt based on the current device state. Only
    called as a result of a NetAdapterStop or a framework failure.

--*/
{
    // Play the needed events based on the current device state
    switch (DeviceState)
    {
        case DeviceState::SelfManagedIoInitialized:
            __fallthrough;

        case DeviceState::Started:
            SuspendSelfManagedIo(
                PowerActionNone,
                PowerSystemUnspecified,
                PowerDeviceUnspecified);
            __fallthrough;

        case DeviceState::Initialized:
            __fallthrough;

        case DeviceState::ReleasingPhase1Pending:
            StopPhase1();
            __fallthrough;

        case DeviceState::ReleasingPhase2Pending:
            StopPhase2();
            __fallthrough;

        case DeviceState::Released:
            __fallthrough;

        case DeviceState::Removed:
            // Adapter should already be stopped
            NT_ASSERT(m_State == AdapterState::Stopped);
            break;
    }
}

_Use_decl_annotations_
void
NxAdapter::StopForClient(
    DeviceState DeviceState
)
/*++

Routine description:

    This function fully stops a miniport before returning. This
    is only called as a result of a NetAdapterStop.

--*/
{
    FullStop(DeviceState);

    m_Flags.StopPending = false;
    m_StopHandled.Set();
}

void
NxAdapter::ReportSurpriseRemove(
    void
)
{
    if (m_State == AdapterState::Started)
    {
        LogVerbose(GetRecorderLog(), FLAG_ADAPTER, "Calling NdisWdfPnpPowerEventHandler(SurpriseRemove). NETADAPTER=%p", GetFxObject());

        NdisWdfPnpPowerEventHandler(
            GetNdisHandle(),
            NdisWdfActionPnpSurpriseRemove,
            NdisWdfActionPowerNone);
    }
}

_Use_decl_annotations_
void
NxAdapter::SetDeviceFailed(
    NTSTATUS Status
)
{
    LogError(GetRecorderLog(), FLAG_ADAPTER,
        "Translator initiated WdfDeviceSetFailed: %!STATUS!",
        Status);

    WdfDeviceSetFailed(m_Device, WdfDeviceFailedNoRestart);
}

_Use_decl_annotations_
void
NxAdapter::GetProperties(
    NET_CLIENT_ADAPTER_PROPERTIES * Properties
) const
{
    Properties->MediaType = m_MediaType;
    Properties->NetLuid = m_NetLuid;
    Properties->DriverIsVerifying = !!MmIsDriverVerifyingByAddress(m_datapathCallbacks.EvtAdapterCreateTxQueue);
    Properties->NdisAdapterHandle = m_NdisAdapterHandle;

    // C++ object shared across ABI
    // this is not viable once the translator is removed from the Cx
    Properties->NblDispatcher = const_cast<INxNblDispatcher *>(static_cast<const INxNblDispatcher *>(&m_NblDatapath));
}

_Use_decl_annotations_
void
NxAdapter::GetDatapathCapabilities(
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES * DatapathCapabilities
) const
{
    RtlZeroMemory(DatapathCapabilities, sizeof(*DatapathCapabilities));

    auto const & txCap = GetTxCapabilities();
    auto const & rxCap = GetRxCapabilities();

    if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeDriver) &&
        (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver))
    {
        DatapathCapabilities->RxMemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER;
    }
    else if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem) &&
             (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeSystem))
    {
        DatapathCapabilities->RxMemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH;
    }
    else if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem) &&
             (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver))
    {
        DatapathCapabilities->RxMemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE;
    }
    else
    {
        NT_FRE_ASSERT(false);
    }

    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES lsoHardwareCapabilities = {};
    m_NxOffloadManager->GetLsoHardwareCapabilities(&lsoHardwareCapabilities);

    size_t const maxL2HeaderSize =
        m_MediaType == NdisMedium802_3
            ? 22 // sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER)
            : 0;

    if (lsoHardwareCapabilities.MaximumOffloadSize > 0)
    {
        //
        // MaximumTxFragmentSize = Max LSO offload size + Max TCP header size + Max IP header size + Ethernet overhead
        //
        DatapathCapabilities->MaximumTxFragmentSize = txCap.PayloadBackfill + maxL2HeaderSize + lsoHardwareCapabilities.MaximumOffloadSize + MAX_IP_HEADER_SIZE + MAX_TCP_HEADER_SIZE;
    }
    else
    {
        DatapathCapabilities->MaximumTxFragmentSize = txCap.PayloadBackfill + maxL2HeaderSize + GetMtuSize();
    }

    DatapathCapabilities->MaximumRxFragmentSize = rxCap.MaximumFrameSize;
    DatapathCapabilities->MaximumNumberOfTxQueues = txCap.MaximumNumberOfQueues;
    DatapathCapabilities->MaximumNumberOfRxQueues = rxCap.MaximumNumberOfQueues;
    DatapathCapabilities->PreferredTxFragmentRingSize = txCap.FragmentRingNumberOfElementsHint;
    DatapathCapabilities->PreferredRxFragmentRingSize = rxCap.FragmentRingNumberOfElementsHint;
    DatapathCapabilities->MaximumNumberOfTxFragments = txCap.MaximumNumberOfFragments;
    DatapathCapabilities->TxPayloadBackfill = txCap.PayloadBackfill;

    if (rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem)
    {
        if (rxCap.MappingRequirement == NetMemoryMappingRequirementNone)
        {
            DatapathCapabilities->RxMemoryConstraints.MappingRequirement = NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_NONE;
        }
        else if (rxCap.MappingRequirement == NetMemoryMappingRequirementDmaMapped)
        {
            DatapathCapabilities->RxMemoryConstraints.MappingRequirement = NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED;
        }
        else
        {
            NT_FRE_ASSERT(false);
        }
    }

    DatapathCapabilities->TxMemoryConstraints.MappingRequirement = static_cast<NET_CLIENT_MEMORY_MAPPING_REQUIREMENT>(txCap.MappingRequirement);
    DatapathCapabilities->TxMemoryConstraints.AlignmentRequirement = txCap.FragmentBufferAlignment;

    DatapathCapabilities->NominalMaxTxLinkSpeed = m_LinkLayerCapabilities.MaxTxLinkSpeed;
    DatapathCapabilities->NominalMaxRxLinkSpeed = m_LinkLayerCapabilities.MaxRxLinkSpeed;
    DatapathCapabilities->NominalMtu = m_MtuSize;
    DatapathCapabilities->MtuWithLso = m_MtuSize;
    DatapathCapabilities->MtuWithRsc = m_MtuSize;

    DatapathCapabilities->RxMemoryConstraints.MappingRequirement = static_cast<NET_CLIENT_MEMORY_MAPPING_REQUIREMENT> (rxCap.MappingRequirement);
    DatapathCapabilities->RxMemoryConstraints.AlignmentRequirement = rxCap.FragmentBufferAlignment;

    if (DatapathCapabilities->RxMemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED)
    {
        DatapathCapabilities->RxMemoryConstraints.Dma.CacheEnabled =
            static_cast<NET_CLIENT_TRI_STATE> (rxCap.DmaCapabilities->CacheEnabled);
        DatapathCapabilities->RxMemoryConstraints.Dma.MaximumPhysicalAddress = rxCap.DmaCapabilities->MaximumPhysicalAddress;
        DatapathCapabilities->RxMemoryConstraints.Dma.PreferredNode = rxCap.DmaCapabilities->PreferredNode;

        DatapathCapabilities->RxMemoryConstraints.Dma.DmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(rxCap.DmaCapabilities->DmaEnabler,
                                          WdfDmaDirectionReadFromDevice);
    }

    if (DatapathCapabilities->TxMemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED)
    {
        DatapathCapabilities->TxMemoryConstraints.Dma.CacheEnabled =
            static_cast<NET_CLIENT_TRI_STATE> (txCap.DmaCapabilities->CacheEnabled);
        DatapathCapabilities->TxMemoryConstraints.Dma.MaximumPhysicalAddress = txCap.DmaCapabilities->MaximumPhysicalAddress;
        DatapathCapabilities->TxMemoryConstraints.Dma.PreferredNode = txCap.DmaCapabilities->PreferredNode;

        DatapathCapabilities->TxMemoryConstraints.Dma.DmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(txCap.DmaCapabilities->DmaEnabler,
                WdfDmaDirectionWriteToDevice);

        // We also need the PDO to let buffer manager call DMA APIs
        WDFDEVICE device = GetDevice();
        auto physicalDeviceObject = WdfDeviceWdmGetPhysicalDevice(device);

        DatapathCapabilities->TxMemoryConstraints.Dma.PhysicalDeviceObject = physicalDeviceObject;
    }

#if defined(_X86_) || defined(_AMD64_) || defined(_ARM64_)
    DatapathCapabilities->FlushBuffers = FALSE;
#else
    DatapathCapabilities->FlushBuffers = TRUE;
#endif

}

_Use_decl_annotations_
void
NxAdapter::GetReceiveScalingCapabilities(
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES * Capabilities
) const
{
    *Capabilities = {
        m_ReceiveScalingCapabilities.NumberOfQueues,
        m_ReceiveScalingCapabilities.IndirectionTableSize,
    };
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::AddNetClientPacketExtensionsToQueueCreationContext(
    NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    QUEUE_CREATION_CONTEXT& QueueCreationContext
)
{
    //
    // convert queue config's NET_CLIENT_EXTENSION to NET_EXTENSION_PRIVATE
    // since we passed API boundary, CX should use private throughout.
    // CX needs to use the stored extension within the adapter object at this point
    // so that the object holding info about packet extension is owned by CX.
    //
    for (size_t i = 0; i < ClientQueueConfig->NumberOfExtensions; i++)
    {
        auto const extension = QueryExtensionLocked(
            ClientQueueConfig->Extensions[i].Name,
            ClientQueueConfig->Extensions[i].Version,
            ClientQueueConfig->Extensions[i].Type);

        // protocol will ask for any extensions it supports but if
        // the driver has not advertised capability it will not be
        // registered and we do not add it to the creation context
        if (extension)
        {
            CX_RETURN_NTSTATUS_IF(
                STATUS_INSUFFICIENT_RESOURCES,
                ! QueueCreationContext.Extensions.append(*extension));
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::CreateTxQueue(
    void * ClientQueue,
    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    NET_CLIENT_QUEUE * AdapterQueue,
    NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch
)
{
    auto lock = wil::acquire_wdf_wait_lock(m_DataPathControlLock);

    ULONG QueueId;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AllocateQueueId(m_txQueues, QueueId),
        "Failed to allocate QueueId.");

    QUEUE_CREATION_CONTEXT context;

    context.CurrentThread = KeGetCurrentThread();
    context.ClientQueueConfig = ClientQueueConfig;
    context.ClientDispatch = ClientDispatch;
    context.AdapterDispatch = AdapterDispatch;
    context.Adapter = this;
    context.QueueId = QueueId;
    context.ClientQueue = ClientQueue;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AddNetClientPacketExtensionsToQueueCreationContext(ClientQueueConfig, context),
        "Failed to transfer NetClientQueueConfig's packet extension to queue_creation_context");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_datapathCallbacks.EvtAdapterCreateTxQueue(GetFxObject(), reinterpret_cast<NETTXQUEUE_INIT*>(&context)),
        "Client failed tx queue creation. NETADAPTER=%p", GetFxObject());

    CX_RETURN_NTSTATUS_IF(
        STATUS_UNSUCCESSFUL,
        ! context.CreatedQueueObject);

    m_txQueues[QueueId] = wistd::move(context.CreatedQueueObject);
    *AdapterQueue = reinterpret_cast<NET_CLIENT_QUEUE>(
        GetTxQueueFromHandle((NETPACKETQUEUE)m_txQueues[QueueId].get()));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::CreateRxQueue(
    void * ClientQueue,
    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    NET_CLIENT_QUEUE * AdapterQueue,
    NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch
)
{
    auto lock = wil::acquire_wdf_wait_lock(m_DataPathControlLock);

    ULONG QueueId;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AllocateQueueId(m_rxQueues, QueueId),
        "Failed to allocate QueueId.");

    QUEUE_CREATION_CONTEXT context;

    context.CurrentThread = KeGetCurrentThread();
    context.ClientQueueConfig = ClientQueueConfig;
    context.ClientDispatch = ClientDispatch;
    context.AdapterDispatch = AdapterDispatch;
    context.Adapter = this;
    context.QueueId = QueueId;
    context.ClientQueue = ClientQueue;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AddNetClientPacketExtensionsToQueueCreationContext(ClientQueueConfig, context),
        "Failed to transfer NetClientQueueConfig's packet extension to queue_creation_context");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_datapathCallbacks.EvtAdapterCreateRxQueue(GetFxObject(), reinterpret_cast<NETRXQUEUE_INIT*>(&context)),
        "Client failed rx queue creation. NETADAPTER=%p", GetFxObject());

    CX_RETURN_NTSTATUS_IF(
        STATUS_UNSUCCESSFUL,
        ! context.CreatedQueueObject);

    m_rxQueues[QueueId] = wistd::move(context.CreatedQueueObject);
    *AdapterQueue = reinterpret_cast<NET_CLIENT_QUEUE>(
        GetRxQueueFromHandle((NETPACKETQUEUE)m_rxQueues[QueueId].get()));

    return STATUS_SUCCESS;
}

inline
void
NxAdapter::DestroyQueue(
    NxQueue * Queue
)
{
    auto lock = wil::acquire_wdf_wait_lock(m_DataPathControlLock);

    switch (Queue->m_queueType)
    {
    case NxQueue::Type::Rx:
        m_rxQueues[Queue->m_queueId].reset();
        break;

    case NxQueue::Type::Tx:
        m_txQueues[Queue->m_queueId].reset();
        break;
    }
}

void
NxAdapter::DestroyQueue(
    NET_CLIENT_QUEUE AdapterQueue
)
{
    DestroyQueue(reinterpret_cast<NxQueue *>(AdapterQueue));
}

_Use_decl_annotations_
NONPAGEDX
void
NxAdapter::ReturnRxBuffer(
    NET_FRAGMENT_RETURN_CONTEXT_HANDLE RxBufferReturnContext
)
{
    m_EvtReturnRxBuffer(GetFxObject(), RxBufferReturnContext);
}

void
NxAdapter::_EvtCleanup(
    _In_  WDFOBJECT NetAdapter
)
/*++
Routine Description:
    A WDF Event Callback

    This routine is called when the client driver WDFDEVICE object is being
    deleted. This happens when WDF is processing the REMOVE irp for the client.

--*/
{
    auto nxAdapter = GetNxAdapterFromHandle((NETADAPTER)NetAdapter);

    //
    // If the adapter has been disposed the workitem for async power reference
    // would have been disposed as well. Enqueueing it will be a no-op.
    // In case there is a thread in NDIS waiting for the power complete notification
    // unblock it.
    //
    if (nxAdapter->m_PowerRequiredWorkItemPending) {
        NdisWdfAsyncPowerReferenceCompleteNotification(nxAdapter->m_NdisAdapterHandle,
            STATUS_INVALID_DEVICE_STATE, FALSE);
        nxAdapter->m_PowerRequiredWorkItemPending = FALSE;
    }

    if (nxAdapter->m_AoAcDisengageWorkItemPending) {
        NdisWdfAsyncPowerReferenceCompleteNotification(nxAdapter->m_NdisAdapterHandle,
            STATUS_INVALID_DEVICE_STATE, TRUE);
        nxAdapter->m_AoAcDisengageWorkItemPending = FALSE;
    }

    if (nxAdapter->m_DefaultRequestQueue) {
        WdfObjectDereferenceWithTag(nxAdapter->m_DefaultRequestQueue->GetFxObject(),
            (void *)NxAdapter::_EvtCleanup);
        nxAdapter->m_DefaultRequestQueue = NULL;
    }

    if (nxAdapter->m_DefaultDirectRequestQueue) {
        WdfObjectDereferenceWithTag(nxAdapter->m_DefaultDirectRequestQueue->GetFxObject(),
            (void *)NxAdapter::_EvtCleanup);
        nxAdapter->m_DefaultDirectRequestQueue = NULL;
    }

    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    nxDevice->AdapterDestroyed(nxAdapter);

    nxAdapter->EnqueueEvent(NxAdapter::Event::EvtCleanup);

    return;
}

_Use_decl_annotations_
NDIS_HANDLE
EvtNdisWdmDeviceGetNdisAdapterHandle(
    DEVICE_OBJECT * DeviceObject
)
/*++
Routine Description:
    A Ndis Event Callback

    This event callback converts a WDF Device Object to the NDIS's
    miniport Adapter handle.
--*/
{
    // It does not make sense to get an adapter handle from a DEVICE_OBJECT.

    UNREFERENCED_PARAMETER(DeviceObject);
    return nullptr;
}

_Use_decl_annotations_
void
EvtNdisUpdatePMParameters(
    NDIS_HANDLE Adapter,
    NDIS_PM_PARAMETERS * PMParameters
)
/*++
Routine Description:




    This routine is called whenever the Miniport->PMCurrentParameters structure
    maintained by Ndis.sys is updated.

    This routine updates its copy of the PMParameters.
    It also evaluates if there has been a change in the SxWake settings and
    calls WdfDeviceAssignSxWakeSettings accordingly

Arguments

    PMParameters - a pointer to NDIS_PM_PARAMETERS structure that has the updated
        power manangement parameters.

--*/
{
    UNREFERENCED_PARAMETER(PMParameters);
    UNREFERENCED_PARAMETER(Adapter);
}

RECORDER_LOG
NxAdapter::GetRecorderLog(
    void
)
{
    return m_NxDriver->GetRecorderLog();
}

WDFDEVICE
NxAdapter::GetDevice(
    void
) const
{
    return m_Device;
}

NTSTATUS
NxAdapter::NdisInitialize(
    void
)
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to initialize a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called ndisPnPStartDevice routine.

Arguments:

    NdisAdapterHandle - Opaque Ndis handle for the Adapter

Return Value:

    NDIS_STATUS_xxx code

--*/
{
    NTSTATUS status;

    status = SetRegistrationAttributes();
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    status = SetGeneralAttributes();
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    //
    // Register packet extensions for offloads which are advertised by the client driver
    //
    m_NxOffloadManager->RegisterExtensions();

    for (auto const &app : m_Apps)
    {
        if (m_ClientDispatch->OffloadInitialize)
        {
            status = m_ClientDispatch->OffloadInitialize(app.get());

            if (status != STATUS_SUCCESS)
            {
                goto Exit;
            }
        }
    }

Exit:
    //
    // Inform the device (state machine) that the adapter initialization is
    // complete. It will process the initialization status to handle device
    // failure appropriately.
    //
    GetNxDeviceFromHandle(m_Device)->AdapterInitialized();

    EnqueueEvent(NxAdapter::Event::NdisMiniportInitializeEx);

    m_TransitionComplete.Wait();

    return status;
}

_Use_decl_annotations_
void
NxAdapter::NdisHalt()
{
    ClearGeneralAttributes();

    // We can change these values here because NetAdapterStop only returns after m_IsHalted is set
    m_Flags.StartCalled = FALSE;
    m_Flags.GeneralAttributesSet = FALSE;

    m_packetExtensions.resize(0);

    //
    // Report the adapter halt to the device
    //
    GetNxDeviceFromHandle(m_Device)->AdapterHalted();
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::AdapterPause()
{
    NdisMPauseComplete(m_NdisAdapterHandle);

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathCreate()
{
    for (auto &app : m_Apps)
    {
        m_ClientDispatch->CreateDatapath(app.get());
    }

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathDestroy(
    void
)
{
    for (auto &app : m_Apps)
    {
        m_ClientDispatch->DestroyDatapath(app.get());
    }

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStart(
    void
)
{
    for (auto &app : m_Apps)
    {
        m_ClientDispatch->StartDatapath(app.get());
    }

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStop(
    void
)
{
    for (auto &app : m_Apps)
    {
        m_ClientDispatch->StopDatapath(app.get());
    }

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStartRestartComplete()
{
    (void)DatapathStart();

    return DatapathRestartComplete();
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStopPauseComplete()
{
    (void)DatapathStop();

    return DatapathPauseComplete();
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathPauseComplete()
{
    NdisMPauseComplete(m_NdisAdapterHandle);

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathRestartComplete()
{
    NdisMRestartComplete(m_NdisAdapterHandle,
        NdisConvertNtStatusToNdisStatus(STATUS_SUCCESS));

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
void
NxAdapter::AsyncTransitionComplete()
{
    m_TransitionComplete.Set();
}

void
NxAdapter::PnPStartComplete(
    void
)
{
    if (m_State == AdapterState::Started)
    {
        NdisWdfMiniportStarted(GetNdisHandle());
    }
}

void
NxAdapter::InitializeSelfManagedIO(
    void
)
/*++
Routine Description:
    This is called by the NxDevice to inform the adapter that self managed IO
    initialize callback for the device has been invoked.
--*/
{
    if (m_State == AdapterState::Started)
    {
        CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            InitializePowerManagement(),
            "Failed to initialize power management for this adapter. NETADAPTER=%p", GetFxObject());

        EnqueueEvent(NxAdapter::Event::AdapterStart);
        m_TransitionComplete.Wait();

        NdisWdfMiniportDataPathStart(m_NdisAdapterHandle);
    }
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::InitializePowerManagement(
    void
)
/*
Routine Description:

    Called after D0Entry to manage the WDFDEVICE power references based on
    AOAC and SS support.

    When SS and AOAC structures are initialized in NDIS, they are both initialized
    with stop flags so that SS/AoAC engines dont try to drop/acquire power refs
    until the WDFDEVICE is ready. Now that we're ready this routine will clear
    those SS/AoAC stop flags by calling into NdisWdfActionStartPowerManagement.

    This routine ensures this adapter will have one (and only one) WDFDEVICE power
    reference. This is done to account for:
    - Cases where both AOAC and SS are enabled
    - One of them is enabled.
    - Neither of them are enabled.

Remarks:

    We already validated the device is the power policy owner earlier.

--*/
{
    auto nxDevice = GetNxDeviceFromHandle(GetDevice());

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        nxDevice->PowerReference(
            FALSE,
            PTR_TO_TAG(EvtNdisInitializeEx)),
        "Failed to acquire a power reference on the parent device");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NdisWdfPnpPowerEventHandler(GetNdisHandle(),
            NdisWdfActionStartPowerManagement, NdisWdfActionPowerNone),
        "NdisWdfActionStartPowerManagement failed. NETADAPTER=%p", GetFxObject());

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxAdapter::SuspendSelfManagedIo(
    POWER_ACTION SystemPowerAction,
    SYSTEM_POWER_STATE TargetSystemPowerState,
    DEVICE_POWER_STATE TargetDevicePowerState
)
/*++
Routine Description:
    This is called by the NxDevice to inform the adapter that self managed IO
    suspend callback for the device has been invoked.
--*/
{
    if (m_State == AdapterState::Started)
    {
        // If m_State is Started the adapter state machine is
        // in a DatapathRunning or DatapathPaused state.

        if (TargetDevicePowerState > PowerDeviceD0)
        {
            NdisWdfMiniportSetPower(
                GetNdisHandle(),
                SystemPowerAction,
                TargetSystemPowerState,
                TargetDevicePowerState);

            EnqueueEvent(NxAdapter::Event::DatapathStop);
        }
        else
        {

            // NdisWdfMiniportDataPathPause is synchronous and results in the
            // Event::NdisMiniportPause event being queued.
            //
            // If we are DatapathRunning this means we end up transitioning to
            // DatapathPaused and if we are already DatapathPaused
            // NdisWdfMiniportDataPathPause is a noop.
            //
            // From either DatapathRunning or DatapathPaused this means we
            // end up transitioning StoppingPaused.

            NdisWdfMiniportDataPathPause(GetNdisHandle());
            EnqueueEvent(NxAdapter::Event::AdapterStop);
        }

        m_TransitionComplete.Wait();
    }
}

_Use_decl_annotations_
void
NxAdapter::RestartSelfManagedIo(
    POWER_ACTION SystemPowerAction,
    SYSTEM_POWER_STATE TargetSystemPowerState,
    DEVICE_POWER_STATE TargetDevicePowerState
)
/*++
Routine Description:
    This is called by the NxDevice to inform the adapter that self managed IO
    restart callback for the device has been invoked.
--*/
{
    if (m_State == AdapterState::Started)
    {
        //
        // Cx guarantees client's SMIORestart callback will be called before the
        // data path has started. At this point client's SMIORestart callback
        // should've already been invoked by the NxDevice.
        //
        EnqueueEvent(NxAdapter::Event::DatapathStart);

        m_TransitionComplete.Wait();

        NdisWdfMiniportSetPower(
            GetNdisHandle(),
            SystemPowerAction,
            TargetSystemPowerState,
            TargetDevicePowerState);
    }
}

bool
NxAdapter::HasPermanentLinkLayerAddress()
{
    return (m_PermanentLinkLayerAddress.Length != 0);
}

bool
NxAdapter::HasCurrentLinkLayerAddress()
{
    return (m_CurrentLinkLayerAddress.Length != 0);
}

_Use_decl_annotations_
void
NxAdapter::EvtLogTransition(
    _In_ SmFx::TransitionType TransitionType,
    _In_ StateId SourceState,
    _In_ EventId ProcessedEvent,
    _In_ StateId TargetState
)
{
    UNREFERENCED_PARAMETER(TransitionType);

    // StateMachineContext is NxAdapter object;
    WDFDEVICE wdfDevice = GetDevice();
    NxDevice* nxDevice = GetNxDeviceFromHandle(wdfDevice);

    TraceLoggingWrite(
        g_hNetAdapterCxEtwProvider,
        "NxAdapterStateTransition",
        TraceLoggingDescription("NxAdapterStateTransition"),
        TraceLoggingKeyword(NET_ADAPTER_CX_NXADAPTER_STATE_TRANSITION),
        TraceLoggingUInt32(nxDevice->GetDeviceBusAddress(), "DeviceBusAddress"),
        TraceLoggingHexUInt64(GetNetLuid().Value, "AdapterNetLuid"),
        TraceLoggingHexInt32(static_cast<INT32>(SourceState), "AdapterStateTransitionFrom"),
        TraceLoggingHexInt32(static_cast<INT32>(ProcessedEvent), "AdapterStateTransitionEvent"),
        TraceLoggingHexInt32(static_cast<INT32>(TargetState), "AdapterStateTransitionTo"));
}

_Use_decl_annotations_
void
NxAdapter::EvtLogMachineException(
    _In_ SmFx::MachineException exception,
    _In_ EventId relevantEvent,
    _In_ StateId relevantState
)
{
    UNREFERENCED_PARAMETER((exception, relevantEvent, relevantState));

    DbgBreakPoint();
}

_Use_decl_annotations_
void
NxAdapter::EvtMachineDestroyed()
{
}

_Use_decl_annotations_
void
NxAdapter::EvtLogEventEnqueue(
    _In_ EventId relevantEvent
)
{
    UNREFERENCED_PARAMETER(relevantEvent);
}

_Use_decl_annotations_
NET_EXTENSION_PRIVATE const *
NxAdapter::QueryExtension(
    PCWSTR Name,
    ULONG Version,
    NET_EXTENSION_TYPE Type
) const
{
    for (auto const & extension : m_packetExtensions)
    {
        if (0 == wcscmp(Name, extension.m_extension.Name) &&
            Version == extension.m_extension.Version &&
            Type == extension.m_extension.Type)
        {
            return &extension.m_extension;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
NET_EXTENSION_PRIVATE const *
NxAdapter::QueryExtensionLocked(
    PCWSTR Name,
    ULONG Version,
    NET_EXTENSION_TYPE Type
) const
{
    auto locked = wil::acquire_wdf_wait_lock(m_ExtensionCollectionLock);

    return QueryExtension(Name, Version, Type);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::RegisterExtension(
    NET_EXTENSION_PRIVATE const & Extension
)
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_DUPLICATE_NAME,
        QueryExtension(Extension.Name, Extension.Version, Extension.Type) != nullptr);

    NxExtension extension;
    CX_RETURN_IF_NOT_NT_SUCCESS(
        extension.Initialize(&Extension));

    auto locked = wil::acquire_wdf_wait_lock(m_ExtensionCollectionLock);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_packetExtensions.append(wistd::move(extension)));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::ReceiveScalingEnable(
    _In_ NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE HashType,
    _In_ NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE ProtocolType
)
{

    return m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingEnable(
        GetFxObject(),
        HashType,
        ProtocolType);
}

void
NxAdapter::ReceiveScalingDisable(
    void
)
{
    m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingDisable(
        GetFxObject());
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::ReceiveScalingSetIndirectionEntries(
    _Inout_ NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES * IndirectionEntries
)
{
    auto table = reinterpret_cast<NET_ADAPTER_RECEIVE_SCALING_INDIRECTION_ENTRIES *>(IndirectionEntries);

    for (size_t i = 0; i < IndirectionEntries->Length; i++)
    {
        auto queue = reinterpret_cast<NxRxQueue *>(IndirectionEntries->Entries[i].Queue);

        table->Entries[i].PacketQueue = reinterpret_cast<NETPACKETQUEUE>(queue->GetWdfObject());
    }

    return m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingSetIndirectionEntries(
        GetFxObject(),
        table);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::ReceiveScalingSetHashSecretKey(
    _In_ NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const * HashSecretKey
)
{
    return m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingSetHashSecretKey(
        GetFxObject(),
        reinterpret_cast<NET_ADAPTER_RECEIVE_SCALING_HASH_SECRET_KEY const *>(HashSecretKey));
}

_Use_decl_annotations_
void
NxAdapter::SetDatapathCapabilities(
    NET_ADAPTER_TX_CAPABILITIES const *TxCapabilities,
    NET_ADAPTER_RX_CAPABILITIES const *RxCapabilities
)
{
    RtlCopyMemory(
        &m_TxCapabilities,
        TxCapabilities,
        TxCapabilities->Size);

    if (TxCapabilities->DmaCapabilities != nullptr)
    {
        RtlCopyMemory(
            &m_TxDmaCapabilities,
            TxCapabilities->DmaCapabilities,
            TxCapabilities->DmaCapabilities->Size);

        m_TxCapabilities.DmaCapabilities = &m_TxDmaCapabilities;
    }

    RtlCopyMemory(
        &m_RxCapabilities,
        RxCapabilities,
        RxCapabilities->Size);

    if (m_RxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriver)
    {
        m_EvtReturnRxBuffer = m_RxCapabilities.EvtAdapterReturnRxBuffer;
    }

    if (RxCapabilities->DmaCapabilities != nullptr)
    {
        RtlCopyMemory(
            &m_RxDmaCapabilities,
            RxCapabilities->DmaCapabilities,
            RxCapabilities->DmaCapabilities->Size);

        m_RxCapabilities.DmaCapabilities = &m_RxDmaCapabilities;
    }
}

const NET_ADAPTER_TX_CAPABILITIES&
NxAdapter::GetTxCapabilities(
    void
) const
{
    NT_ASSERT(0 != m_TxCapabilities.Size);
    return m_TxCapabilities;
}


const NET_ADAPTER_RX_CAPABILITIES&
NxAdapter::GetRxCapabilities(
    void
) const
{
    NT_ASSERT(0 != m_RxCapabilities.Size);
    return m_RxCapabilities;
}

_Use_decl_annotations_
void
NxAdapter::SetLinkLayerCapabilities(
    NET_ADAPTER_LINK_LAYER_CAPABILITIES const & LinkLayerCapabilities
)
{
    RtlCopyMemory(&m_LinkLayerCapabilities,
        &LinkLayerCapabilities,
        LinkLayerCapabilities.Size);
}

_Use_decl_annotations_
void
NxAdapter::SetPacketFilterCapabilities(
    NET_ADAPTER_PACKET_FILTER_CAPABILITIES const & PacketFilter
)
{
    RtlCopyMemory(&m_PacketFilter,
        &PacketFilter,
        PacketFilter.Size);
}

_Use_decl_annotations_
void
NxAdapter::SetMulticastCapabilities(
    NET_ADAPTER_MULTICAST_CAPABILITIES const & MulticastCapabilities
)
{
    RtlCopyMemory(&m_MulticastList,
        &MulticastCapabilities,
        MulticastCapabilities.Size);
}

_Use_decl_annotations_
void
NxAdapter::SetPowerOffloadArpCapabilities(
    NET_ADAPTER_POWER_OFFLOAD_ARP_CAPABILITIES const * Capabilities
)
{
    RtlCopyMemory(
        &m_powerOffloadArpCapabilities,
        Capabilities,
        Capabilities->Size);

    if (m_Flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetPowerOffloadNSCapabilities(
    NET_ADAPTER_POWER_OFFLOAD_NS_CAPABILITIES const * Capabilities
)
{
    RtlCopyMemory(
        &m_powerOffloadNSCapabilities,
        Capabilities,
        Capabilities->Size);

    if (m_Flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetWakeBitmapCapabilities(
    NET_ADAPTER_WAKE_BITMAP_CAPABILITIES const * Capabilities
)
{
    RtlCopyMemory(
        &m_wakeBitmapCapabilities,
        Capabilities,
        Capabilities->Size);

    if (m_Flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetMagicPacketCapabilities(
    NET_ADAPTER_WAKE_MAGIC_PACKET_CAPABILITIES const * Capabilities
)
{
    RtlCopyMemory(
        &m_magicPacketCapabilities,
        Capabilities,
        Capabilities->Size);

    if (m_Flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetWakeMediaChangeCapabilities(
    NET_ADAPTER_WAKE_MEDIA_CHANGE_CAPABILITIES const * Capabilities
)
{
    RtlCopyMemory(
        &m_wakeMediaChangeCapabilities,
        Capabilities,
        Capabilities->Size);

    if (m_Flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetWakePacketFilterCapabilities(
    NET_ADAPTER_WAKE_PACKET_FILTER_CAPABILITIES const * Capabilities
)
{
    RtlCopyMemory(
        &m_wakePacketFilterCapabilities,
        Capabilities,
        Capabilities->Size);

    if (m_Flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetCurrentLinkState(
    NET_ADAPTER_LINK_STATE const & CurrentLinkState
)
{
    auto const linkStateChanged =
        m_CurrentLinkState.TxLinkSpeed != CurrentLinkState.TxLinkSpeed ||
        m_CurrentLinkState.RxLinkSpeed != CurrentLinkState.RxLinkSpeed ||
        m_CurrentLinkState.MediaConnectState != CurrentLinkState.MediaConnectState ||
        m_CurrentLinkState.MediaDuplexState != CurrentLinkState.MediaDuplexState ||
        m_CurrentLinkState.SupportedPauseFunctions != CurrentLinkState.SupportedPauseFunctions ||
        m_CurrentLinkState.AutoNegotiationFlags != CurrentLinkState.AutoNegotiationFlags;

    if (linkStateChanged)
    {
        RtlCopyMemory(
            &m_CurrentLinkState,
            &CurrentLinkState,
            CurrentLinkState.Size);

        if (m_Flags.GeneralAttributesSet)
        {
            IndicateCurrentLinkStateToNdis();
        }
    }
}

void
NxAdapter::GetTriageInfo(
    void
)
{
    g_NetAdapterCxTriageBlock.NxAdapterLinkageOffset = FIELD_OFFSET(NxAdapter, m_Linkage);
}

_Use_decl_annotations_
void NxAdapter::ReportWakeReasonPacket(
    const NET_ADAPTER_WAKE_REASON_PACKET * Reason
) const
{
    Verifier_VerifyNetPowerTransition(m_NxDriver->GetPrivateGlobals(), GetNxDeviceFromHandle(m_Device));
    size_t savedPacketSize;
    auto wakePacketBuffer = WdfMemoryGetBuffer(Reason->WakePacket, &savedPacketSize);
    if (savedPacketSize > MAX_ETHERNET_PACKET_SIZE)
    {
        savedPacketSize = MAX_ETHERNET_PACKET_SIZE;
    }
    ULONG wakePacketSize = ALIGN_UP_BY(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8) + ALIGN_UP_BY(savedPacketSize, 8);
    ULONG wakeReasonSize =
        ALIGN_UP_BY(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8) +
        wakePacketSize;

    size_t reservedWakeReasonSize;
    auto wakeReason = static_cast<PNDIS_PM_WAKE_REASON>(WdfMemoryGetBuffer(m_WakeReasonMemory, &reservedWakeReasonSize));
    RtlZeroMemory(wakeReason, reservedWakeReasonSize);
    wakeReason->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    wakeReason->Header.Revision = NDIS_PM_WAKE_REASON_REVISION_1;
    wakeReason->Header.Size = NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1;
    wakeReason->WakeReason = NdisWakeReasonPacket;
    wakeReason->InfoBufferOffset = ALIGN_UP_BY(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8);
    wakeReason->InfoBufferSize = wakePacketSize;

    auto wakePacket = reinterpret_cast<PNDIS_PM_WAKE_PACKET>((PUCHAR)wakeReason + ALIGN_UP_BY(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8));
    wakePacket->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    wakePacket->Header.Revision = NDIS_PM_WAKE_PACKET_REVISION_1;
    wakePacket->Header.Size = NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1;
    wakePacket->PatternId = Reason->PatternId;
    wakePacket->OriginalPacketSize = Reason->OriginalPacketSize;
    wakePacket->SavedPacketSize = static_cast<ULONG>(savedPacketSize);
    wakePacket->SavedPacketOffset = ALIGN_UP_BY(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8);

    PUCHAR data = (PUCHAR)wakePacket + ALIGN_UP_BY(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8);
    RtlCopyMemory(data, wakePacketBuffer, savedPacketSize);

    auto wakeIndication = MakeNdisStatusIndication(
        m_NdisAdapterHandle, NDIS_STATUS_PM_WAKE_REASON, *wakeReason);
    wakeIndication.StatusBufferSize = wakeReasonSize;

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &wakeIndication);
}

_Use_decl_annotations_
void NxAdapter::ReportWakeReasonMediaChange(
    NET_IF_MEDIA_CONNECT_STATE Reason
) const
{
    Verifier_VerifyNetPowerTransition(m_NxDriver->GetPrivateGlobals(), GetNxDeviceFromHandle(m_Device));
    NDIS_PM_WAKE_REASON wakeReason;
    wakeReason.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    wakeReason.Header.Revision = NDIS_PM_WAKE_REASON_REVISION_1;
    wakeReason.Header.Size = NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1;
    switch (Reason)
    {
    case MediaConnectStateDisconnected:
        wakeReason.WakeReason = NdisWakeReasonMediaDisconnect;
        break;
    case MediaConnectStateConnected:
        wakeReason.WakeReason = NdisWakeReasonMediaConnect;
        break;
    default:
        Verifier_ReportViolation(
            m_NxDriver->GetPrivateGlobals(),
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetAdapterWakeReasonMediaChange,
            0,
            0);
    }
    wakeReason.InfoBufferOffset = 0;
    wakeReason.InfoBufferSize = 0;
    auto wakeIndication = MakeNdisStatusIndication(
        m_NdisAdapterHandle, NDIS_STATUS_PM_WAKE_REASON, wakeReason);

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &wakeIndication);
}
