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

#include <KWaitEvent.h>
#include <smfx.h>
#include <wil\resource.h>

#include "FxObjectBase.hpp"
#include "NxAdapterStateMachine.h"
#include "NxForward.hpp"

#include "NxPacketExtensionPrivate.hpp"

#include <NetClientApi.h>

#define PTR_TO_TAG(val) ((PVOID)(ULONG_PTR)(val))

typedef union _AdapterFlags
{
#pragma warning(disable:4201)
    struct
    {
        //
        // The SetGeneralAttributesInProgress adds validation that
        // the client is calling certain SetCapabilities APIs
        // only from EvtAdapterSetCapabilities callback
        //
        ULONG
            SetGeneralAttributesInProgress : 1;

        //
        // Used to keep track of why the client's data path is paused.
        //
        ULONG
            PauseReasonNdis : 1;

        ULONG
            PauseReasonWdf : 1;

        ULONG
            PauseReasonStopping : 1;

        //
        // Stop reasons tell why the adapter did not start yet, MiniportInitializeEx
        // is called only after all these flags are cleared
        //

        // The adapter can't start because the parent device is not started yet
        ULONG
            StopReasonPnp : 1;

        // The adapter can't start because the driver did not call NetAdapterStart
        ULONG
            StopReasonApi : 1;

        //
        // If restart is a result of NDIS requesting restart, adapter must call
        // NdisMRestartComplete.
        //
        ULONG
            NdisMiniportRestartInProgress : 1;
        //
        //
        //
        ULONG
            IsDefault : 1;

    };

    ULONG
        Flags;

} AdapterFlags;

//
// The NxAdapter is an object that represents a NetAdapterCx Client Adapter
//

