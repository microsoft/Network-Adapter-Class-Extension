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

#include <netadapter.h>

#else

#include <KArray.h>

#include "NxXlat.hpp"
#include "NxNblDatapath.hpp"

#endif // _KERNEL_MODE

#include <FxObjectBase.hpp>
#include <KWaitEvent.h>
#include <smfx.h>

#include "NxAdapterStateMachine.h"

#include "NxPacketExtensionPrivate.hpp"

#include <NetClientApi.h>

#include "NxAdapterExtension.hpp"
#include "NxUtility.hpp"

#include "NxOffload.hpp"

#include "netadaptercx_triage.h"

#define PTR_TO_TAG(val) ((PVOID)(ULONG_PTR)(val))

#define MAX_IP_HEADER_SIZE 60
#define MAX_TCP_HEADER_SIZE 60

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
            Unused : 28;
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

//
// The NxAdapter is an object that represents a NetAdapterCx Client Adapter
//

FORCEINLINE
VOID
NDIS_PM_CAPABILITIES_INIT_FROM_NET_ADAPTER_POWER_CAPABILITIES(
    _Out_ PNDIS_PM_CAPABILITIES NdisPowerCaps,
    _In_ NET_ADAPTER_POWER_CAPABILITIES * NxPowerCaps
)
{

    RtlZeroMemory(NdisPowerCaps, sizeof(NDIS_PM_CAPABILITIES));

    NdisPowerCaps->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    NdisPowerCaps->Header.Size = NDIS_SIZEOF_NDIS_PM_CAPABILITIES_REVISION_2;
    NdisPowerCaps->Header.Revision = NDIS_PM_CAPABILITIES_REVISION_2;

    NdisPowerCaps->Flags = NxPowerCaps->Flags;
    NdisPowerCaps->SupportedWoLPacketPatterns = NxPowerCaps->SupportedWakePatterns;
    NdisPowerCaps->NumTotalWoLPatterns = NxPowerCaps->NumTotalWakePatterns;
    NdisPowerCaps->MaxWoLPatternSize = NxPowerCaps->MaxWakePatternSize;
    NdisPowerCaps->MaxWoLPatternOffset = NxPowerCaps->MaxWakePatternOffset;
    NdisPowerCaps->MaxWoLPacketSaveBuffer = NxPowerCaps->MaxWakePacketSaveBuffer;
    NdisPowerCaps->SupportedProtocolOffloads = NxPowerCaps->SupportedProtocolOffloads;
    NdisPowerCaps->NumArpOffloadIPv4Addresses = NxPowerCaps->NumArpOffloadIPv4Addresses;
    NdisPowerCaps->NumNSOffloadIPv6Addresses = NxPowerCaps->NumNSOffloadIPv6Addresses;
    NdisPowerCaps->SupportedWakeUpEvents = NxPowerCaps->SupportedWakeUpEvents;
    NdisPowerCaps->MediaSpecificWakeUpEvents = NxPowerCaps->SupportedMediaSpecificWakeUpEvents;

    //
    // WDF will request the D-IRPs. Doing this allows NDIS to treat this
    // device as wake capable and send appropriate PM parameters.
    //
    if (NdisPowerCaps->SupportedWoLPacketPatterns & NDIS_PM_WOL_BITMAP_PATTERN_SUPPORTED)
    {
        NdisPowerCaps->MinPatternWakeUp = NdisDeviceStateD2;
    }
    if (NdisPowerCaps->SupportedWoLPacketPatterns & NDIS_PM_WOL_MAGIC_PACKET_ENABLED)
    {
        NdisPowerCaps->MinMagicPacketWakeUp = NdisDeviceStateD2;
    }
    if (NdisPowerCaps->SupportedWakeUpEvents & NDIS_PM_WAKE_ON_MEDIA_CONNECT_SUPPORTED)
    {
        NdisPowerCaps->MinLinkChangeWakeUp = NdisDeviceStateD2;
    }
}

#define ADAPTER_INIT_SIGNATURE 'SIAN'

struct PAGED AdapterInit : PAGED_OBJECT<'nIAN'>
{
    ULONG InitSignature = ADAPTER_INIT_SIGNATURE;
    NETADAPTER CreatedAdapter = nullptr;

    //
    // Common to all types of adapters
    //
    NET_ADAPTER_DATAPATH_CALLBACKS DatapathCallbacks = {};
    WDF_OBJECT_ATTRIBUTES NetRequestAttributes = {};

    // For adapters layered on top of a WDFDEVICE
    WDFDEVICE Device = nullptr;
    WDF_OBJECT_ATTRIBUTES NetPowerSettingsAttributes = {};

    // Used by other WDF class extensions that wish to hook NETADAPTER events
    Rtl::KArray<AdapterExtensionInit> AdapterExtensions;
};

