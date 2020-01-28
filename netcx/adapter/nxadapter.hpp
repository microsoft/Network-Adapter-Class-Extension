// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxAdapter object.

--*/

#pragma once

#ifndef _KERNEL_MODE

#include <windows.h>
#include <wdf.h>
#include <wdfcx.h>
#include <wdfcxbase.h>

#include <WppRecorder.h>

#include "NdisUm.h"

#include "Driver.hpp"

#else

#include <KArray.h>

#include "NxXlat.hpp"
#include "NxNblDatapath.hpp"

#endif // _KERNEL_MODE

#include <preview/netadapter.h>

#include <FxObjectBase.hpp>
#include <KWaitEvent.h>
#include <smfx.h>

#include "NxAdapterStateMachine.h"

#include "NxExtension.hpp"

#include <NetClientApi.h>

#include "NxAdapterExtension.hpp"
#include "NxUtility.hpp"

#include "NxOffload.hpp"
#include "NxCollection.hpp"
#include "NxRequest.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"

#include "netadaptercx_triage.h"

#define PTR_TO_TAG(val) ((void *)(ULONG_PTR)(val))

#define MAX_IP_HEADER_SIZE 60
#define MAX_TCP_HEADER_SIZE 60
#define MAX_ETHERNET_PACKET_SIZE 1514

#pragma warning(push)
#pragma warning(disable:4201)

void
NdisWdfCxCharacteristicsInitialize(
    _Inout_ NDIS_WDF_CX_CHARACTERISTICS & Chars
);

void
NdisMiniportDriverCharacteristicsInitialize(
    _Inout_ NDIS_MINIPORT_DRIVER_CHARACTERISTICS & Characteristics
);

union AdapterFlags
{
    struct
    {
        //
        // TRUE if the client driver already called NetAdapterStart. Does not mean
        // the adapter is fully started
        //
        UINT32
            StartCalled : 1;

        //
        // TRUE if the Cx already set the NDIS general attributes
        //
        UINT32
            GeneralAttributesSet : 1;

        UINT32
            StartPending : 1;

        UINT32
            StopPending : 1;

        UINT32
            PowerReferenceAcquired : 1;

        UINT32
            Unused : 27;
    };

    UINT32
        AsInteger;

};

C_ASSERT(sizeof(AdapterFlags) == sizeof(UINT32));

#pragma warning(pop)

enum class AdapterState
{
    Initialized = 0,
    Started,
    Stopping,
    Stopped,
};

#define ADAPTER_INIT_SIGNATURE 'SIAN'

struct PAGED AdapterInit
    : PAGED_OBJECT<'nIAN'>
{
    ULONG
        InitSignature = ADAPTER_INIT_SIGNATURE;

    NETADAPTER
        CreatedAdapter = nullptr;

    //
    // Common to all types of adapters
    //
    NET_ADAPTER_DATAPATH_CALLBACKS
        DatapathCallbacks = {};

    WDF_OBJECT_ATTRIBUTES
        NetRequestAttributes = {};

    // For adapters layered on top of a WDFDEVICE
    WDFDEVICE
        Device = nullptr;

    MediaExtensionType
        MediaExtensionType = MediaExtensionType::None;

    // Used by other WDF class extensions that wish to hook NETADAPTER events
    Rtl::KArray<AdapterExtensionInit>
        AdapterExtensions;
};

AdapterInit *
GetAdapterInitFromHandle(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NETADAPTER_INIT * Handle
);

struct QUEUE_CREATION_CONTEXT;
struct NX_PRIVATE_GLOBALS;

class NxAdapter;
class NxDriver;
class NxQueue;
class NxRequestQueue;

FORCEINLINE
NxAdapter *
GetNxAdapterFromHandle(
    _In_ NETADAPTER Adapter
);