FORCEINLINE
VOID
NDIS_PM_CAPABILITIES_INIT_FROM_NET_ADAPTER_POWER_CAPABILITIES(
    _Out_ PNDIS_PM_CAPABILITIES NdisPowerCaps,
    _In_ PNET_ADAPTER_POWER_CAPABILITIES NxPowerCaps
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

PNxAdapter
FORCEINLINE
GetNxAdapterFromHandle(
    _In_ NETADAPTER Adapter
    );

typedef class NxAdapter *PNxAdapter;
class NxAdapter
    : public CFxObject<NETADAPTER, NxAdapter, GetNxAdapterFromHandle, false>
    , private NxAdapterStateMachine<NxAdapter>
{
    friend class NxAdapterCollection;

private:

    NxDriver *
        m_NxDriver;

    WDFDEVICE
        m_Device;

    LIST_ENTRY
        m_Linkage;

    Rtl::KArray<wil::unique_wdf_object>
        m_rxQueues;

    Rtl::KArray<wil::unique_wdf_object>
        m_txQueues;

    //
    // Data path lock. Prevent concurrent queue create and destroy.
    //
    WDFWAITLOCK
        m_DataPathControlLock;

    //
    // Certain callbacks must wait for the state machine to finish processing a
    // posted event.
    //
    KAutoEvent
        m_StateChangeComplete;

    NTSTATUS
        m_StateChangeStatus;

    //
    // When we're starting adapters during PnP start we need to wait until
    // NdisWdfPnpPowerHandler (NdisWdfActionPnpStart) returns, otherwise
    // we move the device state machine too soon
    //
    KAutoEvent
        m_MiniportStartComplete;

    NTSTATUS
        m_MiniportStartStatus;

    //
    // Used to callback into NDIS when a asynchronous stop idle operation is
    // complete. When the work item callback is invoked, a synchronous call to
    // WdfDeviceStopIdle is made.
    //
    WDFWORKITEM
        m_PowerRequiredWorkItem;

    //
    // Track WdfDeviceStopIdle failures so as to avoid imbalance of stop idle
    // and resume idle. This relieves the caller (NDIS) from needing to keep
    // track of failures.
    //
    volatile LONG
        m_PowerRefFailureCount;

    //
    // Track whether the queued workitem ran.
    //
    BOOLEAN
        m_PowerRequiredWorkItemPending;

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
        m_AoAcDisengageWorkItemPending;

    //
    // For diagnostic and verification purpose only
    //
    BOOLEAN
        m_AoAcEngaged;

    //
    // Copy of Net LUID received in MiniportInitializeEx callback
    //
    NET_LUID
        m_NetLuid;

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
    Rtl::KArray<wistd::unique_ptr<INxApp>>
        m_Apps;

    NxNblDatapath
        m_NblDatapath;

#endif // _KERNEL_MODE

    //
    // Work item used to start NDIS datapath in case of a PnP rebalance,
    // the KWaitEvent is used to make sure we delay any remove operations
    // while the work item is executing
    //
    WDFWORKITEM
        m_StartDatapathWorkItem;

    KWaitEvent
        m_StartDatapathWorkItemDone;

    //
    // Work item used to stop the adapter when NetAdapterStop is called (if needed)
    //
    WDFWORKITEM
        m_StopAdapterWorkItem;

    //
    // Adapter registered packet extensions from application/miniport driver
    //
    WDFWAITLOCK
        m_ExtensionCollectionLock;

    Rtl::KArray<NxPacketExtensionPrivate>
        m_SharedPacketExtensions;

    // Receive scaling capabilities
    NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES
        m_ReceiveScalingCapabilities = {};

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

    // State machine operations
    SyncOperationDispatch
        NdisInitialize;

    SyncOperationDispatch
        ShouldClientDataPathStartForSelfManagedIoRestart;

    SyncOperationDispatch
        ShouldClientDataPathStartForNdisRestart;

    SyncOperationDispatch
        ClientDataPathStarting;

    AsyncOperationDispatch
        RestartDatapathAfterStop;

    AsyncOperationDispatch
        NdisHalt;

    AsyncOperationDispatch
        ClientDataPathPaused;

    AsyncOperationDispatch
        ClientDataPathStarted;

    AsyncOperationDispatch
        PauseClientDataPathForNdisPause;

    AsyncOperationDispatch
        ClientDataPathPausing;

    SyncOperationDispatch
        ClientDataPathPauseComplete;

    AsyncOperationDispatch
        ClientDataPathStartFailed;

    AsyncOperationDispatch
        PauseClientDataPathForSelfManagedIoSuspend;

    AsyncOperationDispatch
        PauseClientDataPathForNdisPause2;

    AsyncOperationDispatch
        PauseClientDataPathForStop;

    AsyncOperationDispatch
        StoppingAdapterReportToNdis;

    SyncOperationDispatch
        ShouldAdapterStartForPnpStart;

    SyncOperationDispatch
        ShouldAdapterStartForNetAdapterStart;

    AsyncOperationDispatch
        AdapterHalted;

    SyncOperationDispatch
        InitializingReportToNdis;

public:

    //
    // Arrary of active NETTX/RXQUEUE objects
    //
    WDFOBJECT
        m_DataPathQueues[2];

    //
    // Event that is set when the Data Path is paused.
    //
    KAutoEvent
        m_IsPaused;

    //
    // Event that is set when the adapter is halted
    //
    KWaitEvent
        m_IsHalted;

    //
    // Copy of Capabilities set by the client.
    //
    NET_ADAPTER_LINK_LAYER_CAPABILITIES
        m_LinkLayerCapabilities;

    NET_ADAPTER_POWER_CAPABILITIES
        m_PowerCapabilities;

    NET_ADAPTER_LINK_STATE
        m_CurrentLinkState;

    NET_ADAPTER_RX_CAPABILITIES
        m_RxCapabilities;

    NET_ADAPTER_DMA_CAPABILITIES
        m_RxDmaCapabilities;

    NET_ADAPTER_TX_CAPABILITIES
        m_TxCapabilities;

    NET_ADAPTER_DMA_CAPABILITIES
        m_TxDmaCapabilities;

    ULONG
        m_MtuSize;

    NET_ADAPTER_LINK_LAYER_ADDRESS
        m_PermanentLinkLayerAddress;

    NET_ADAPTER_LINK_LAYER_ADDRESS
        m_CurrentLinkLayerAddress;

    // Receive scaling
    struct ReceiveScaling
    {

        bool
            Enabled = false;

        bool
            DatapathRunning = false;

        NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE
            HashType = NetAdapterReceiveScalingHashTypeNone;

        NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE
            ProtocolType = NetAdapterReceiveScalingProtocolTypeNone;

        NET_ADAPTER_RECEIVE_SCALING_HASH_SECRET_KEY
            HashSecretKey = { 0, {} };

    }
        m_ReceiveScaling;

    //
    // Opaque handle returned by ndis.sys for this adapter.
    //
    NDIS_HANDLE
        m_NdisAdapterHandle;

    //
    // A copy of the config structure that client passed to NetAdapterCx.
    //
    NET_ADAPTER_CONFIG
        m_Config;

    //
    // optional callback API from the driver if rx buffer allocation mode
    // is NetRxFragmentBufferAllocationModeNone
    //
    PFN_NET_ADAPTER_RETURN_RX_BUFFER   m_EvtReturnRxBuffer = nullptr;

    //
    // Attibutes the client set to use with NETREQUEST and NETPOWERSETTINGS
    // objects. If the client didn't provide one, the Size field will be
    // zero.
    //
    WDF_OBJECT_ATTRIBUTES
        m_NetRequestObjectAttributes;

    WDF_OBJECT_ATTRIBUTES
        m_NetPowerSettingsObjectAttributes;

    //
    // Pointer to the Default and Default Direct Oid Queus
    //

    PNxRequestQueue
        m_DefaultRequestQueue;

    PNxRequestQueue
        m_DefaultDirectRequestQueue;

    //
    // List Head to track all allocated Oids
    //
    LIST_ENTRY
        m_ListHeadAllOids;

    AdapterFlags
        m_Flags;

#ifdef _KERNEL_MODE
    //
    // WakeList is used to manage wake patterns, WakeUpFlags and
    // MediaSpecificWakeUpEvents
    //
    PNxWake
        m_NxWake;

#endif // _KERNEL_MODE

    NTSTATUS
    ApiStart(
        void
        );

    void
    PnpStart(
        void
        );

    void
    Stop(
        void
        );

    void
    GetProperties(
        _Out_ NET_CLIENT_ADAPTER_PROPERTIES * Properties
        ) const;

    void
    GetDatapathCapabilities(
        _Out_ NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES * DatapathCapabilities
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

    NTSTATUS
    CreateQueueGroup(
        _In_reads_(ClientQueueGroupConfig->NumberOfQueues) PVOID ClientQueueContexts[],
        _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
        _In_ NET_CLIENT_QUEUE_GROUP_CONFIG const * ClientQueueGroupConfig,
        _Out_ NET_CLIENT_QUEUE_GROUP * AdapterQueueGroup,
        _Out_writes_(ClientQueueGroupConfig->NumberOfQueues) NET_CLIENT_QUEUE AdapterQueues[],
        _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterQueueDispatch
        );

    void
    DestroyQueueGroup(
        _In_ NET_CLIENT_QUEUE_GROUP AdapterQueueGroup
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
        _In_ PNX_PRIVATE_GLOBALS NxPrivateGlobals,
        _In_ NETADAPTER Adapter,
        _In_ WDFDEVICE Device,
        _In_ PNET_ADAPTER_CONFIG Config
        );

    void
    DestroyQueue(
        _In_ NxQueue * Queue
        );

public:

    ~NxAdapter(
        void
        );

    NTSTATUS
    ValidatePowerCapabilities(
        void
    );

    static
    NTSTATUS
    _Create(
        _In_ PNX_PRIVATE_GLOBALS PrivateGlobals,
        _In_ WDFDEVICE Device,
        _In_opt_ PWDF_OBJECT_ATTRIBUTES ClientAttributes,
        _In_ PNET_ADAPTER_CONFIG Config,
        _Out_ PNxAdapter * Adapter
        );

    NTSTATUS
    Init(
        void
        );

    NDIS_HANDLE
    GetNdisHandle(
        VOID
        ) const;

    static
    VOID
    _EvtCleanup(
        _In_ WDFOBJECT NetAdatper
        );

    static
    NDIS_HANDLE
    _EvtNdisWdmDeviceGetNdisAdapterHandle(
        _In_ PDEVICE_OBJECT DeviceObject
        );

    static
    PDEVICE_OBJECT
    _EvtNdisGetDeviceObject(
        _In_ NDIS_HANDLE NxAdapterAsContext
        );

    static
    PDEVICE_OBJECT
    _EvtNdisGetNextDeviceObject(
        _In_ NDIS_HANDLE NxAdapterAsContext
        );

    static
    NTSTATUS
    _EvtNdisGetAssignedFdoName(
        _In_  NDIS_HANDLE NxAdapterAsContext,
        _Out_ PUNICODE_STRING FdoName
        );

    static
    NTSTATUS
    _EvtNdisPowerReference(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ BOOLEAN WaitForD0,
        _In_ BOOLEAN InvokeCompletionCallback
        );

    static
    VOID
    _EvtNdisPowerDereference(
        _In_ NDIS_HANDLE NxAdapterAsContext
        );

    NTSTATUS
    AcquirePowerReferenceHelper(
        _In_ BOOLEAN WaitForD0,
        _In_ PVOID Tag
        );

    static
    VOID
    _EvtNdisAoAcEngage(
        _In_ NDIS_HANDLE NxAdapterAsContext
        );

    static
    NTSTATUS
    _EvtNdisAoAcDisengage(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ BOOLEAN WaitForD0
        );

    static
    NDIS_STATUS
    _EvtNdisInitializeEx(
        _In_ NDIS_HANDLE NdisAdapterHandle,
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
        );

    NTSTATUS
    NdisInitialize(
        _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
        );

    static
    VOID
    _EvtNdisUpdatePMParameters(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNDIS_PM_PARAMETERS PMParameters
    );

    RECORDER_LOG
    GetRecorderLog(
        void
        );

    WDFDEVICE
    GetDevice(
        void
        ) const
    {
        return m_Device;
    }

    static
    VOID
    _EvtNdisHaltEx(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ NDIS_HALT_ACTION HaltAction
        );

    VOID
    NdisHaltEx(
        void
        );

    static
    NDIS_STATUS
    _EvtNdisPause(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters
        );

    static
    NDIS_STATUS
    _EvtNdisRestart(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters
        );

    static
    VOID
    _EvtNdisSendNetBufferLists(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNET_BUFFER_LIST NetBufferLists,
        _In_ NDIS_PORT_NUMBER PortNumber,
        _In_ ULONG SendFlags
        );

    static
    VOID
    _EvtNdisReturnNetBufferLists(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNET_BUFFER_LIST NetBufferLists,
        _In_ ULONG ReturnFlags
        );

    static
    VOID
    _EvtNdisCancelSend(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PVOID CancelId
        );

    static
    VOID
    _EvtNdisDevicePnpEventNotify(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNET_DEVICE_PNP_EVENT NetDevicePnPEvent
        );

    static
    VOID
    _EvtNdisShutdownEx(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ NDIS_SHUTDOWN_ACTION ShutdownAction
        );

    static
    NDIS_STATUS
    _EvtNdisOidRequest(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNDIS_OID_REQUEST NdisRequest
        );

    static
    NDIS_STATUS
    _EvtNdisDirectOidRequest(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNDIS_OID_REQUEST NdisRequest
        );

    static
    VOID
    _EvtNdisCancelOidRequest(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PVOID RequestId
        );

    static
    VOID
    _EvtNdisCancelDirectOidRequest(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PVOID RequestId
        );

    static
    NDIS_STATUS
    _EvtNdisSynchronousOidRequestHandler(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PNDIS_OID_REQUEST NdisRequest
        );

    static
    BOOLEAN
    _EvtNdisCheckForHangEx(
        _In_ NDIS_HANDLE NxAdapterAsContext
        );

    static
    NDIS_STATUS
    _EvtNdisResetEx(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _Out_ PBOOLEAN AddressingReset
        );

    static
    VOID
    _EvtStopIdleWorkItem(
        _In_ WDFWORKITEM WorkItem
        );

    static
    VOID
    _EvtStartDatapathWorkItem(
        _In_ WDFWORKITEM WorkItem
        );

    static
    VOID
    _EvtStopAdapterWorkItem(
        _In_ WDFWORKITEM WorkItem
        );

    static
    NTSTATUS
    _EvtNdisAllocateMiniportBlock(
        _In_  NDIS_HANDLE NxAdapterAsContext,
        _In_  ULONG Size,
        _Out_ PVOID * MiniportBlock
        );

    static
    VOID
    _EvtNdisMiniportCompleteAdd(
        _In_  NDIS_HANDLE NxAdapterAsContext,
        _In_  PNDIS_WDF_COMPLETE_ADD_PARAMS Params
        );

    static
    VOID
    _EvtNdisDeviceStartComplete(
        _In_ NDIS_HANDLE NxAdapterAsContext
        );

    NTSTATUS
    InitializeDatapath(
        void
        );

    NTSTATUS
    CreateDatapath(
        void
        );

    void
    DestroyDatapath(
        void
        );

    NET_LUID
    GetNetLuid(
        void
        );

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

    VOID
    ClearGeneralAttributes(
        void
        );

    VOID
    SetMtuSize(
        _In_ ULONG mtuSize
        );

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

    BOOLEAN
    IsClientDataPathStarted(
        void
        );

    VOID
    InitializeSelfManagedIO(
        void
        );

    VOID
    SuspendSelfManagedIO(
        void
        );

    VOID
    RestartSelfManagedIO(
        void
        );

    NTSTATUS
    WaitForStateChangeResult(
        void
        );

    void
    SignalMiniportStartComplete(
        _In_ NTSTATUS Status
        );

    NTSTATUS
    WaitForMiniportStart(
        void
        );

    bool
    AnyReasonToBePaused(
        void
        ) const;

    bool
    AnyReasonToBeStopped(
        void
        ) const;

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

    VOID
    SetAsDefault(
        VOID
        );

    bool
    IsDefault(
        VOID
        ) const;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxAdapter, _GetNxAdapterFromHandle);

PNxAdapter
FORCEINLINE
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