AdapterInit *
GetAdapterInitFromHandle(
    _In_ NX_PRIVATE_GLOBALS *PrivateGlobals,
    _In_ NETADAPTER_INIT * Handle
);

struct QUEUE_CREATION_CONTEXT;
struct NX_PRIVATE_GLOBALS;

class NxAdapter;
class NxDriver;
class NxQueue;
class NxRequestQueue;
class NxWake;

FORCEINLINE
NxAdapter *
GetNxAdapterFromHandle(
    _In_ NETADAPTER Adapter
);


class NxAdapter
    : public CFxObject<NETADAPTER, NxAdapter, GetNxAdapterFromHandle, false>
    , public NxAdapterStateMachine<NxAdapter>
{
    friend class NxAdapterCollection;

private:

    NxDriver *
        m_NxDriver = nullptr;

    WDFDEVICE
        m_Device = WDF_NO_HANDLE;

    //
    // Opaque handle returned by ndis.sys for this adapter.
    //
    NDIS_HANDLE
        m_NdisAdapterHandle = nullptr;

    AdapterFlags
        m_Flags = {};

    AdapterState
        m_State = AdapterState::Initialized;

    AsyncResult
        m_StartHandled;

    KAutoEvent
        m_StopHandled;

    LIST_ENTRY
        m_Linkage = {};

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
        m_DataPathControlLock = WDF_NO_HANDLE;

    KAutoEvent
        m_TransitionComplete;

    //
    // Used to callback into NDIS when a asynchronous stop idle operation is
    // complete. When the work item callback is invoked, a synchronous call to
    // WdfDeviceStopIdle is made.
    //
    WDFWORKITEM
        m_PowerRequiredWorkItem;

    //
    // Track whether the queued workitem ran.
    //
    BOOLEAN
        m_PowerRequiredWorkItemPending = FALSE;

    //
    // Used to callback into NDIS when a asynchronous AoAc disengage operation
    // is complete.
    //
    WDFWORKITEM
        m_AoAcDisengageWorkItem;

    //
    // Track whether the queued workitem ran.
    //
    BOOLEAN
        m_AoAcDisengageWorkItemPending = FALSE;

    //
    // For diagnostic and verification purpose only
    //
    BOOLEAN
        m_AoAcEngaged = FALSE;

    //
    // Copy of Net LUID received in MiniportInitializeEx callback
    //
    NET_LUID
        m_NetLuid = {};

    //
    // Copy of physical medium
    //
    NDIS_MEDIUM
        m_MediaType = (NDIS_MEDIUM)0xFFFFFFFF;

    NET_CLIENT_CONTROL_DISPATCH const *
        m_ClientDispatch = nullptr;

#ifdef _KERNEL_MODE

    //
    // All datapath apps on the adapter
    //
    Rtl::KArray<wistd::unique_ptr<INxApp>, NonPagedPoolNx>
        m_Apps;

    NxNblDatapath
        m_NblDatapath;

#endif // _KERNEL_MODE

    //
    // Adapter registered packet extensions from application/miniport driver
    //
    WDFWAITLOCK
        m_ExtensionCollectionLock = WDF_NO_HANDLE;

    Rtl::KArray<NxPacketExtensionPrivate>
        m_SharedPacketExtensions;

    // Receive scaling capabilities
    NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES
        m_ReceiveScalingCapabilities = {};

    UNICODE_STRING
        m_BaseName = {};

    UNICODE_STRING
        m_InstanceName = {};

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
        AdapterPause;

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
        m_LinkLayerCapabilities = {};

    NET_ADAPTER_POWER_CAPABILITIES
        m_PowerCapabilities = {};

    NET_ADAPTER_LINK_STATE
        m_CurrentLinkState = {};

    NET_ADAPTER_RX_CAPABILITIES
        m_RxCapabilities = {};

    NET_ADAPTER_DMA_CAPABILITIES
        m_RxDmaCapabilities = {};

    NET_ADAPTER_TX_CAPABILITIES
        m_TxCapabilities = {};

    NET_ADAPTER_DMA_CAPABILITIES
        m_TxDmaCapabilities = {};

    ULONG
        m_MtuSize = 0;

    NET_ADAPTER_LINK_LAYER_ADDRESS
        m_PermanentLinkLayerAddress = {};

    NET_ADAPTER_LINK_LAYER_ADDRESS
        m_CurrentLinkLayerAddress = {};

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
        m_EvtReturnRxBuffer = nullptr;

    //
    // Attibutes the client set to use with NETREQUEST and NETPOWERSETTINGS
    // objects. If the client didn't provide one, the Size field will be
    // zero.
    //
    WDF_OBJECT_ATTRIBUTES
        m_NetRequestObjectAttributes = {};

    WDF_OBJECT_ATTRIBUTES
        m_NetPowerSettingsObjectAttributes = {};

    //
    // Pointer to the Default and Default Direct Oid Queus
    //

    NxRequestQueue *
        m_DefaultRequestQueue = nullptr;

    NxRequestQueue *
        m_DefaultDirectRequestQueue = nullptr;

    //
    // List Head to track all allocated Oids
    //
    LIST_ENTRY
        m_ListHeadAllOids = {};

#ifdef _KERNEL_MODE
    //
    // WakeList is used to manage wake patterns, WakeUpFlags and
    // MediaSpecificWakeUpEvents
    //
    NxWake *
        m_NxWake = nullptr;

#endif // _KERNEL_MODE

    //
    // Pointer to instance which manages hardware offloads like
    // Checksum, Lso etc in NetAdapterCx
    //
    wistd::unique_ptr<NxOffloadManager>
        m_NxOffloadManager;

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
        _In_ PVOID ClientQueue,
        _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
        _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
        _Out_ NET_CLIENT_QUEUE * AdapterQueue,
        _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch
    );

    NTSTATUS
    CreateRxQueue(
        _In_ PVOID ClientQueue,
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
        _In_ PVOID RxBufferVa,
        _In_ PVOID RxBufferReturnContext
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    QueryRegisteredPacketExtension(
        _In_ NET_PACKET_EXTENSION_PRIVATE const * ExtensionToQuery
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    RegisterPacketExtension(
        _In_ NET_PACKET_EXTENSION_PRIVATE const * ExtensionToAdd
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

    NTSTATUS
    ValidatePowerCapabilities(
        void
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
        VOID
    ) const;

    void
    Refresh(
        _In_ DeviceState DeviceState
    );

    static
    VOID
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
    VOID
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

    VOID
    SetCurrentLinkLayerAddress(
        _In_ NET_ADAPTER_LINK_LAYER_ADDRESS * LinkLayerAddress
    );

    VOID
    SetDatapathCapabilities(
        _In_ NET_ADAPTER_TX_CAPABILITIES const * TxCapabilities,
        _In_ NET_ADAPTER_RX_CAPABILITIES const * RxCapabilities
    );

    VOID
    SetReceiveScalingCapabilities(
        _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const & Capabilities
    );

    void
    SetLinkLayerCapabilities(
        _In_ NET_ADAPTER_LINK_LAYER_CAPABILITIES const & LinkLayerCapabilities
    );

    void
    SetPowerCapabilities(
        _In_ NET_ADAPTER_POWER_CAPABILITIES const & PowerCapabilities
    );

    void
    SetCurrentLinkState(
        _In_ NET_ADAPTER_LINK_STATE const & CurrentLinkState
    );

    VOID
    ClearGeneralAttributes(
        void
    );

    VOID
    SetMtuSize(
        _In_ ULONG mtuSize
    );

    ULONG
    GetMtuSize(
        void
    ) const;

    VOID
    IndicatePowerCapabilitiesToNdis(
        void
    );

    VOID
    IndicateCurrentLinkStateToNdis(
        void
    );

    VOID
    IndicateMtuSizeChangeToNdis(
        void
    );

    VOID
    IndicateCurrentLinkLayerAddressToNdis(
        void
    );

    void
    PnPStartComplete(
        void
    );

    VOID
    InitializeSelfManagedIO(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    InitializePowerManagement(
        void
    );

    void
    SuspendSelfManagedIo(
        _In_ POWER_ACTION SystemPowerAction,
        _In_ SYSTEM_POWER_STATE TargetSystemPowerState,
        _In_ DEVICE_POWER_STATE TargetDevicePowerState
    );

    void
    RestartSelfManagedIo(
        _In_ POWER_ACTION SystemPowerAction,
        _In_ SYSTEM_POWER_STATE TargetSystemPowerState,
        _In_ DEVICE_POWER_STATE TargetDevicePowerState
    );

    bool
    HasPermanentLinkLayerAddress(
        void
    );

    bool
    HasCurrentLinkLayerAddress(
        void
    );

    NET_PACKET_EXTENSION_PRIVATE *
    QueryAndGetNetPacketExtensionWithLock(
        _In_ const NET_PACKET_EXTENSION_PRIVATE * ExtensionToQuery
    );

    _Requires_lock_held_(m_ExtensionCollectionLock)
    NET_PACKET_EXTENSION_PRIVATE *
    QueryAndGetNetPacketExtension(
        _In_ const NET_PACKET_EXTENSION_PRIVATE * ExtensionToQuery
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
    DispatchRequest(
        _In_ NETREQUEST Request
    );

    NDIS_MEDIUM
    GetMediaType(
        void
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

    BOOLEAN IsAoAcWorkItem;

} NX_STOP_IDLE_WORKITEM_CONTEXT, *PNX_STOP_IDLE_WORKITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    NX_STOP_IDLE_WORKITEM_CONTEXT,
    GetStopIdleWorkItemObjectContext);

