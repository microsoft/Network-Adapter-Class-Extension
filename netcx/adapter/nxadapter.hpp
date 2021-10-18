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

#include "AdapterPnpPower.hpp"

#include "NxExtension.hpp"

#include <NetClientApi.h>

#include "NxAdapterExtension.hpp"
#include "NxUtility.hpp"

#include "NxOffload.hpp"
#include "NxAdapterCollection.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "NxQueue.hpp"
#include "rxscaling/NxRxScaling.hpp"
#include "ExecutionContext.hpp"
#include "monitor/NxPacketMonitor.hpp"

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

union AdapterFlags
{
    struct
    {
        //
        // TRUE if the Cx already set the NDIS general attributes
        //
        UINT32
            GeneralAttributesSet : 1;

        UINT32
            Unused : 31;
    };

    UINT32
        AsInteger;

};

C_ASSERT(sizeof(AdapterFlags) == sizeof(UINT32));

#pragma warning(pop)

#define ADAPTER_INIT_SIGNATURE 'SIAN'

struct PAGED AdapterInit
    : PAGED_OBJECT<'nIAN'>
{
    ULONG
        InitSignature = ADAPTER_INIT_SIGNATURE;

    WDFDEVICE
        ParentDevice = WDF_NO_HANDLE;

    MediaExtensionType
        ExtensionType = MediaExtensionType::None;

    wil::unique_wdf_any<WDFOBJECT>
        Object;

    NETADAPTER
        CreatedAdapter = WDF_NO_HANDLE;

    //
    // Common to all types of adapters
    //
    NET_ADAPTER_DATAPATH_CALLBACKS
        DatapathCallbacks = {};

    // Used by other WDF class extensions that wish to hook NETADAPTER events
    Rtl::KArray<AdapterExtensionInit>
        AdapterExtensions;

    Rtl::KArray<NET_ADAPTER_TX_DEMUX>
        TxDemux;

    bool ShouldAcquirePowerReferencesForAdapter = true;
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

class NxAdapter
    : public NxAdapterStateMachine<NxAdapter>
{
    friend class NxCollection<NxAdapter>;
    friend class NxAdapterCollection;
    friend class AdapterPnpPower;

private:

    NxPacketMonitor
        m_packetMonitor;

    //
    // The globals from the driver that invoked NetAdapterCreate
    //
    NX_PRIVATE_GLOBALS &
        m_privateGlobals;

    NTSTATUS
    QueryStandardizedKeyword(
        _In_ UNICODE_STRING const & Keyword,
        _Out_ bool & Enabled
    );

    NTSTATUS
    QueryStandardizedKeyword(
        _In_ UNICODE_STRING const & Keyword,
        _Out_ ULONG & Value
    );

    NETADAPTER
        m_adapter = WDF_NO_HANDLE;

    WDFDEVICE
        m_device = WDF_NO_HANDLE;

    void *
        m_plugPlayNotificationHandle = nullptr;

    //
    // Opaque handle returned by ndis.sys for this adapter.
    //
    NDIS_HANDLE
        m_ndisAdapterHandle = nullptr;

    AdapterFlags
        m_flags = {};

    // Tracks how many outstanding calls to NxAdapter::PowerReference
    // there are. This is kept for debugging purposes only
    LONG
        m_powerReferences = 0;

    // Tracks how many calls to NxAdapter::PowerReference were successful
    LONG
        m_successfulPowerReferences = 0;

    LIST_ENTRY
        m_linkage = {};

    Rtl::KArray<unique_packet_queue>
        m_rxQueues;

    Rtl::KArray<unique_packet_queue>
        m_txQueues;

    Rtl::KArray<NxAdapterExtension, NonPagedPoolNx>
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
    // Copy of interface guid
    //
    GUID
        m_interfaceGuid{};

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

    Rtl::KArray<NET_ADAPTER_TX_DEMUX>
        m_txDemux;

    UNICODE_STRING
        m_baseName = {};

    UNICODE_STRING
        m_instanceName = {};

    UNICODE_STRING
        m_driverImageName = {};

    WDFMEMORY
        m_wakeReasonMemory = WDF_NO_HANDLE;

    MediaExtensionType
        m_mediaExtensionType = MediaExtensionType::None;

    PFN_NET_ADAPTER_TX_PEER_DEMUX
        m_wifiTxPeerDemux = nullptr;

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

    AdapterPnpPower
        PnpPower;

    NxRxScaling
        RxScaling;

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

    NET_ADAPTER_RECEIVE_FILTER_CAPABILITIES
        m_receiveFilterCapabilities = {};

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

    NET_CLIENT_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES
        m_ieee8021qTagCapabilties = {};

    bool
        m_isOffloadPaused = false;

    EXECUTION_CONTEXT_RUNTIME_KNOBS const *
        m_executionContextKnobs = nullptr;

    NETADAPTER
    GetFxObject(
        void
    ) const;

    unique_miniport_reference
    GetMiniportReference(
        void
    ) const;

    NxAdapterExtension *
    GetExtension(
        _In_ NX_PRIVATE_GLOBALS const * ExtensionGlobals
    );

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
    GetTxDemuxConfiguration(
        _Out_ NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION * Configuration
    ) const;

    NTSTATUS
    CreateTxQueue(
        _In_ void * ClientQueue,
        _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
        _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
        _Out_ NET_CLIENT_QUEUE * AdapterQueue,
        _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch,
        _Out_ NET_EXECUTION_CONTEXT * ExecutionContext,
        _Out_ NET_EXECUTION_CONTEXT_DISPATCH const ** ExecutionContextDispatch
    );

    NTSTATUS
    CreateRxQueue(
        _In_ void * ClientQueue,
        _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
        _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
        _Out_ NET_CLIENT_QUEUE * AdapterQueue,
        _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch,
        _Out_ NET_EXECUTION_CONTEXT * ExecutionContext,
        _Out_ NET_EXECUTION_CONTEXT_DISPATCH const ** ExecutionContextDispatch
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
    PowerReference(
        _In_ bool WaitForD0
    );

    void
    PowerDereference(
        void
    );

    void
    ReleaseOutstandingPowerReferences(
        void
    );

    void
    PauseOffloads(
        void
    );

    void
    ResumeOffloads(
        void
    );

private:

    NxAdapter(
        _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
        _In_ AdapterInit const * AdapterInit
    );

    void
    DestroyQueue(
        _In_ NxQueue * Queue
    );

    NTSTATUS
    InitializeAdapterExtensions(
        _In_ AdapterInit const * AdapterInit
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

    ULONG
    GetCombinedMediaSpecificWakeUpEvents(
        void
    ) const;

    ULONG
    GetCombinedSupportedProtocolOffloads(
        void
    ) const;

public:

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

    static
    NTSTATUS
    _Create(
        _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
        _In_ AdapterInit const * AdapterInit
    );

    NTSTATUS
    Initialize(
        _In_ AdapterInit const * AdapterInit
    );

    void
    PrepareForRebalance(
        void
    );

    NDIS_HANDLE
    GetNdisHandle(
        void
    ) const;

    static
    void
    _EvtCleanup(
        _In_ WDFOBJECT NetAdatper
    );

    NTSTATUS
    NdisInitialize(
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
        _In_ NET_ADAPTER_DATAPATH_CALLBACKS const & Callbacks
    );

    const GUID *
    GetInterfaceGuid(
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

    UNICODE_STRING const &
    GetInstanceName(
        void
    ) const;

    UNICODE_STRING const &
    GetDriverImageName(
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

    NTSTATUS
    SetReceiveFilter(
        _In_ ULONG PacketFilter,
        _In_ SIZE_T MulticastAddressCount,
        _In_ IF_PHYSICAL_ADDRESS const * MulticastAddressList
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    size_t
    WifiTxPeerDemux(
        _In_ NET_EUI48_ADDRESS const * Address
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_ADAPTER_TX_DEMUX const *
    GetTxPeerAddressDemux(
        void
    ) const;

    void
    SetDatapathCapabilities(
        _In_ NET_ADAPTER_TX_CAPABILITIES const * TxCapabilities,
        _In_ NET_ADAPTER_RX_CAPABILITIES const * RxCapabilities
    );

    void
    SetLinkLayerCapabilities(
        _In_ NET_ADAPTER_LINK_LAYER_CAPABILITIES const & LinkLayerCapabilities
    );

    void
    SetReceiveFilterCapabilities(
        _In_ NET_ADAPTER_RECEIVE_FILTER_CAPABILITIES const & Capabilities
    );

    void
    SetIeee8021qTagCapabiilities(
        _In_ NET_ADAPTER_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES const & Capabilities
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
    SetEapolPacketCapabilities(
        _In_ NET_ADAPTER_WAKE_EAPOL_PACKET_CAPABILITIES const * Capabilities
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

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    InitializePowerManagement(
        void
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
    RegisterRingExtensions(
        void
    );

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

    NTSTATUS
    DispatchDirectOidRequest(
        _In_ NDIS_OID_REQUEST * NdisOidRequest,
        _In_ DispatchContext * Context
    );

    NDIS_MEDIUM
    GetMediaType(
        void
    ) const;

    void
    ReportWakeReasonPacket(
        _In_ const NET_ADAPTER_WAKE_REASON_PACKET * Reason
    ) const;

    void
    ReportWakeReasonMediaChange(
        _In_ NET_IF_MEDIA_CONNECT_STATE MediaEvent
    ) const;

    void
    WifiDestroyPeerAddressDatapath(
        _In_ size_t Demux
    );

    NxPacketMonitor &
    GetPacketMonitor(
        void
    );

    void
    GetIeee8021qHardwareCapabilities(
        NET_CLIENT_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES * HardwareCapabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UpdateNdisPmParameters(
        _Inout_ NDIS_PM_PARAMETERS * PmParameters
    );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxAdapter, GetNxAdapterFromHandle);

struct NET_RECEIVE_FILTER
{
    PKTHREAD
        CurrentThread;

    NET_PACKET_FILTER_FLAGS
        PacketFilter;

    SIZE_T
        MulticastAddressCount;

    IF_PHYSICAL_ADDRESS const *
        MulticastAddressList;
};