class NxAdapter
    : public CFxObject<NETADAPTER, NxAdapter, GetNxAdapterFromHandle, false>
    , public NxAdapterStateMachine<NxAdapter>
{
    friend class NxCollection<NxAdapter>;
    friend class NxAdapterCollection;

private:

    NxDriver *
        m_driver = nullptr;

    WDFDEVICE
        m_device = WDF_NO_HANDLE;

    //
    // Opaque handle returned by ndis.sys for this adapter.
    //
    NDIS_HANDLE
        m_ndisAdapterHandle = nullptr;

    AdapterFlags
        m_flags = {};

    AdapterState
        m_state = AdapterState::Initialized;

    AsyncResult
        m_startHandled;

    KAutoEvent
        m_stopHandled;

    LIST_ENTRY
        m_linkage = {};

    Rtl::KArray<wil::unique_wdf_object>
        m_rxQueues;

    Rtl::KArray<wil::unique_wdf_object>
        m_txQueues;

    Rtl::KArray<NxAdapterExtension>
        m_adapterExtensions;

    //
    // Data path lock. Prevent concurrent queue create and destroy.
    //
    WDFWAITLOCK
        m_dataPathControlLock = WDF_NO_HANDLE;

    KAutoEvent
        m_transitionComplete;

    //
    // Used to callback into NDIS when a asynchronous stop idle operation is
    // complete. When the work item callback is invoked, a synchronous call to
    // WdfDeviceStopIdle is made.
    //
    WDFWORKITEM
        m_powerRequiredWorkItem;

    //
    // Track whether the queued workitem ran.
    //
    BOOLEAN
        m_powerRequiredWorkItemPending = FALSE;

    //
    // Used to callback into NDIS when a asynchronous AoAc disengage operation
    // is complete.
    //
    WDFWORKITEM
        m_aoAcDisengageWorkItem;

    //
    // Track whether the queued workitem ran.
    //
    BOOLEAN
        m_aoAcDisengageWorkItemPending = FALSE;

    //
    // For diagnostic and verification purpose only
    //
    BOOLEAN
        m_aoAcEngaged = FALSE;

    //
    // Copy of Net LUID received in MiniportInitializeEx callback
    //
    NET_LUID
        m_netLuid = {};

    //
    // Copy of physical medium
    //
    NDIS_MEDIUM
        m_mediaType = (NDIS_MEDIUM)0xFFFFFFFF;

    NET_CLIENT_CONTROL_DISPATCH const *
        m_clientDispatch = nullptr;

#ifdef _KERNEL_MODE

    //
    // All datapath apps on the adapter
    //
    Rtl::KArray<wistd::unique_ptr<INxApp>, NonPagedPoolNx>
        m_apps;

    NxNblDatapath
        m_nblDatapath;

#endif // _KERNEL_MODE

    //
    // Adapter registered packet extensions from application/miniport driver
    //
    WDFWAITLOCK
        m_extensionCollectionLock = WDF_NO_HANDLE;

    Rtl::KArray<NxExtension>
        m_packetExtensions;

    // Receive scaling capabilities
    NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES
        m_receiveScalingCapabilities = {};

    UNICODE_STRING
        m_baseName = {};

    UNICODE_STRING
        m_instanceName = {};

    WDFMEMORY
        m_wakeReasonMemory = WDF_NO_HANDLE;

    MediaExtensionType
        m_mediaExtensionType = MediaExtensionType::None;
private:

    friend class NxAdapterStateMachine<NxAdapter>;

    // State machine events
    EvtLogTransitionFunc
        EvtLogTransition;

    EvtLogEventEnqueueFunc
        EvtLogEventEnqueue;

    EvtLogMachineExceptionFunc
        EvtLogMachineException;

    EvtMachineDestroyedFunc
        EvtMachineDestroyed;

    AsyncOperationDispatch
        AsyncTransitionComplete;

    AsyncOperationDispatch
        NdisHalt;

    SyncOperationDispatch
        DatapathCreate;

    SyncOperationDispatch
        DatapathDestroy;

    SyncOperationDispatch
        DatapathStart;

    SyncOperationDispatch
        DatapathStop;

    SyncOperationDispatch
        DatapathStartRestartComplete;

    SyncOperationDispatch
        DatapathStopPauseComplete;

    SyncOperationDispatch
        DatapathPauseComplete;

    SyncOperationDispatch
        DatapathRestartComplete;


public:

    //
    // Copy of Capabilities set by the client.
    //
    NET_ADAPTER_LINK_LAYER_CAPABILITIES
        m_linkLayerCapabilities = {};

    NET_ADAPTER_LINK_STATE
        m_currentLinkState = {};

    NET_ADAPTER_RX_CAPABILITIES
        m_rxCapabilities = {};

    NET_ADAPTER_DMA_CAPABILITIES
        m_rxDmaCapabilities = {};

    NET_ADAPTER_TX_CAPABILITIES
        m_txCapabilities = {};

    NET_ADAPTER_DMA_CAPABILITIES
        m_txDmaCapabilities = {};

    ULONG
        m_mtuSize = 0;

    NET_ADAPTER_LINK_LAYER_ADDRESS
        m_permanentLinkLayerAddress = {};

    NET_ADAPTER_LINK_LAYER_ADDRESS
        m_currentLinkLayerAddress = {};

    NET_ADAPTER_PACKET_FILTER_CAPABILITIES
        m_packetFilter = {};

    NET_ADAPTER_MULTICAST_CAPABILITIES
        m_multicastList = {};

    //
    // A copy of the datapath callbacks structure that client passed to NetAdapterCx.
    //
    NET_ADAPTER_DATAPATH_CALLBACKS
        m_datapathCallbacks = {};

    //
    // optional callback API from the driver if rx buffer allocation mode
    // is NetRxFragmentBufferAllocationModeNone
    //
    PFN_NET_ADAPTER_RETURN_RX_BUFFER 
        m_evtReturnRxBuffer = nullptr;

    //
    // Attibutes the client set to use with NETREQUEST and NETPOWERSETTINGS
    // objects. If the client didn't provide one, the Size field will be
    // zero.
    //
    WDF_OBJECT_ATTRIBUTES
        m_netRequestObjectAttributes = {};

    //
    // Pointer to the Default and Default Direct Oid Queus
    //

    NxRequestQueue *
        m_defaultRequestQueue = nullptr;

    NxRequestQueue *
        m_defaultDirectRequestQueue = nullptr;

#if 0
    //
    // List Head to track all allocated Oids
    //
    LIST_ENTRY
        m_ListHeadAllOids = {};
#endif

    NxPowerPolicy
        m_powerPolicy;

    //
    // Used when calling WDF APIs that support tag tracking
    //
    void *
        m_tag = 0;

    //
    // Pointer to instance which manages hardware offloads like
    // Checksum, Lso etc in NetAdapterCx
    //
    wistd::unique_ptr<NxOffloadManager>
        m_offloadManager;

    NTSTATUS
    CreateNdisMiniport(
        void
    );

    void
    NdisMiniportCreationComplete(
        _In_ NDIS_WDF_COMPLETE_ADD_PARAMS const & Parameters
    );

    void
    SendNetBufferLists(
        _In_ NET_BUFFER_LIST * NetBufferLists,
        _In_ NDIS_PORT_NUMBER PortNumber,
        _In_ ULONG SendFlags
    );

    void
    ReturnNetBufferLists(
        _In_ NET_BUFFER_LIST * NetBufferLists,
        _In_ ULONG ReturnFlags
    );

    bool
    NetClientOidPreProcess(
        _In_ NDIS_OID_REQUEST & Request,
        _Out_ NDIS_STATUS & NdisStatus
    ) const;

    bool
    NetClientDirectOidPreProcess(
        _In_ NDIS_OID_REQUEST & Request,
        _Out_ NDIS_STATUS & NdisStatus
    ) const;

    bool
    NetClientSynchrohousOidPreProcess(
        _In_ NDIS_OID_REQUEST & Request,
        _Out_ NDIS_STATUS & NdisStatus
    ) const;

    NTSTATUS
    ClientStart(
        void
    );

    void
    ClientStop(
        void
    );

    void
    StopPhase1(
        void
    );

    void
    StopPhase2(
        void
    );

    void
    FullStop(
        _In_ DeviceState DeviceState
    );

    void
    ReportSurpriseRemove(
        void
    );

    void
    SetDeviceFailed(
        _In_ NTSTATUS Status
    );

    void
    GetProperties(
        _Out_ NET_CLIENT_ADAPTER_PROPERTIES * Properties
    ) const;

    void
    GetDatapathCapabilities(
        _Out_ NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES * DatapathCapabilities
    ) const;

    void
    GetReceiveScalingCapabilities(
        _Out_ NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES * Capabilities
    ) const;

    NTSTATUS
    CreateTxQueue(
        _In_ void * ClientQueue,
        _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
        _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
        _Out_ NET_CLIENT_QUEUE * AdapterQueue,
        _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch
    );

    NTSTATUS
    CreateRxQueue(
        _In_ void * ClientQueue,
        _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
        _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
        _Out_ NET_CLIENT_QUEUE * AdapterQueue,
        _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch
    );

    void
    DestroyQueue(
        _In_ NET_CLIENT_QUEUE AdapterQueue
    );

    _IRQL_requires_min_(PASSIVE_LEVEL)
    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_requires_same_
    NONPAGED
    void
    ReturnRxBuffer(
        _In_ NET_FRAGMENT_RETURN_CONTEXT_HANDLE RxBufferReturnContext
    );

    NTSTATUS
    ReceiveScalingEnable(
        _In_ NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE HashType,
        _In_ NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE ProtocolType
    );

    void
    ReceiveScalingDisable(
        void
    );

    NTSTATUS
    ReceiveScalingSetIndirectionEntries(
        _Inout_ NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES * IndirectionEntries
    );

    NTSTATUS
    ReceiveScalingSetHashSecretKey(
        _In_ NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const * HashSecretKey
    );

    NTSTATUS
    PowerReference(
        _In_ bool WaitForD0
    );

    void
    PowerDereference(
        void
    );

private:

    NxAdapter(
        _In_ NX_PRIVATE_GLOBALS const & NxPrivateGlobals,
        _In_ AdapterInit const & AdapterInit,
        _In_ NETADAPTER Adapter
    );

    void
    DestroyQueue(
        _In_ NxQueue * Queue
    );

    NTSTATUS
    InitializeAdapterExtensions(
        _In_ AdapterInit const *AdapterInit
    );

    void
    StartForClient(
        _In_ DeviceState DeviceState
    );

    NTSTATUS
    StartForClientInner(
        _In_ DeviceState DeviceState
    );

    void
    StopForClient(
        _In_ DeviceState DeviceState
    );

    void
    FrameworkOidHandler(
        _In_ NDIS_OID_REQUEST * NdisOidRequest,
        _In_ DispatchContext * Context
    );

    void
    FrameworkSetOidHandler(
        _In_ NDIS_OID_REQUEST * NdisOidRequest,
        _In_ DispatchContext * Context
    );

    void
    DispatchOidRequestToAdapter(
        _In_ NDIS_OID_REQUEST * NdisOidRequest
    );

    void
    DispatchDirectOidRequestToAdapter(
        _In_ NDIS_OID_REQUEST* NdisOidRequest
    );

    ULONG
    GetCombinedMediaSpecificWakeUpEvents(
        void
    ) const;

public:

    ~NxAdapter(
        void
    );

    NX_PRIVATE_GLOBALS *
    GetPrivateGlobals(
        void
    ) const;

    NET_DRIVER_GLOBALS *
    GetPublicGlobals(
        void
    ) const;

    void
    EnqueueEventSync(
        _In_ NxAdapter::Event Event
    );

    void
    QueuePowerReferenceWorkItem(
        void
    );

    void
    EngageAoAc(
        void
    );

    NTSTATUS
    DisengageAoAc(
        _In_ bool WaitForD0
    );

    static
    NTSTATUS
    _Create(
        _In_ NX_PRIVATE_GLOBALS const & PrivateGlobals,
        _In_ AdapterInit const & AdapterInit,
        _In_opt_ WDF_OBJECT_ATTRIBUTES * ClientAttributes,
        _Out_ NxAdapter ** Adapter
    );

    NTSTATUS
    Init(
        _In_ AdapterInit const *AdapterInit
    );

    void
    PrepareForRebalance(
        void
    );

    NDIS_HANDLE
    GetNdisHandle(
        void
    ) const;

    void
    Refresh(
        _In_ DeviceState DeviceState
    );

    static
    void
    _EvtCleanup(
        _In_ WDFOBJECT NetAdatper
    );

    NTSTATUS
    NdisInitialize(
        void
    );

    RECORDER_LOG
    GetRecorderLog(
        void
    );

    WDFDEVICE
    GetDevice(
        void
    ) const;

    static
    void
    GetTriageInfo(
        void
    );

    static
    void
    _EvtStopIdleWorkItem(
        _In_ WDFWORKITEM WorkItem
    );

    NTSTATUS
    InitializeDatapath(
        void
    );

    NET_LUID
    GetNetLuid(
        void
    );

    UNICODE_STRING const &
    GetBaseName(
        void
    ) const;

    bool
    StartCalled(
        void
    ) const;

    NTSTATUS
    SetRegistrationAttributes(
        void
    );

    NTSTATUS
    SetGeneralAttributes(
        void
    );

    void
    SetPermanentLinkLayerAddress(
        _In_ NET_ADAPTER_LINK_LAYER_ADDRESS * LinkLayerAddress
    );

    void
    SetCurrentLinkLayerAddress(
        _In_ NET_ADAPTER_LINK_LAYER_ADDRESS * LinkLayerAddress
    );

    void
    SetDatapathCapabilities(
        _In_ NET_ADAPTER_TX_CAPABILITIES const * TxCapabilities,
        _In_ NET_ADAPTER_RX_CAPABILITIES const * RxCapabilities
    );

    void
    SetReceiveScalingCapabilities(
        _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const & Capabilities
    );

    void
    SetLinkLayerCapabilities(
        _In_ NET_ADAPTER_LINK_LAYER_CAPABILITIES const & LinkLayerCapabilities
    );

    void
    SetPacketFilterCapabilities(
        _In_ NET_ADAPTER_PACKET_FILTER_CAPABILITIES const & PacketFilter
    );

    void
    SetMulticastCapabilities(
        _In_ NET_ADAPTER_MULTICAST_CAPABILITIES const & MulticastList
    );

    void
    SetPowerOffloadArpCapabilities(
        _In_ NET_ADAPTER_POWER_OFFLOAD_ARP_CAPABILITIES const * Capabilities
    );

    void
    SetPowerOffloadNSCapabilities(
        _In_ NET_ADAPTER_POWER_OFFLOAD_NS_CAPABILITIES const * Capabilities
    );

    void
    SetWakeBitmapCapabilities(
        _In_ NET_ADAPTER_WAKE_BITMAP_CAPABILITIES const * Capabilities
    );

    void
    SetMagicPacketCapabilities(
        _In_ NET_ADAPTER_WAKE_MAGIC_PACKET_CAPABILITIES const * Capabilities
    );

    void
    SetWakeMediaChangeCapabilities(
        _In_ NET_ADAPTER_WAKE_MEDIA_CHANGE_CAPABILITIES const * Capabilities
    );

    void
    SetWakePacketFilterCapabilities(
        _In_ NET_ADAPTER_WAKE_PACKET_FILTER_CAPABILITIES const * Capabilities
    );

    void
    SetCurrentLinkState(
        _In_ NET_ADAPTER_LINK_STATE const & CurrentLinkState
    );

    void
    ClearGeneralAttributes(
        void
    );

    void
    SetMtuSize(
        _In_ ULONG mtuSize
    );

    ULONG
    GetMtuSize(
        void
    ) const;

    void
    IndicatePowerCapabilitiesToNdis(
        void
    );

    void
    IndicateCurrentLinkStateToNdis(
        void
    );

    void
    IndicateMtuSizeChangeToNdis(
        void
    );

    void
    IndicateCurrentLinkLayerAddressToNdis(
        void
    );

    void
    PnPStartComplete(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    InitializePowerManagement1(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    InitializePowerManagement2(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SelfManagedIoStart(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SelfManagedIoStop(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SelfManagedIoRestart(
        _In_ POWER_ACTION SystemPowerAction
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SelfManagedIoSuspend(
        _In_ POWER_ACTION SystemPowerAction,
        _In_ WDF_POWER_DEVICE_STATE TargetDevicePowerState
    );

    bool
    HasPermanentLinkLayerAddress(
        void
    );

    bool
    HasCurrentLinkLayerAddress(
        void
    );

    _Requires_lock_held_(m_extensionCollectionLock)
    NET_EXTENSION_PRIVATE const *
    QueryExtension(
        _In_ PCWSTR Name,
        _In_ ULONG Version,
        _In_ NET_EXTENSION_TYPE Type
    ) const;

    NET_EXTENSION_PRIVATE const *
    QueryExtensionLocked(
        _In_ PCWSTR Name,
        _In_ ULONG Version,
        _In_ NET_EXTENSION_TYPE Type
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    RegisterExtension(
        _In_ NET_EXTENSION_PRIVATE const & Extension
    );

    const NET_ADAPTER_TX_CAPABILITIES &
    GetTxCapabilities(
        void
    ) const;

    const NET_ADAPTER_RX_CAPABILITIES &
    GetRxCapabilities(
        void
    ) const;

    NTSTATUS
    AddNetClientPacketExtensionsToQueueCreationContext(
        _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
        _In_ QUEUE_CREATION_CONTEXT & QueueCreationContext
    );

    void
    DispatchOidRequest(
        _In_ NDIS_OID_REQUEST * NdisOidRequest,
        _In_ DispatchContext * Context
    );

    void
    DispatchDirectOidRequest(
        _In_ NDIS_OID_REQUEST * NdisOidRequest,
        _In_ DispatchContext * Context
    );

    NDIS_MEDIUM
    GetMediaType(
        void
    ) const;

    void ReportWakeReasonPacket(
        _In_ const NET_ADAPTER_WAKE_REASON_PACKET * Reason
    ) const;

    void ReportWakeReasonMediaChange(
        _In_ NET_IF_MEDIA_CONNECT_STATE MediaEvent
    ) const;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxAdapter, _GetNxAdapterFromHandle);

FORCEINLINE
NxAdapter *
GetNxAdapterFromHandle(
    _In_ NETADAPTER Adapter
    )
/*++
Routine Description:

    This routine is just a wrapper around the _GetNxAdapterFromHandle function.
    To be able to define a the NxAdapter class above, we need a forward declaration of the
    accessor function. Since _GetNxAdapterFromHandle is defined by Wdf, we dont want to
    assume a prototype of that function for the foward declaration.

--*/

{
    return _GetNxAdapterFromHandle(Adapter);
}

typedef struct _NX_STOP_IDLE_WORKITEM_CONTEXT
{

    BOOLEAN
        IsAoAcWorkItem;

} NX_STOP_IDLE_WORKITEM_CONTEXT, *PNX_STOP_IDLE_WORKITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    NX_STOP_IDLE_WORKITEM_CONTEXT,
    GetStopIdleWorkItemObjectContext
);

