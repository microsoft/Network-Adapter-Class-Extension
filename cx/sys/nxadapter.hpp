/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxAdapter.hpp

Abstract:

    This is the definition of the NxAdapter object.





Environment:

    kernel mode only

Revision History:

--*/

#pragma once

#include <wil\resource.h>
#include <KArray.h>

#include "NxXlat.hpp"
#include "NxNblDatapath.hpp"




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
        ULONG SetGeneralAttributesInProgress : 1;
        //
        // Used to keep track of why the client's data path is paused.
        //
        ULONG PauseReasonNdis : 1;
        ULONG PauseReasonWdf : 1;
        ULONG PauseReasonPacketClient : 1;
        //
        // If restart is a result of NDIS requesting restart, adapter must call
        // NdisMRestartComplete. 
        //
        ULONG NdisMiniportRestartInProgress : 1;
        //
        // 
        //
        ULONG IsDefault : 1;
    };
    ULONG Flags;
} AdapterFlags;

//
// The NxAdapter is an object that represents a NetAdapterCx Client Adapter
//

FORCEINLINE
VOID
NDIS_PM_CAPABILITIES_INIT_FROM_NET_ADAPTER_POWER_CAPABILITIES(
    _Out_ PNDIS_PM_CAPABILITIES            NdisPowerCaps, 
    _In_  PNET_ADAPTER_POWER_CAPABILITIES  NxPowerCaps
){

    RtlZeroMemory(NdisPowerCaps, sizeof(NDIS_PM_CAPABILITIES));

    NdisPowerCaps->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    NdisPowerCaps->Header.Size = NDIS_SIZEOF_NDIS_PM_CAPABILITIES_REVISION_2;
    NdisPowerCaps->Header.Revision = NDIS_PM_CAPABILITIES_REVISION_2;

    NdisPowerCaps->Flags                      = NxPowerCaps->Flags;
    NdisPowerCaps->SupportedWoLPacketPatterns = NxPowerCaps->SupportedWakePatterns;
    NdisPowerCaps->NumTotalWoLPatterns        = NxPowerCaps->NumTotalWakePatterns;
    NdisPowerCaps->MaxWoLPatternSize          = NxPowerCaps->MaxWakePatternSize;
    NdisPowerCaps->MaxWoLPatternOffset        = NxPowerCaps->MaxWakePatternOffset;
    NdisPowerCaps->MaxWoLPacketSaveBuffer     = NxPowerCaps->MaxWakePacketSaveBuffer;
    NdisPowerCaps->SupportedProtocolOffloads  = NxPowerCaps->SupportedProtocolOffloads;
    NdisPowerCaps->NumArpOffloadIPv4Addresses = NxPowerCaps->NumArpOffloadIPv4Addresses;
    NdisPowerCaps->NumNSOffloadIPv6Addresses  = NxPowerCaps->NumNSOffloadIPv6Addresses;
    NdisPowerCaps->SupportedWakeUpEvents      = NxPowerCaps->SupportedWakeUpEvents;
    NdisPowerCaps->MediaSpecificWakeUpEvents  = NxPowerCaps->SupportedMediaSpecificWakeUpEvents;

    //
    // WDF will request the D-IRPs. Doing this allows NDIS to treat this
    // device as wake capable and send appropriate PM parameters.
    //
    if (NdisPowerCaps->SupportedWoLPacketPatterns & NDIS_PM_WOL_BITMAP_PATTERN_SUPPORTED) {
        NdisPowerCaps->MinPatternWakeUp = NdisDeviceStateD2;
    }
    if (NdisPowerCaps->SupportedWoLPacketPatterns & NDIS_PM_WOL_MAGIC_PACKET_ENABLED) {
        NdisPowerCaps->MinMagicPacketWakeUp = NdisDeviceStateD2;
    }
    if (NdisPowerCaps->SupportedWakeUpEvents & NDIS_PM_WAKE_ON_MEDIA_CONNECT_SUPPORTED) {
        NdisPowerCaps->MinLinkChangeWakeUp = NdisDeviceStateD2;
    }
}

PNxAdapter
FORCEINLINE
GetNxAdapterFromHandle(
    _In_ NETADAPTER Adapter
    );

typedef class NxAdapter *PNxAdapter;
class NxAdapter : public CFxObject<NETADAPTER,
                                   NxAdapter,
                                   GetNxAdapterFromHandle,
                                   false>,
                  public INxAdapter
{

private:

    PNxDriver                    m_NxDriver;

    WDFDEVICE                    m_Device;

    //
    // Adapter state management via the state machine engine
    //
    SM_ENGINE_CONTEXT            m_SmEngineContext;

    //
    // Certain callbacks must wait for the state machine to finish processing a
    // posted event.
    //
    KEVENT                      m_StateChangeComplete;
    NTSTATUS                    m_StateChangeStatus;

    //
    // Used to callback into NDIS when a asynchronous stop idle operation is
    // complete. When the work item callback is invoked, a synchronous call to 
    // WdfDeviceStopIdle is made.
    //
    WDFWORKITEM                 m_PowerRequiredWorkItem;

    //
    // Track WdfDeviceStopIdle failures so as to avoid imbalance of stop idle 
    // and resume idle. This relieves the caller (NDIS) from needing to keep
    // track of failures.
    //
    volatile LONG               m_PowerRefFailureCount;

    //
    // Track whether the queued workitem ran.
    //
    BOOLEAN                     m_PowerRequiredWorkItemPending;

    //
    // Used to callback into NDIS when a asynchronous AoAc disengage operation 
    // is complete. 
    //
    WDFWORKITEM                 m_AoAcDisengageWorkItem;

    //
    // Track whether the queued workitem ran.
    //
    BOOLEAN                     m_AoAcDisengageWorkItemPending;

    //
    // For diagnostic and verification purpose only
    //
    BOOLEAN                     m_AoAcEngaged;

    //
    // Copy of Net LUID received in MiniportInitializeEx callback
    //
    NET_LUID                    m_NetLuid;

    //
    // Copy of physical medium
    //
    NDIS_MEDIUM                 m_MediaType = (NDIS_MEDIUM)0xFFFFFFFF;

    //
    // All datapath apps on the adapter
    //
    Rtl::KArray<wistd::unique_ptr<INxApp>> m_Apps;

    NxNblDatapath               m_NblDatapath;

    //
    // Work item used to start NDIS datapath in case of a PnP rebalance,
    // the KEVENT is used to make sure we delay any remove operations
    // while the work item is executing
    //
    WDFWORKITEM                 m_StartDatapathWorkItem;
    KEVENT                      m_StartDatapathWorkItemDone;

public: 

    //
    // Arrary of active NETTX/RXQUEUE objects 
    //
    WDFOBJECT                    m_DataPathQueues[2];

    //
    // Event that is set when the Data Path is paused. 
    //
    KEVENT                       m_IsPaused;

    //
    // Copy of Capabilities set by the client.
    //
    NET_ADAPTER_LINK_LAYER_CAPABILITIES m_LinkLayerCapabilities;

    NET_ADAPTER_POWER_CAPABILITIES      m_PowerCapabilities;

    NET_ADAPTER_LINK_STATE              m_CurrentLinkState;

    ULONG                               m_MtuSize;

    //
    // Opaque handle returned by ndis.sys for this adapter.
    //
    NDIS_HANDLE                        m_NdisAdapterHandle;

    //
    // A copy of the config structure that client passed to NetAdapterCx. 
    //
    NET_ADAPTER_CONFIG                 m_Config;

    //
    // Attibutes the client set to use with NETREQUEST and NETPOWERSETTINGS
    // objects. If the client didn't provide one, the Size field will be
    // zero.
    //
    WDF_OBJECT_ATTRIBUTES              m_NetRequestObjectAttributes;
    WDF_OBJECT_ATTRIBUTES              m_NetPowerSettingsObjectAttributes;

    //
    // Pointer to the Default and Default Direct Oid Queus
    //

    PNxRequestQueue                    m_DefaultRequestQueue;

    PNxRequestQueue                    m_DefaultDirectRequestQueue;

    //
    // List Head to track all allocated Oids
    //
    LIST_ENTRY                         m_ListHeadAllOids;

    AdapterFlags                       m_Flags;

    //
    // Net packet client instance used for translators
    //
    unique_net_packet_client           m_PacketClient;

    //
    // WakeList is used to manage wake patterns, WakeUpFlags and 
    // MediaSpecificWakeUpEvents
    //
    PNxWake                            m_NxWake;

    //
    // INxAdapter
    //

    virtual NTSTATUS OpenQueue(
        _In_ INxAppQueue *appQueue,
        _In_ NxQueueType queueType,
        _Out_ INxAdapterQueue **adapterQueue) override;

    virtual NDIS_HANDLE GetNdisAdapterHandle() override { return m_NdisAdapterHandle; }

    virtual INxNblDispatcher *GetNblDispatcher() override { return &m_NblDatapath; }

    _IRQL_requires_(PASSIVE_LEVEL)
    virtual NDIS_MEDIUM GetMediaType() override;

private:
    NxAdapter(
        _In_ PNX_PRIVATE_GLOBALS      NxPrivateGlobals,
        _In_ NETADAPTER               Adapter,
        _In_ WDFDEVICE                Device,
        _In_ PNET_ADAPTER_CONFIG      Config
        );

public:

    ~NxAdapter();

    NTSTATUS
    ValidatePowerCapabilities(
        VOID
    );

    static
    NTSTATUS
    _Create(
        _In_     PNX_PRIVATE_GLOBALS      PrivateGlobals,
        _In_     WDFDEVICE                Device,
        _In_opt_ PWDF_OBJECT_ATTRIBUTES   ClientAttributes,
        _In_     PNET_ADAPTER_CONFIG      Config,
        _Out_    PNxAdapter*              Adapter
        );

    NTSTATUS
    Init(
        VOID
        );

    static
    VOID
    _EvtCleanup(
        _In_  WDFOBJECT NetAdatper
        );

    static
    NDIS_HANDLE
    _EvtNdisWdmDeviceGetNdisAdapterHandle(
        _In_ PDEVICE_OBJECT    DeviceObject
        );

    static
    PDEVICE_OBJECT
    _EvtNdisGetDeviceObject(
        _In_ NDIS_HANDLE                    NxAdapterAsContext
        );

    static
    PDEVICE_OBJECT
    _EvtNdisGetNextDeviceObject(
        _In_ NDIS_HANDLE                    NxAdapterAsContext
        );

    static
    NTSTATUS
    _EvtNdisGetAssignedFdoName(
        _In_  NDIS_HANDLE                   NxAdapterAsContext,
        _Out_ PUNICODE_STRING               FdoName
        );

    static
    NTSTATUS
    _EvtNdisPowerReference(
        _In_ NDIS_HANDLE                    NxAdapterAsContext,
        _In_ BOOLEAN                        WaitForD0,
        _In_ BOOLEAN                        InvokeCompletionCallback
        );

    static
    VOID
    _EvtNdisPowerDereference(
        _In_ NDIS_HANDLE                    NxAdapterAsContext
        );

    NTSTATUS
    AcquirePowerReferenceHelper(
        _In_    BOOLEAN      WaitForD0,
        _In_    PVOID        Tag
        );

    static
    VOID
    _EvtNdisAoAcEngage(
        _In_    NDIS_HANDLE                 NxAdapterAsContext
        );

    static
    NTSTATUS
    _EvtNdisAoAcDisengage(
        _In_    NDIS_HANDLE                 NxAdapterAsContext,
        _In_    BOOLEAN                     WaitForD0
        );

    static
    NDIS_STATUS
    _EvtNdisInitializeEx(
        _In_ NDIS_HANDLE                    NdisAdapterHandle,
        _In_ NDIS_HANDLE                    NxAdapterAsContext,
        _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
        );
    
    virtual
    NTSTATUS
    NdisInitialize(
        _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
        );
        
    static
    VOID
    _EvtNdisUpdatePMParameters(
        _In_ NDIS_HANDLE                    NxAdapterAsContext,
        _In_ PNDIS_PM_PARAMETERS            PMParameters
    );

    RECORDER_LOG
    GetRecorderLog() { 
        return m_NxDriver->GetRecorderLog();
    }

    WDFDEVICE
    GetDevice() {
        return m_Device;
    }

    static
    VOID
    _EvtNdisHaltEx(
        _In_ NDIS_HANDLE                    NxAdapterAsContext,
        _In_ NDIS_HALT_ACTION               HaltAction
        );
    
    virtual
    VOID
    NdisHalt(
        VOID
        );
        
    static
    NDIS_STATUS
    _EvtNdisPause(
        _In_ NDIS_HANDLE                     NxAdapterAsContext,
        _In_ PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters
        );
    
    static
    NDIS_STATUS
    _EvtNdisRestart(
        _In_ NDIS_HANDLE                       NxAdapterAsContext,
        _In_ PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters
        );
    
    static
    VOID
    _EvtNdisSendNetBufferLists(
        _In_ NDIS_HANDLE      NxAdapterAsContext,
        _In_ PNET_BUFFER_LIST NetBufferLists,
        _In_ NDIS_PORT_NUMBER PortNumber,
        _In_ ULONG            SendFlags
        );
    
    static
    VOID
    _EvtNdisReturnNetBufferLists(
        _In_ NDIS_HANDLE      NxAdapterAsContext,
        _In_ PNET_BUFFER_LIST NetBufferLists,
        _In_ ULONG            ReturnFlags
        );
    
    static
    VOID
    _EvtNdisCancelSend(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PVOID       CancelId
        );
    
    static
    VOID
    _EvtNdisDevicePnpEventNotify(
        _In_ NDIS_HANDLE           NxAdapterAsContext,
        _In_ PNET_DEVICE_PNP_EVENT NetDevicePnPEvent
        );
    
    static
    VOID
    _EvtNdisShutdownEx(
        _In_ NDIS_HANDLE          NxAdapterAsContext,
        _In_ NDIS_SHUTDOWN_ACTION ShutdownAction
        );
    
    static
    NDIS_STATUS
    _EvtNdisOidRequest(
        _In_ NDIS_HANDLE       NxAdapterAsContext,
        _In_ PNDIS_OID_REQUEST NdisRequest
        );

    static
    NDIS_STATUS
    _EvtNdisDirectOidRequest(
        _In_ NDIS_HANDLE       NxAdapterAsContext,
        _In_ PNDIS_OID_REQUEST NdisRequest
        );

    static
    VOID
    _EvtNdisCancelOidRequest(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PVOID       RequestId
        );
    
    static
    VOID
    _EvtNdisCancelDirectOidRequest(
        _In_ NDIS_HANDLE NxAdapterAsContext,
        _In_ PVOID       RequestId
        );

    static
    BOOLEAN
    _EvtNdisCheckForHangEx(
        _In_ NDIS_HANDLE NxAdapterAsContext
        );
    
    static
    NDIS_STATUS
    _EvtNdisResetEx(
        _In_  NDIS_HANDLE NxAdapterAsContext,
        _Out_ PBOOLEAN    AddressingReset
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
    _EvtNetPacketClientGetExtensions(
        _In_ PVOID ClientContext
        );

    static
    VOID
    _EvtNetPacketClientStart(
        _In_ PVOID ClientContext
        );

    static
    VOID
    _EvtNetPacketClientPause(
        _In_ PVOID ClientContext
        );

    static
    NTSTATUS
    _EvtNdisAllocateMiniportBlock(
        _In_  NDIS_HANDLE NxAdapterAsContext,
        _In_  ULONG       Size,
        _Out_ PVOID       *MiniportBlock
        );

    static
    VOID
    _EvtNdisMiniportCompleteAdd(
        _In_  NDIS_HANDLE NxAdapterAsContext,
        _In_  PNDIS_WDF_COMPLETE_ADD_PARAMS Params
        );

    NTSTATUS
    InitializeDatapathApp(
        _In_ INxAppFactory *factory);

    VOID
    DataPathStart(
        VOID
        );

    VOID
    DataPathPause(
        VOID
        );

    NTSTATUS
    StartDatapath();

    void
    StopDatapath();

    NET_LUID
    GetNetLuid(
        VOID
        );
        
    NTSTATUS
    SetRegistrationAttributes(
        VOID
        );

    NTSTATUS
    SetGeneralAttributes(
        VOID
        );

    VOID
    ClearGeneralAttributes(
        VOID
        );

    VOID
    SetMtuSize(
        _In_ ULONG mtuSize
        );

    VOID
    IndicatePowerCapabilitiesToNdis(
        VOID
        );

    VOID
    IndicateCurrentLinkStateToNdis(
        VOID
        );

    VOID
    IndicateMtuSizeChangeToNdis(
        VOID
        );

    BOOLEAN
    IsClientDataPathStarted(
        VOID
        );

    VOID
    InitializeSelfManagedIO(
        VOID
        );

    VOID
    SuspendSelfManagedIO(
        VOID
        );

    VOID
    RestartSelfManagedIO(
        VOID
        );

    NTSTATUS
    WaitForStateChangeResult(
        VOID
        );

    bool
    AnyReasonToBePaused();

    GENERATED_DECLARATIONS_FOR_NXADAPTER_STATE_ENTRY_FUNCTIONS();
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxAdapter, _GetNxAdapterFromHandle);

PNxAdapter
FORCEINLINE
GetNxAdapterFromHandle(
    _In_ NETADAPTER     Adapter
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

SM_ENGINE_ETW_WRITE_STATE_TRANSITION NxAdapterStateTransitionTracing;

typedef struct _NX_STOP_IDLE_WORKITEM_CONTEXT {
    BOOLEAN IsAoAcWorkItem;
} NX_STOP_IDLE_WORKITEM_CONTEXT, *PNX_STOP_IDLE_WORKITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NX_STOP_IDLE_WORKITEM_CONTEXT,
                                   GetStopIdleWorkItemObjectContext);
