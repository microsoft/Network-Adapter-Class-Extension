// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:
    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include <nblutil.h>
#include <NetClientBufferImpl.h>
#include <NetClientDriverConfigurationImpl.hpp>

#include "NxAdapter.tmh"

#include "NxPacketExtensionPrivate.hpp"

static const NET_CLIENT_DISPATCH CxDispatch =
{
    sizeof(NET_CLIENT_DISPATCH),
    &NetClientCreateBufferPool,
    &NetClientQueryDriverConfigurationUlong,
    &NetClientQueryDriverConfigurationBoolean,
};

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
NTSTATUS
NetClientAdapterCreateTxQueue(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ PVOID ClientContext,
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
    _In_ PVOID ClientContext,
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
VOID
NetClientReturnRxBuffer(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ PVOID RxBufferVirtualAddress,
    _In_ PVOID RxBufferReturnContext
)
{
    reinterpret_cast<NxAdapter*>(Adapter)->ReturnRxBuffer(RxBufferVirtualAddress, RxBufferReturnContext);
}

static
NTSTATUS
NetClientAdapterCreateQueueGroup(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_reads_(ClientQueueConfig->NumberOfQueues) PVOID ClientContexts[],
    _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    _In_ NET_CLIENT_QUEUE_GROUP_CONFIG const * ClientQueueConfig,
    _Out_ NET_CLIENT_QUEUE_GROUP * AdapterQueueGroup,
    _Out_writes_(ClientQueueConfig->NumberOfQueues) NET_CLIENT_QUEUE AdapterQueues[],
    _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterQueueDispatch
    )
{
    return reinterpret_cast<NxAdapter *>(Adapter)->CreateQueueGroup(
        ClientContexts,
        ClientDispatch,
        ClientQueueConfig,
        AdapterQueueGroup,
        AdapterQueues,
        AdapterQueueDispatch);
}

static
void
NetClientAdapterDestroyQueueGroup(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_QUEUE_GROUP AdapterQueueGroup
    )
{
    reinterpret_cast<NxAdapter *>(Adapter)->DestroyQueueGroup(AdapterQueueGroup);
}

static
NTSTATUS
NetClientAdapterRegisterPacketExtension(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_PACKET_EXTENSION const * Extension
    )
{
    NET_PACKET_EXTENSION_PRIVATE extenstionPrivate = {};
    extenstionPrivate.Name = Extension->Name;
    extenstionPrivate.Version = Extension->Version;
    extenstionPrivate.Size = Extension->ExtensionSize;

    // Expect NET_CLIENT to use alignment boundary 2,4, etc values
    extenstionPrivate.NonWdfStyleAlignment = Extension->Alignment;

    return reinterpret_cast<NxAdapter *>(Adapter)->RegisterPacketExtension(&extenstionPrivate);
}

static
NTSTATUS
NetClientAdapterQueryRegisteredPacketExtension(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_PACKET_EXTENSION const * Extension
    )
{
    NET_PACKET_EXTENSION_PRIVATE extenstionPrivate = {};
    extenstionPrivate.Name = Extension->Name;
    extenstionPrivate.Version = Extension->Version;

    return reinterpret_cast<NxAdapter *>(Adapter)->QueryRegisteredPacketExtension(&extenstionPrivate);
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

static const NET_CLIENT_ADAPTER_DISPATCH AdapterDispatch =
{
    sizeof(NET_CLIENT_ADAPTER_DISPATCH),
    &NetClientAdapterGetProperties,
    &NetClientAdapterGetDatapathCapabilities,
    &NetClientAdapterCreateTxQueue,
    &NetClientAdapterCreateRxQueue,
    &NetClientAdapterDestroyQueue,
    &NetClientAdapterCreateQueueGroup,
    &NetClientAdapterDestroyQueueGroup,
    &NetClientReturnRxBuffer,
    &NetClientAdapterRegisterPacketExtension,
    &NetClientAdapterQueryRegisteredPacketExtension,
    &NetClientAdapterReceiveScalingEnable,
    &NetClientAdapterReceiveScalingDisable,
    &NetClientAdapterReceiveScalingSetIndirectionEntries,
    &NetClientAdapterReceiveScalingSetHashSecretKey,
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

NxAdapter::NxAdapter(
    _In_ PNX_PRIVATE_GLOBALS NxPrivateGlobals,
    _In_ NETADAPTER          Adapter,
    _In_ WDFDEVICE           Device,
    _In_ PNET_ADAPTER_CONFIG Config):
    CFxObject(Adapter),
    m_Device(Device),
    m_NxDriver(NxPrivateGlobals->NxDriver),
    m_StateChangeStatus(STATUS_SUCCESS),
    m_NetLuid({ 0 }),
    m_MtuSize(0),
    m_Flags({ 0 }),
    m_NxWake(NULL),
    m_NetRequestObjectAttributes({ 0 }),
    m_NetPowerSettingsObjectAttributes({ 0 })
{
    FuncEntry(FLAG_ADAPTER);

    UNREFERENCED_PARAMETER(NxPrivateGlobals);

    RtlCopyMemory(&m_Config, Config, Config->Size);

    if (Config->NetRequestObjectAttributes != nullptr)
    {
        RtlCopyMemory(&m_NetRequestObjectAttributes, Config->NetRequestObjectAttributes, Config->NetRequestObjectAttributes->Size);
    }

    if (Config->NetPowerSettingsObjectAttributes != nullptr)
    {
        RtlCopyMemory(&m_NetPowerSettingsObjectAttributes, Config->NetPowerSettingsObjectAttributes, Config->NetPowerSettingsObjectAttributes->Size);
    }

    // Make sure we don't carry the client provided pointers
    m_Config.NetRequestObjectAttributes = nullptr;
    m_Config.NetPowerSettingsObjectAttributes = nullptr;

    // adapter initial state is paused.
    m_IsPaused.Set();

    m_PowerRefFailureCount = 0;
    m_PowerRequiredWorkItemPending = FALSE;
    m_AoAcDisengageWorkItemPending = FALSE;

    m_Flags.StopReasonApi = TRUE;
    m_Flags.StopReasonPnp = TRUE;





    FuncExit(FLAG_ADAPTER);
}

NTSTATUS
NxAdapter::Init(
    VOID
    )
/*++
    Since constructor cant fail have an init routine.
--*/
{
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_WORKITEM_CONFIG   stopIdleWorkItemConfig;
    WDF_OBJECT_ATTRIBUTES startDatapathWIattributes;
    WDF_WORKITEM_CONFIG startDatapathWIconfig;
    PNX_STOP_IDLE_WORKITEM_CONTEXT workItemContext;

    NTSTATUS status;

    status = NxWake::_Create(this, &m_NetPowerSettingsObjectAttributes, &m_NxWake);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = GetFxObject();

    status = WdfWaitLockCreate(&objectAttributes, &m_DataPathControlLock);
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

    WDF_OBJECT_ATTRIBUTES_INIT(&startDatapathWIattributes);
    startDatapathWIattributes.ParentObject = GetFxObject();

    WDF_WORKITEM_CONFIG_INIT(&startDatapathWIconfig, _EvtStartDatapathWorkItem);

    status = WdfWorkItemCreate(&startDatapathWIconfig, &startDatapathWIattributes, &m_StartDatapathWorkItem);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "WdfWorkItemCreate failed (m_StartDatapathWorkItem) %!STATUS!", status);
        return status;
    }

    // During the first adapter state machine start we don't need to queue a work item, initialize this event to signaled
    m_StartDatapathWorkItemDone.Set();

    WDF_WORKITEM_CONFIG stopAdapterWorkItemConfig;
    WDF_WORKITEM_CONFIG_INIT(&stopAdapterWorkItemConfig, NxAdapter::_EvtStopAdapterWorkItem);

    WDF_OBJECT_ATTRIBUTES stopAdapterWorkItemAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&stopAdapterWorkItemAttributes);
    stopAdapterWorkItemAttributes.ParentObject = GetFxObject();

    status = WdfWorkItemCreate(
        &stopAdapterWorkItemConfig,
        &stopAdapterWorkItemAttributes,
        &m_StopAdapterWorkItem);

    if (!NT_SUCCESS(status))
    {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "WdfWorkItemCreate failed (m_StopAdapterWorkItem) %!STATUS!", status);
        return status;
    }

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

    return status;
}

NxAdapter::~NxAdapter()
{
    FuncEntry(FLAG_ADAPTER);
    FuncExit(FLAG_ADAPTER);
}

NTSTATUS
NxAdapter::_Create(
    _In_     PNX_PRIVATE_GLOBALS      PrivateGlobals,
    _In_     WDFDEVICE                Device,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES   ClientAttributes,
    _In_     PNET_ADAPTER_CONFIG      Config,
    _Out_    PNxAdapter*              Adapter
)
/*++
Routine Description:
    Static method that creates the NETADAPTER  object.

    This is the internal implementation of the NetAdapterCreate public API.

    Please refer to the NetAdapterCreate API for more description on this
    function and the arguments.
--*/
{
    FuncEntry(FLAG_ADAPTER);
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES attributes;

    // The WDFOBJECT created by NetAdapterCx representing the Net Adapter
    NETADAPTER            netAdapter;

    PNxAdapter            nxAdapter;
    PVOID                 nxAdapterMemory;
    WDFDRIVER             driver;
    PNxDevice             nxDevice;

    //
    // Create a WDFOBJECT for the NxAdapter
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxAdapter);
    attributes.ParentObject = Device;

    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxAdapter::_EvtCleanup;
    //
    // Ensure that the destructor would be called when this object is distroyed.
    //
    NxAdapter::_SetObjectAttributes(&attributes);

    status = WdfObjectCreate(&attributes, (WDFOBJECT*)&netAdapter);
    if (!NT_SUCCESS(status)) {
        LogError(PrivateGlobals->NxDriver->GetRecorderLog(), FLAG_ADAPTER,
                 "WdfObjectCreate for NetAdapter failed %!STATUS!", status);
        FuncExit(FLAG_ADAPTER);
        return status;
    }

    //
    // Since we just created the netAdapter, the NxAdapter object has
    // yet not been constructed. Get the nxAdapter's memory.
    //
    nxAdapterMemory = (PVOID) GetNxAdapterFromHandle(netAdapter);

    //
    // Use the inplacement new and invoke the constructor on the
    // NxAdapter's memory
    //
    nxAdapter = new (nxAdapterMemory) NxAdapter(PrivateGlobals,
                                                netAdapter,
                                                Device,
                                                Config);
    __analysis_assume(nxAdapter != NULL);
    NT_ASSERT(nxAdapter);

    status = nxAdapter->Init();
    if (!NT_SUCCESS(status)) {
        FuncExit(FLAG_ADAPTER);
        return status;
    }

    //
    // Report the newly created device to Ndis.sys
    //
    driver = PrivateGlobals->NxDriver->GetFxObject();

    if (Device) {

        nxDevice = GetNxDeviceFromHandle(Device);

        //
        // Tell the device to process the NetAdapterCreate.
        // NxDevice will report the PnP device add to NDIS since it manages
        // device's PnP/Power state.
        //
        status = nxDevice->AdapterAdd(nxAdapter);

        if (!NT_SUCCESS(status)) {
            FuncExit(FLAG_ADAPTER);
            return status;
        }

        //
        // If PnP start already happened we need to tell the adapter state machine
        //
        // This assumes drivers can only create adapters in
        // EvtDriverDeviceAdd or after the device is started.
        //
        if (nxDevice->IsStarted())
            nxAdapter->PnpStart();

    } else {
        FuncExit(FLAG_ADAPTER);
        return STATUS_NOT_SUPPORTED;
    }

    if (ClientAttributes != WDF_NO_OBJECT_ATTRIBUTES) {
        status = WdfObjectAllocateContext(netAdapter, ClientAttributes, NULL);
        if (!NT_SUCCESS(status)) {
            LogError(PrivateGlobals->NxDriver->GetRecorderLog(), FLAG_ADAPTER,
                     "WdfObjectAllocateContext for ClientAttributes failed %!STATUS!", status);
            FuncExit(FLAG_ADAPTER);
            return status;
        }
    }


    //
    // Dont Fail after this point or else the client's Cleanup / Destroy
    // callbacks can get called.
    //
    // Inform the wake object that initialization has succeeded so it can
    // enable driver provided WDF_OBJECT_ATTRIBUTES cleanup callbacks
    //
    nxAdapter->m_NxWake->AdapterInitComplete();

    nxAdapter->m_NblDatapath.SetNdisHandle(nxAdapter->GetNdisHandle());

    *Adapter = nxAdapter;

    FuncExit(FLAG_ADAPTER);
    return status;
}

void
NxAdapter::SetAsDefault(
    void
    )
{
    m_Flags.IsDefault = TRUE;
}

bool
NxAdapter::IsDefault(
    void
    ) const
{
    return !!m_Flags.IsDefault;
}

NDIS_HANDLE
NxAdapter::GetNdisHandle(
    void
    ) const
{
    return m_NdisAdapterHandle;
}

PDEVICE_OBJECT
NxAdapter::_EvtNdisGetDeviceObject(
    _In_ NDIS_HANDLE                    NxAdapterAsContext
    )
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the WDM Device Object for the clients FDO

Arguments:
    NxAdapterAsContext - The NxAdapter object reported as context to NDIS
--*/
{
    FuncEntry(FLAG_ADAPTER);
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;
    FuncExit(FLAG_ADAPTER);
    return WdfDeviceWdmGetDeviceObject(nxAdapter->m_Device);
}

PDEVICE_OBJECT
NxAdapter::_EvtNdisGetNextDeviceObject(
    _In_ NDIS_HANDLE                    NxAdapterAsContext
    )
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the attached WDM Device Object for the clients FDO

Arguments:
    NxAdapterAsContext - The NxAdapter object reported as context to NDIS
--*/
{
    FuncEntry(FLAG_ADAPTER);
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;
    FuncExit(FLAG_ADAPTER);
    return WdfDeviceWdmGetAttachedDevice(nxAdapter->m_Device);
}

NTSTATUS
NxAdapter::_EvtNdisGetAssignedFdoName(
    _In_  NDIS_HANDLE                   NxAdapterAsContext,
    _Out_ PUNICODE_STRING               FdoName
    )
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the name NetAdapterCx assigned to this device. NDIS
    needs to know this name to be able to create a symbolic link for the miniport.

Arguments:
    NxAdapterAsContext - The NxAdapter object reported as context to NDIS
    FdoName            - UNICODE_STRING that will receive the FDO name
--*/
{
    NTSTATUS       status;
    WDFSTRING      fdoName;
    UNICODE_STRING usFdoName;

    FuncEntry(FLAG_ADAPTER);
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    status = WdfStringCreate(
        NULL,
        WDF_NO_OBJECT_ATTRIBUTES,
        &fdoName);

    if (!NT_SUCCESS(status))
    {
        FuncExit(FLAG_ADAPTER);
        return status;
    }

    status = WdfDeviceRetrieveDeviceName(nxAdapter->m_Device, fdoName);

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
    FuncExit(FLAG_ADAPTER);
    return status;
}

VOID
NxAdapter::_EvtNdisAoAcEngage(
    _In_    NDIS_HANDLE NxAdapterAsContext)
/*++
Routine Description:
    Called by NDIS to engage AoAC when NIC goes quiet (no AoAC ref counts).
    While AoAC is engaged selective suspend must always be stopped. This means
    we will not see power refs/derefs from selective suspend until AoAC is
    disengaged.

Arguments:
    NxAdapterAsContext - The NxAdapter object reported as context to NDIS

--*/
{
    FuncEntry(FLAG_ADAPTER);
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    ASSERT(!nxAdapter->m_AoAcEngaged);

    //
    // -1 is returned if failure count was already 0 (floor). This denotes no
    // failed calls to _EvtNdisPowerReference need to be accounted for.
    // If any other value is returned the failure count was decremented
    // successfully so no need to call WdfDeviceResumeIdle
    //
    if (-1 == NxInterlockedDecrementFloor(&nxAdapter->m_PowerRefFailureCount, 0)) {
        WdfDeviceResumeIdleWithTag(nxAdapter->m_Device,
                                (PVOID)(ULONG_PTR)_EvtNdisAoAcEngage);
    }

    nxAdapter->m_AoAcEngaged = TRUE;

    FuncExit(FLAG_ADAPTER);
    return;
}

NTSTATUS
NxAdapter::_EvtNdisAoAcDisengage(
    _In_    NDIS_HANDLE NxAdapterAsContext,
    _In_    BOOLEAN     WaitForD0)
{
    FuncEntry(FLAG_ADAPTER);
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;
    NTSTATUS status;

    ASSERT(nxAdapter->m_AoAcEngaged);

    status = nxAdapter->AcquirePowerReferenceHelper(WaitForD0,
                                            PTR_TO_TAG(_EvtNdisAoAcDisengage));
    if (status == STATUS_PENDING) {
        ASSERT(!WaitForD0);
        nxAdapter->m_AoAcDisengageWorkItemPending = TRUE;
        WdfWorkItemEnqueue(nxAdapter->m_AoAcDisengageWorkItem);
    }
    else {
        nxAdapter->m_AoAcEngaged = FALSE;
    }

    FuncExit(FLAG_ADAPTER);
    return status;
}

NTSTATUS
NxAdapter::AcquirePowerReferenceHelper(
    _In_    BOOLEAN      WaitForD0,
    _In_    PVOID        Tag
    )
/*++
Routine Description:
    Support routine for _EvtNdisPowerReference and _EvtNdisAoAcDisengage.
    See their routine descriptions for more informaiton.

Arguments:
    WaitForD0 - If WaitFromD0 is false and STATUS_PENDING status is returned
        the Cx will call back into NDIS once power up is complete.
    Tag - Diagnostic use in WdfDeviceStopIdle tag tracking
--*/
{
    FuncEntry(FLAG_ADAPTER);
    NTSTATUS status;

    status = WdfDeviceStopIdleWithTag(m_Device,
                            WaitForD0,
                            Tag); // Tag for tracking
    if (!NT_SUCCESS(status)) {
        LogWarning(GetRecorderLog(), FLAG_ADAPTER,
                   "WdfDeviceStopIdle failed %!STATUS!", status);

        //
        // Track WdfDeviceStopIdle failures so as to avoid imbalance of stop idle
        // and resume idle.
        //
        InterlockedIncrement(&m_PowerRefFailureCount);

        FuncExit(FLAG_ADAPTER);
        return status;
    }

    FuncExit(FLAG_ADAPTER);
    return status;
}

NTSTATUS
NxAdapter::_EvtNdisPowerReference(
    _In_     NDIS_HANDLE  NxAdapterAsContext,
    _In_     BOOLEAN      WaitForD0,
    _In_     BOOLEAN      InvokeCompletionCallback
    )
/*++
Routine Description:
    A Ndis Event Callback

    This routine is called to acquire a power reference on the miniport.
    Every call to _EvtNdisPowerReference must be matched by a call
    to _EvtNdisPowerDereference. However, this function
    keeps tracks of failures from the underlying call to WdfDeviceStopIdle. So the
    caller only needs to make sure they always call power _EvtNdisPowerDereference
    after calling _EvtNdisPowerReference without keeping track of returned failures.

Arguments:
    NxAdapterAsContext - The NxAdapter object reported as context to NDIS
    WaitForD0 - If WaitFromD0 is false and STATUS_PENDING status is returned
        the Cx will call back into NDIS once power up is complete.
    InvokeCompletionCallback - IFF WaitForD0 is FALSE (i.e. Async power ref),
        then the caller can ask for a callback when power up is complete
--*/
{
    FuncEntry(FLAG_ADAPTER);
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;
    NTSTATUS status;

    NT_ASSERTMSG("Invalid parameter to NdisPowerRef",
                !(WaitForD0 && InvokeCompletionCallback));

    status = nxAdapter->AcquirePowerReferenceHelper(WaitForD0, PTR_TO_TAG(_EvtNdisPowerReference));
    if (status == STATUS_PENDING) {
        if (InvokeCompletionCallback) {
            ASSERT(!WaitForD0);
            nxAdapter->m_PowerRequiredWorkItemPending = TRUE;
            WdfWorkItemEnqueue(nxAdapter->m_PowerRequiredWorkItem);
        }
    }

    FuncExit(FLAG_ADAPTER);
    return status;
}

VOID
NxAdapter::_EvtStopIdleWorkItem(
    _In_ WDFWORKITEM WorkItem
    )
/*++
Routine Description:
    A shared WDF work item callback for processing an asynchronous
    _EvtNdisPowerReference and _EvtNdisAoAcDisengage callback.
    The workitem's WDF object context has a boolean to distinguish the two work items.

Arguments:
    WDFWORKITEM - The WDFWORKITEM object that is parented to NxAdapter.
--*/
{
    WDFOBJECT nxAdapterWdfHandle;
    PNxAdapter nxAdapter = NULL;
    NTSTATUS status;
    PNX_STOP_IDLE_WORKITEM_CONTEXT pWorkItemContext;

    FuncEntry(FLAG_ADAPTER);

    pWorkItemContext = GetStopIdleWorkItemObjectContext(WorkItem);
    nxAdapterWdfHandle = WdfWorkItemGetParentObject(WorkItem);
    nxAdapter = GetNxAdapterFromHandle((NETADAPTER)nxAdapterWdfHandle);

    status = WdfDeviceStopIdleWithTag(nxAdapter->m_Device, TRUE,
                                    PTR_TO_TAG(_EvtStopIdleWorkItem));
    if (!NT_SUCCESS(status)) {
        InterlockedIncrement(&nxAdapter->m_PowerRefFailureCount);
        LogWarning(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
            "WdfDeviceStopIdle from work item failed %!STATUS!", status);
    }

    //
    // This workitem was scheduled because an asynchronous call to
    // WdfDeviceStopIdle returned STATUS_PENDING. Since the expectation is that
    // _EvtNdisPowerReference only takes a single power reference on the device,
    // undo that reference. We already have a synchronous one from above.
    //
    if (-1 == NxInterlockedDecrementFloor(&nxAdapter->m_PowerRefFailureCount, 0)) {
        WdfDeviceResumeIdleWithTag(nxAdapter->m_Device,
                                PTR_TO_TAG(_EvtStopIdleWorkItem)); //TAG
    }

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

    FuncExit(FLAG_ADAPTER);
}

NTSTATUS
NxAdapter::_EvtNdisAllocateMiniportBlock(
    _In_  NDIS_HANDLE NxAdapterAsContext,
    _In_  ULONG       Size,
    _Out_ PVOID       *MiniportBlock
    )
/*++
Routine Description:
    This function will allocate the memory for the adapter's miniport block as
    a context in the NETADAPTER object. This way we can ensure the miniport block
    will be valid memory during the adapter's life time.
--*/
{
    FuncEntry(FLAG_ADAPTER);

    NTSTATUS status = STATUS_SUCCESS;
    PNxAdapter nxAdapter = static_cast<PNxAdapter>(NxAdapterAsContext);

    WDF_OBJECT_ATTRIBUTES contextAttributes;

    // Initialize contextAttributes with our dummy type
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &contextAttributes,
        NDIS_MINIPORT_BLOCK_TYPE
        );

    // Override the memory size to allocate with what NDIS told us
    contextAttributes.ContextSizeOverride = Size;

    status = WdfObjectAllocateContext(nxAdapter->GetFxObject(), &contextAttributes, MiniportBlock);

    if (!NT_SUCCESS(status))
    {
        LogError(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
            "WdfObjectAllocateContext failed to allocate the miniport block, %!STATUS!", status);

        FuncExit(FLAG_ADAPTER);
        return status;
    }

    FuncExit(FLAG_ADAPTER);
    return status;
}

VOID
NxAdapter::_EvtNdisMiniportCompleteAdd(
    _In_ NDIS_HANDLE NxAdapterAsContext,
    _In_ PNDIS_WDF_COMPLETE_ADD_PARAMS Params
    )
/*++
Routine Description:
    NDIS uses this callback to report certain adapter parameters to the NIC
    after the Miniport side of device add has completed.
--*/
{
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter nxAdapter = static_cast<PNxAdapter>(NxAdapterAsContext);
    nxAdapter->m_NetLuid = Params->NetLuid;
    nxAdapter->m_MediaType = Params->MediaType;

    FuncExit(FLAG_ADAPTER);
}

_Use_decl_annotations_
VOID
NxAdapter::_EvtNdisDeviceStartComplete(
    NDIS_HANDLE NxAdapterAsContext
    )
/*
Description:

    NDIS uses this callback to indicate the underlying device has completed
    the start IRP, but because NDIS does not distinguish between device and
    adapter it reports for only one of a device's child adapters.
*/
{
    FuncEntry(FLAG_ADAPTER);

    NxAdapter *nxAdapter = static_cast<NxAdapter *>(NxAdapterAsContext);
    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());
    
    nxDevice->StartComplete();

    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::_EvtNdisPowerDereference(
    _In_ NDIS_HANDLE                    NxAdapterAsContext
    )
/*++
Routine Description:
    A Ndis Event Callback

    This routine is called to release a power reference on the
    miniport that it previously acquired. Every call to _EvtNdisPowerReference
    MUST be matched by a call to _EvtNdisPowerDereference.
    However, since failures are tracked from calls to _EvtNdisPowerReference, the
    caller can call _EvtNdisPowerDereference without having to keep track of
    failures from calls to _EvtNdisPowerReference.



Arguments:
    NxAdapterAsContext - The NxAdapter object reported as context to NDIS
--*/
{
    FuncEntry(FLAG_ADAPTER);
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    //
    // -1 is returned if failure count was already 0 (floor). This denotes no
    // failed calls to _EvtNdisPowerReference need to be accounted for.
    // If any other value is returned the failure count was decremented
    // successfully so no need to call WdfDeviceResumeIdle
    //
    if (-1 == NxInterlockedDecrementFloor(&nxAdapter->m_PowerRefFailureCount, 0)) {
        WdfDeviceResumeIdleWithTag(nxAdapter->m_Device,
                                (PVOID)(ULONG_PTR)_EvtNdisPowerDereference);
    }

    FuncExit(FLAG_ADAPTER);
}

NTSTATUS
NxAdapter::SetRegistrationAttributes(
    VOID
    )
{
    FuncEntry(FLAG_ADAPTER);

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
    FuncExit(FLAG_ADAPTER);

    return status;
}


VOID
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

VOID
NxAdapter::SetCurrentLinkLayerAddress(
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS *LinkLayerAddress
    )
{
    NT_ASSERT(LinkLayerAddress->Length <= NDIS_MAX_PHYS_ADDRESS_LENGTH);

    RtlCopyMemory(
        &m_CurrentLinkLayerAddress,
        LinkLayerAddress,
        sizeof(m_CurrentLinkLayerAddress));
}

_Use_decl_annotations_
VOID
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
    NDIS_RSS_CAPS_FLAGS rssCapsFlags = {};

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
    VOID
    )
{
    FuncEntry(FLAG_ADAPTER);

    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES adapterGeneral = {0};
    NDIS_PM_CAPABILITIES                     pmCapabilities;
    NTSTATUS                                 status = STATUS_INVALID_PARAMETER;
    NDIS_STATUS                              ndisStatus;

    m_Flags.SetGeneralAttributesInProgress = FALSE;

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

    adapterGeneral.SupportedPacketFilters = m_LinkLayerCapabilities.SupportedPacketFilters;
    adapterGeneral.MaxMulticastListSize = m_LinkLayerCapabilities.MaxMulticastListSize;
    adapterGeneral.SupportedStatistics = m_LinkLayerCapabilities.SupportedStatistics;

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
    NDIS_PM_CAPABILITIES_INIT_FROM_NET_ADAPTER_POWER_CAPABILITIES(&pmCapabilities,
                                                                  &m_PowerCapabilities);

    adapterGeneral.PowerManagementCapabilitiesEx = &pmCapabilities;

    adapterGeneral.SupportedOidList = NULL;
    adapterGeneral.SupportedOidListLength = 0;

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

Exit:
    FuncExit(FLAG_ADAPTER);

    return status;
}

NTSTATUS
NxAdapter::ValidatePowerCapabilities(
    VOID
    )
/*++
Routine Description:
    This routine returns a failure status if a driver's power capabilities are
    invalid based on its power policy ownership.

    We should ideally call this during PnP start (set capbilities/attribs) itself
    but we do not because there is no good WDF API at the moment that a Cx can
    use to determine power policy ownership at that time. So we use WdfDeviceStopIdle
    which can only be used starting D0Entry.

Return Value:
    NTSTATUS

Arguments:
    VOID
--*/
{
    NTSTATUS status;
    BOOLEAN  isPwrPolicyOwner;

    status = WdfDeviceStopIdleWithTag(GetDevice(), FALSE,
                            PTR_TO_TAG(_EvtNdisPowerReference));
    if (!NT_SUCCESS(status)) {
        //
        // Device is not the power policy owner. Replace with better WDF API
        // once available.
        //
        LogInfo(GetRecorderLog(), FLAG_DEVICE, "Device is not the power policy owner");
        status = STATUS_SUCCESS;
        isPwrPolicyOwner = FALSE;
    }
    else {
        WdfDeviceResumeIdleWithTag(GetDevice(), PTR_TO_TAG(_EvtNdisPowerReference));
        isPwrPolicyOwner = TRUE;
    }

    if (!isPwrPolicyOwner) {
        if (m_PowerCapabilities.NumTotalWakePatterns != 0        ||
            m_PowerCapabilities.SupportedWakeUpEvents != 0      ||
            m_PowerCapabilities.SupportedWakePatterns != 0 ||
            m_PowerCapabilities.Flags & NET_ADAPTER_POWER_SELECTIVE_SUSPEND ||
            m_PowerCapabilities.Flags & NET_ADAPTER_POWER_WAKE_PACKET_INDICATION ||
            m_PowerCapabilities.ManageS0IdlePowerReferences == WdfTrue)
        {
            status = STATUS_INVALID_PARAMETER;
            LogError(GetRecorderLog(), FLAG_DEVICE,
                "Error %!STATUS! WDFDEVICE is not the power policy owner. Power capabilities "
                "reported by NetAdapterSetPowerCapabilities require ownership", status);
        }
    }

    if (m_PowerCapabilities.ManageS0IdlePowerReferences == WdfUseDefault) {
        //
        // Evaluate the Cx's default role in taking power references
        //
        if (isPwrPolicyOwner) {
            m_PowerCapabilities.ManageS0IdlePowerReferences = WdfTrue;
        }
        else {
            m_PowerCapabilities.ManageS0IdlePowerReferences = WdfFalse;
        }
    }

    return status;
}

VOID
NxAdapter::ClearGeneralAttributes(
    VOID
    )
{
    FuncEntry(FLAG_ADAPTER);

    m_Flags.SetGeneralAttributesInProgress = TRUE;

    //
    // Clear the Capabilities that are mandatory for the NetAdapter driver to set
    //
    RtlZeroMemory(&m_LinkLayerCapabilities,     sizeof(NET_ADAPTER_LINK_LAYER_CAPABILITIES));
    m_MtuSize = 0;

    //
    // Initialize the optional capabilities to defaults.
    //
    NET_ADAPTER_POWER_CAPABILITIES_INIT(&m_PowerCapabilities);
    NET_ADAPTER_LINK_STATE_INIT_DISCONNECTED(&m_CurrentLinkState);

    FuncExit(FLAG_ADAPTER);
}


VOID
NxAdapter::SetMtuSize(
    _In_ ULONG mtuSize
    )
{
    FuncEntry(FLAG_ADAPTER);

    m_MtuSize = mtuSize;

    if (!m_Flags.SetGeneralAttributesInProgress)
    {
        LogVerbose(GetRecorderLog(), FLAG_ADAPTER, "Updated MTU Size to %lu", mtuSize);

        IndicateMtuSizeChangeToNdis();
    }

    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::IndicatePowerCapabilitiesToNdis(
    VOID
    )
/*++
Routine Description:

    Sends a Power Capabilities status indication to NDIS.

--*/
{
    FuncEntry(FLAG_ADAPTER);

    NDIS_PM_CAPABILITIES   pmCapabilities;

    NDIS_PM_CAPABILITIES_INIT_FROM_NET_ADAPTER_POWER_CAPABILITIES(
        &pmCapabilities, &m_PowerCapabilities);

    auto statusIndication =
        MakeNdisStatusIndication(
            m_NdisAdapterHandle, NDIS_STATUS_PM_HARDWARE_CAPABILITIES, pmCapabilities);

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &statusIndication);

    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::IndicateCurrentLinkStateToNdis(
    VOID
    )
/*++
Routine Description:

    Sends a Link status indication to NDIS.

--*/
{
    FuncEntry(FLAG_ADAPTER);

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

    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::IndicateMtuSizeChangeToNdis(
    VOID
    )
{
    FuncEntry(FLAG_ADAPTER);

    auto statusIndication =
        MakeNdisStatusIndication(
            m_NdisAdapterHandle, NDIS_STATUS_L2_MTU_SIZE_CHANGE, m_MtuSize);

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &statusIndication);

    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::IndicateCurrentLinkLayerAddressToNdis(
    VOID
    )
{
    auto statusIndication = MakeNdisStatusIndication(
        m_NdisAdapterHandle,
        NDIS_STATUS_CURRENT_MAC_ADDRESS_CHANGE,
        m_CurrentLinkLayerAddress);

    NdisMIndicateStatusEx(m_NdisAdapterHandle, &statusIndication);
}

NDIS_STATUS
NxAdapter::_EvtNdisInitializeEx(
    _In_ NDIS_HANDLE                    NdisAdapterHandle,
    _In_ NDIS_HANDLE                    UnusedContext,
    _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
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
    FuncEntry(FLAG_ADAPTER);
    NTSTATUS status;
    PNxAdapter nxAdapter;

    UNREFERENCED_PARAMETER(UnusedContext);

    nxAdapter = (PNxAdapter)
        NdisWdfGetAdapterContextFromAdapterHandle(NdisAdapterHandle);

    status = nxAdapter->NdisInitialize(MiniportInitParameters);

    FuncExit(FLAG_ADAPTER);
    return NdisConvertNtStatusToNdisStatus(status);
}

VOID
NxAdapter::_EvtNdisHaltEx(
    _In_ NDIS_HANDLE                    NxAdapterAsContext,
    _In_ NDIS_HALT_ACTION               HaltAction
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

    NxAdapterAsContext - NxAdapter reported as Context to NDIS.sys

--*/
{
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    FuncEntry(FLAG_ADAPTER);

    UNREFERENCED_PARAMETER(HaltAction);

    nxAdapter->NdisHaltEx();

    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::NdisHaltEx()
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to halt a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called NdisWdfPnpAddDevice routine.

Arguments:

    NxAdapterAsContext - NxAdapter reported as Context to NDIS.sys

--*/
{
    FuncEntry(FLAG_ADAPTER);

    EnqueueEvent(NxAdapter::Event::NdisMiniportHalt);

    FuncExit(FLAG_ADAPTER);
}

BOOLEAN
NxAdapter::IsClientDataPathStarted(
    VOID
    )
/*++
Routine Description:
    Checks if the Client's data path is started or not.
    This routine starts returning true right before the client's EvtAdapterStart
    routine is called and starts returning false only after the client has
    completed the EvtAdapterPause operation.
--*/
{
    return !m_IsPaused.Test();
}

NDIS_STATUS
NxAdapter::_EvtNdisPause(
    _In_ NDIS_HANDLE                     NxAdapterAsContext,
    _In_ PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters
    )
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to pause the adapter.
    NetAdapterCx in response calls the clients EvtAdapterPause callback.

--*/
{
    FuncEntry(FLAG_ADAPTER);
    UNREFERENCED_PARAMETER(PauseParameters);

    PNxAdapter nxAdapter = (PNxAdapter)NxAdapterAsContext;

    nxAdapter->EnqueueEvent(NxAdapter::Event::NdisMiniportPause);

    FuncExit(FLAG_ADAPTER);
    return NDIS_STATUS_PENDING;
}

NET_LUID
NxAdapter::GetNetLuid(
    VOID
    )
{
    return m_NetLuid;
}

NDIS_STATUS
NxAdapter::_EvtNdisRestart(
    _In_ NDIS_HANDLE                       NxAdapterAsContext,
    _In_ PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters
    )
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to start (restart) the adapter.
    NetAdapterCx in response calls the clients EvtAdapterStart callback.

--*/
{
    FuncEntry(FLAG_ADAPTER);
    UNREFERENCED_PARAMETER(RestartParameters);

    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    nxAdapter->EnqueueEvent(NxAdapter::Event::NdisMiniportRestart);

    FuncExit(FLAG_ADAPTER);
    return NDIS_STATUS_PENDING;
}

VOID
NxAdapter::_EvtNdisSendNetBufferLists(
    _In_ NDIS_HANDLE      NxAdapterAsContext,
    _In_ PNET_BUFFER_LIST NetBufferLists,
    _In_ NDIS_PORT_NUMBER PortNumber,
    _In_ ULONG            SendFlags
    )
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to send data over the network the adapter.

    NetAdapterCx in response calls the clients EvtAdapterSendNetBufferLists callback.

--*/
{
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    nxAdapter->m_NblDatapath.SendNetBufferLists(NetBufferLists, PortNumber, SendFlags);
}

VOID
NxAdapter::_EvtNdisReturnNetBufferLists(
    _In_ NDIS_HANDLE      NxAdapterAsContext,
    _In_ PNET_BUFFER_LIST NetBufferLists,
    _In_ ULONG            ReturnFlags
    )
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to return a NBL to the miniport.

    NetAdapterCx in response calls the clients EvtAdapterReturnNetBufferLists callback.

--*/
{
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    nxAdapter->m_NblDatapath.ReturnNetBufferLists(NetBufferLists, ReturnFlags);
}

VOID
NxAdapter::_EvtNdisCancelSend(
    _In_ NDIS_HANDLE NxAdapterAsContext,
    _In_ PVOID       CancelId
    )
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to cancel a send NBL request.

--*/
{
    FuncEntry(FLAG_ADAPTER);
    UNREFERENCED_PARAMETER(NxAdapterAsContext);
    UNREFERENCED_PARAMETER(CancelId);

    NT_ASSERTMSG("Missing Implementation", FALSE);
    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::_EvtNdisDevicePnpEventNotify(
    _In_ NDIS_HANDLE           NxAdapterAsContext,
    _In_ PNET_DEVICE_PNP_EVENT NetDevicePnPEvent
    )
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to report a Pnp Event.

--*/
{
    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    FuncEntry(FLAG_ADAPTER);

    if (NetDevicePnPEvent->DevicePnPEvent == NdisDevicePnPEventPowerProfileChanged) {
        if (NetDevicePnPEvent->InformationBufferLength == sizeof(ULONG)) {
            ULONG NdisPowerProfile = *((PULONG)NetDevicePnPEvent->InformationBuffer);

            if (NdisPowerProfile == NdisPowerProfileBattery) {
                LogInfo(nxAdapter->GetRecorderLog(), FLAG_ADAPTER, "On NdisPowerProfileBattery");
            } else if (NdisPowerProfile == NdisPowerProfileAcOnLine) {
                LogInfo(nxAdapter->GetRecorderLog(), FLAG_ADAPTER, "On NdisPowerProfileAcOnLine");
            } else {
                LogError(nxAdapter->GetRecorderLog(), FLAG_ADAPTER, "Unexpected PowerProfile %d", NdisPowerProfile);
                NT_ASSERTMSG("Unexpected PowerProfile", FALSE);
            }
        }
    } else {
        LogInfo(nxAdapter->GetRecorderLog(), FLAG_ADAPTER, "PnpEvent %!NDIS_DEVICE_PNP_EVENT! from Ndis.sys", NetDevicePnPEvent->DevicePnPEvent);
    }
    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::_EvtNdisShutdownEx(
    _In_ NDIS_HANDLE          NxAdapterAsContext,
    _In_ NDIS_SHUTDOWN_ACTION ShutdownAction
    )
{
    FuncEntry(FLAG_ADAPTER);
    UNREFERENCED_PARAMETER(NxAdapterAsContext);
    UNREFERENCED_PARAMETER(ShutdownAction);

    NT_ASSERTMSG("Missing Implementation", FALSE);
    FuncExit(FLAG_ADAPTER);
}

NDIS_STATUS
NxAdapter::_EvtNdisOidRequest(
    _In_ NDIS_HANDLE       NxAdapterAsContext,
    _In_ PNDIS_OID_REQUEST NdisOidRequest
    )
{
    NDIS_STATUS ndisStatus;
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;
    struct _NDIS_OID_REQUEST::_REQUEST_DATA::_SET *Set;

    // intercept and forward to the NDIS translator.
    for (auto &app : nxAdapter->m_Apps)
    {
        if (nxAdapter->m_ClientDispatch->NdisOidRequestHandler)
        {
            NTSTATUS status;
            if (nxAdapter->m_ClientDispatch->NdisOidRequestHandler(app.get(), NdisOidRequest, &status))
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);
                return NdisConvertNtStatusToNdisStatus(status);
            }
        }
    }

    switch (NdisOidRequest->RequestType) {
    case NdisRequestSetInformation:

        Set = &NdisOidRequest->DATA.SET_INFORMATION;

        switch(Set->Oid)
        {

        case OID_PNP_SET_POWER:

            //
            // A Wdf Client has no need to handle the OID_PNP_SET_POWER
            // request. It leverages standard Wdf power callbacks.
            //

            //
            // NOTE: NDIS Contract: After this is completed successfully for a Dx transition
            // client must not indicate any packets, issues status indications, or
            // generally interact with NDIS.



            LogVerbose(nxAdapter->GetRecorderLog(), FLAG_ADAPTER, "Ignoring OID_PNP_SET_POWER");
            FuncExit(FLAG_ADAPTER);
            return NDIS_STATUS_SUCCESS;

        case OID_PM_PARAMETERS:
            //
            // Cx processes this on behalf of the client.
            // It maintains a NxWake object that tracks the OID_PM_PARAMETERS
            //
            if (Set->InformationBufferLength < sizeof(NDIS_PM_PARAMETERS)) {
                NT_ASSERTMSG("Bad OID_PM_PARAMETERS size", FALSE);
                FuncExit(FLAG_ADAPTER);
                return NDIS_STATUS_INVALID_PARAMETER;
            }
            ndisStatus = nxAdapter->m_NxWake->ProcessOidPmParameters((PNDIS_PM_PARAMETERS)Set->InformationBuffer);

            FuncExit(FLAG_ADAPTER);
            return ndisStatus;
        case OID_PM_ADD_WOL_PATTERN:
        case OID_PM_REMOVE_WOL_PATTERN:
            ndisStatus = nxAdapter->m_NxWake->ProcessOidWakePattern(nxAdapter->GetFxObject(), Set);

            FuncExit(FLAG_ADAPTER);
            return ndisStatus;
        case OID_PM_ADD_PROTOCOL_OFFLOAD:
        case OID_PM_REMOVE_PROTOCOL_OFFLOAD:
            ndisStatus = nxAdapter->m_NxWake->ProcessOidProtocolOffload(nxAdapter->GetFxObject(), Set);

            FuncExit(FLAG_ADAPTER);
            return ndisStatus;
        } // switch(Set->Oid)
        break;

    default:
        break;

    } //switch (NdisOidRequest->RequestType)

    //
    // Send the OID to the client
    //

    if (nxAdapter->m_DefaultRequestQueue == NULL) {
        LogWarning(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
                   "Rejecting OID, client didn't create a default queue");
        return NDIS_STATUS_NOT_SUPPORTED;
    }

    nxAdapter->m_DefaultRequestQueue->QueueNdisOidRequest(NdisOidRequest);

    ndisStatus = NDIS_STATUS_PENDING;

    FuncExit(FLAG_ADAPTER);
    return ndisStatus;
}

NDIS_STATUS
NxAdapter::_EvtNdisDirectOidRequest(
    _In_ NDIS_HANDLE NxAdapterAsContext,
    _In_ PNDIS_OID_REQUEST Request
    )
{
    NDIS_STATUS ndisStatus;
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    // intercept and forward to the NDIS translator.
    for (auto &app : nxAdapter->m_Apps)
    {
        if (nxAdapter->m_ClientDispatch->NdisDirectOidRequestHandler)
        {
            NTSTATUS status;
            if (nxAdapter->m_ClientDispatch->NdisDirectOidRequestHandler(app.get(), Request, &status))
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);
                return NdisConvertNtStatusToNdisStatus(status);
            }
        }
    }

    if (nxAdapter->m_DefaultDirectRequestQueue == NULL) {
        LogWarning(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
                   "Rejecting OID, client didn't create a default direct queue");
        return NDIS_STATUS_NOT_SUPPORTED;
    }

    nxAdapter->m_DefaultDirectRequestQueue->QueueNdisOidRequest(Request);

    ndisStatus = NDIS_STATUS_PENDING;

    FuncExit(FLAG_ADAPTER);
    return ndisStatus;
}

VOID
NxAdapter::_EvtNdisCancelOidRequest(
    _In_ NDIS_HANDLE NxAdapterAsContext,
    _In_ PVOID       RequestId
    )
{
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;
    WDFDEVICE wdfDevice = nxAdapter->GetDevice();
    NxDevice* nxDevice = GetNxDeviceFromHandle(wdfDevice);

    if (nxAdapter->m_DefaultRequestQueue != NULL) {

        TraceLoggingWrite(
            g_hNetAdapterCxEtwProvider,
            "NxAdapterStateTransition",
            TraceLoggingDescription("NxAdapterEvtNdisCancelOidRequest"),
            TraceLoggingKeyword(NET_ADAPTER_CX_OID | NET_ADAPTER_CX_NX_ADAPTER),
            TraceLoggingUInt32(nxDevice->GetDeviceBusAddress(), "DeviceBusAddress"),
            TraceLoggingHexUInt64(nxAdapter->GetNetLuid().Value, "AdapterNetLuid"),
            TraceLoggingUInt32((ULONG)RequestId, "OidRequestId"));

        nxAdapter->m_DefaultRequestQueue->CancelRequests(RequestId);
    }

    FuncExit(FLAG_ADAPTER);
}

VOID
NxAdapter::_EvtNdisCancelDirectOidRequest(
    _In_ NDIS_HANDLE NxAdapterAsContext,
    _In_ PVOID       RequestId
    )
{
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    if (nxAdapter->m_DefaultDirectRequestQueue != NULL) {
        nxAdapter->m_DefaultDirectRequestQueue->CancelRequests(RequestId);
    }

    FuncExit(FLAG_ADAPTER);
}

NDIS_STATUS
NxAdapter::_EvtNdisSynchronousOidRequestHandler(
    _In_ NDIS_HANDLE NxAdapterAsContext,
    _In_ PNDIS_OID_REQUEST Request
    )
{
    FuncEntry(FLAG_ADAPTER);

    auto nxAdapter = reinterpret_cast<NxAdapter *>(NxAdapterAsContext);

    // intercept and forward to the NDIS translator.
    for (auto &app : nxAdapter->m_Apps)
    {
        if (nxAdapter->m_ClientDispatch->NdisSynchronousOidRequestHandler)
        {
            NTSTATUS status;
            if (nxAdapter->m_ClientDispatch->NdisSynchronousOidRequestHandler(app.get(), Request, &status))
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);
                return NdisConvertNtStatusToNdisStatus(status);
            }
        }
    }

    return NdisConvertNtStatusToNdisStatus(STATUS_NOT_SUPPORTED);
}

BOOLEAN
NxAdapter::_EvtNdisCheckForHangEx(
    _In_ NDIS_HANDLE NxAdapterAsContext
    )
{
    UNREFERENCED_PARAMETER(NxAdapterAsContext);
    FuncEntry(FLAG_ADAPTER);
    FuncExit(FLAG_ADAPTER);
    return FALSE;
}

NDIS_STATUS
NxAdapter::_EvtNdisResetEx(
    _In_  NDIS_HANDLE NxAdapterAsContext,
    _Out_ PBOOLEAN    AddressingReset
    )
{
    FuncEntry(FLAG_ADAPTER);
    UNREFERENCED_PARAMETER(NxAdapterAsContext);

    *AddressingReset = FALSE;

    NT_ASSERTMSG("Missing Implementation", FALSE);

    FuncExit(FLAG_ADAPTER);
    return NDIS_STATUS_NOT_SUPPORTED;
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
NxAdapter::CreateDatapath(
    void
    )
{
    for (auto &app : m_Apps)
    {
        NTSTATUS status = CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            m_ClientDispatch->CreateDatapath(app.get()),
            "Failed to start datapath app. Adapter=%p", this);

        if (!NT_SUCCESS(status))
        {
            DestroyDatapath();
            return status;
        }
    }

    // when we convert the framework to handling dynamic receive scaling
    // we will need to synchronize access to m_ReceiveScaling variable
    m_ReceiveScaling.DatapathRunning = true;

    if (m_ReceiveScaling.Enabled)
    {
        NTSTATUS status = m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingEnable(
            GetFxObject());

        if (!NT_SUCCESS(status))
        {
            DestroyDatapath();
        }
    }

    return STATUS_SUCCESS;
}

void
NxAdapter::DestroyDatapath(
    void
    )
{
    for (auto &app : m_Apps)
    {
        m_ClientDispatch->DestroyDatapath(app.get());
    }

    m_ReceiveScaling.DatapathRunning = false;
}

NTSTATUS
NxAdapter::ApiStart(
    void
    )
{
    EnqueueEvent(NxAdapter::Event::NetAdapterStart);
    return STATUS_SUCCESS;
}

void
NxAdapter::PnpStart(
    void
    )
{
    EnqueueEvent(NxAdapter::Event::PnpStart);
}

void
NxAdapter::Stop(
    void
    )
{
    EnqueueEvent(NxAdapter::Event::NetAdapterStop);

    // Wait until the adapter is halted to return
    m_IsHalted.Wait();
}

_Use_decl_annotations_
void
NxAdapter::GetProperties(
    NET_CLIENT_ADAPTER_PROPERTIES * Properties
    ) const
{
    Properties->MediaType = m_MediaType;
    Properties->NetLuid = m_NetLuid;
    Properties->DriverIsVerifying = !!MmIsDriverVerifyingByAddress((PVOID)m_Config.EvtAdapterCreateTxQueue);
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
        DatapathCapabilities->MemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER;
    }
    else if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem) &&
             (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeSystem))
    {
        DatapathCapabilities->MemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH;
    }
    else if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem) &&
             (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver))
    {
        DatapathCapabilities->MemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE;
    }
    else
    {
        NT_FRE_ASSERT(false);
    }

    DatapathCapabilities->MaximumTxFragmentSize = txCap.MaximumFragmentBufferSize;
    DatapathCapabilities->MaximumRxFragmentSize = rxCap.MaximumFragmentBufferSize;
    DatapathCapabilities->MaximumNumberOfTxQueues = txCap.MaximumNumberOfQueues;
    DatapathCapabilities->MaximumNumberOfRxQueues = rxCap.MaximumNumberOfQueues;
    DatapathCapabilities->PreferredTxFragmentRingSize = txCap.FragmentRingNumberOfElementsHint;
    DatapathCapabilities->PreferredRxFragmentRingSize = rxCap.FragmentRingNumberOfElementsHint;

    if (rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem)
    {
        if (rxCap.MappingRequirement == NetMemoryMappingRequirementNone)
        {
            DatapathCapabilities->MemoryConstraints.MappingRequirement = NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_NONE;
        }
        else if (rxCap.MappingRequirement == NetMemoryMappingRequirementDmaMapped)
        {
            DatapathCapabilities->MemoryConstraints.MappingRequirement = NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED;
        }
        else
        {
            NT_FRE_ASSERT(false);
        }
    }

    DatapathCapabilities->NominalMaxTxLinkSpeed = m_LinkLayerCapabilities.MaxTxLinkSpeed;
    DatapathCapabilities->NominalMaxRxLinkSpeed = m_LinkLayerCapabilities.MaxRxLinkSpeed;
    DatapathCapabilities->NominalMtu = m_MtuSize;
    DatapathCapabilities->MtuWithLso = m_MtuSize;
    DatapathCapabilities->MtuWithRsc = m_MtuSize;

    DatapathCapabilities->MemoryConstraints.MappingRequirement =
        static_cast<NET_CLIENT_MEMORY_MAPPING_REQUIREMENT> (rxCap.MappingRequirement);
    DatapathCapabilities->MemoryConstraints.AlignmentRequirement = rxCap.FragmentBufferAlignment;

    if (DatapathCapabilities->MemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED)
    {
        DatapathCapabilities->MemoryConstraints.Dma.CacheEnabled =
            static_cast<NET_CLIENT_TRI_STATE> (rxCap.DmaCapabilities->CacheEnabled);
        DatapathCapabilities->MemoryConstraints.Dma.MaximumPhysicalAddress = rxCap.DmaCapabilities->MaximumPhysicalAddress;
        DatapathCapabilities->MemoryConstraints.Dma.PreferredNode = rxCap.DmaCapabilities->PreferredNode;


        DatapathCapabilities->MemoryConstraints.Dma.DmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(rxCap.DmaCapabilities->DmaEnabler,
                                          WdfDmaDirectionReadFromDevice);
    }
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::AddNetClientPacketExtensionsToQueueCreationContext(
    NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    QUEUE_CREATION_CONTEXT& QueueCreationContext
    )
{
    //
    // convert queue config's NET_CLIENT_PACKET_EXTENSION to NET_PACKET_EXTENSION_PRIVATE
    // since we passed API boundary, CX should use private throughout.
    // CX needs to use the stored extension within the adapter object at this point
    // so that the object holding info about packet extension is owned by CX.
    //
    for (size_t i = 0; i < ClientQueueConfig->NumberOfPacketExtensions; i++)
    {
        NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
        extensionPrivate.Name = ClientQueueConfig->PacketExtensions[i].Name;
        extensionPrivate.Version = ClientQueueConfig->PacketExtensions[i].Version;

        NET_PACKET_EXTENSION_PRIVATE* storedExtension =
            QueryAndGetNetPacketExtensionWithLock(&extensionPrivate);

        CX_RETURN_NTSTATUS_IF(
            STATUS_INVALID_PARAMETER,
            !storedExtension);

        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            !QueueCreationContext.NetClientAddedPacketExtensions.append(*storedExtension));
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::CreateTxQueue(
    PVOID ClientQueue,
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
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! context.QueueContexts.resize(1));

    context.CurrentThread = KeGetCurrentThread();
    context.ClientQueueConfig = ClientQueueConfig;
    context.ClientDispatch = ClientDispatch;
    context.AdapterDispatch = AdapterDispatch;
    context.Adapter = this;
    context.QueueContexts[0].QueueId = QueueId;
    context.QueueContexts[0].ClientQueue = ClientQueue;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AddNetClientPacketExtensionsToQueueCreationContext(ClientQueueConfig, context),
        "Failed to transfer NetClientQueueConfig's packet extension to queue_creation_context");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_Config.EvtAdapterCreateTxQueue(GetFxObject(), reinterpret_cast<NETTXQUEUE_INIT*>(&context)),
        "Client failed tx queue creation. NETADAPTER=%p", GetFxObject());

    CX_RETURN_NTSTATUS_IF(
        STATUS_UNSUCCESSFUL,
        ! context.QueueContexts[0].CreatedQueueObject);

    m_txQueues[QueueId] = wistd::move(context.QueueContexts[0].CreatedQueueObject);
    *AdapterQueue = reinterpret_cast<NET_CLIENT_QUEUE>(
        GetTxQueueFromHandle((NETTXQUEUE)m_txQueues[QueueId].get()));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::CreateRxQueue(
    PVOID ClientQueue,
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
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! context.QueueContexts.resize(1));

    context.CurrentThread = KeGetCurrentThread();
    context.ClientQueueConfig = ClientQueueConfig;
    context.ClientDispatch = ClientDispatch;
    context.AdapterDispatch = AdapterDispatch;
    context.Adapter = this;
    context.QueueContexts[0].QueueId = QueueId;
    context.QueueContexts[0].ClientQueue = ClientQueue;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AddNetClientPacketExtensionsToQueueCreationContext(ClientQueueConfig, context),
        "Failed to transfer NetClientQueueConfig's packet extension to queue_creation_context");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_Config.EvtAdapterCreateRxQueue(GetFxObject(), reinterpret_cast<NETRXQUEUE_INIT*>(&context)),
        "Client failed rx queue creation. NETADAPTER=%p", GetFxObject());

    CX_RETURN_NTSTATUS_IF(
        STATUS_UNSUCCESSFUL,
        ! context.QueueContexts[0].CreatedQueueObject);

    m_rxQueues[QueueId] = wistd::move(context.QueueContexts[0].CreatedQueueObject);
    *AdapterQueue = reinterpret_cast<NET_CLIENT_QUEUE>(
        GetRxQueueFromHandle((NETRXQUEUE)m_rxQueues[QueueId].get()));

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

NTSTATUS
NxAdapter::CreateQueueGroup(
    PVOID ClientQueueContexts[],
    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    NET_CLIENT_QUEUE_GROUP_CONFIG const * ClientQueueGroupConfig,
    NET_CLIENT_QUEUE_GROUP * AdapterQueueGroup,
    NET_CLIENT_QUEUE AdapterQueues[],
    NET_CLIENT_QUEUE_DISPATCH const ** AdapterQueueDispatch
    )
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_NOT_SUPPORTED,
        ! m_Config.EvtAdapterCreateRssQueueGroup);

    QUEUE_GROUP_CREATION_CONTEXT context;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! context.QueueContexts.resize(ClientQueueGroupConfig->NumberOfQueues));

    context.CurrentThread = KeGetCurrentThread();
    context.ClientQueueConfig = &ClientQueueGroupConfig->QueueConfig;
    context.ClientQueueGroupConfig = ClientQueueGroupConfig;
    context.ClientDispatch = ClientDispatch;
    context.AdapterDispatch = AdapterQueueDispatch;
    context.Adapter = this;
    for (size_t i = 0; i < ClientQueueGroupConfig->NumberOfQueues; i++)
    {
        context.QueueContexts[i].QueueId;
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            AllocateQueueId(m_rxQueues, context.QueueContexts[i].QueueId),
            "Failed to allocate QueueId.");

        context.QueueContexts[i].ClientQueue = ClientQueueContexts[i];
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AddNetClientPacketExtensionsToQueueCreationContext(context.ClientQueueConfig, context),
        "Failed to transfer NetClientQueueConfig's packet extension to queue_creation_context");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_Config.EvtAdapterCreateRssQueueGroup(
            GetFxObject(),
            reinterpret_cast<NETRSSQUEUEGROUP_INIT*>(&context)),
        "Client failed rss queue creation. NETADAPTER=%p",
            GetFxObject());

    CX_RETURN_NTSTATUS_IF(STATUS_UNSUCCESSFUL, ! context.CreatedQueueGroupObject);

    auto queueGroup = GetQueueGroupFromHandle(
        static_cast<NETRSSQUEUEGROUP>(context.CreatedQueueGroupObject.release()));
    auto const & queues = queueGroup->GetQueueArray();

    NT_ASSERT(queues.count() == ClientQueueGroupConfig->NumberOfQueues);

    for (size_t i = 0; i < queues.count(); i++)
    {
#pragma warning(suppress : 26000) // size of `queues` is guaranteed to be equal to `NumberOfQueues`
        AdapterQueues[i] = reinterpret_cast<NET_CLIENT_QUEUE>(queues[i]);
    }

    *AdapterQueueGroup = reinterpret_cast<NET_CLIENT_QUEUE_GROUP>(queueGroup);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NONPAGEDX
void
NxAdapter::ReturnRxBuffer(
    PVOID RxBufferVa,
    PVOID RxBufferReturnContext
    )
{
    m_EvtReturnRxBuffer(GetFxObject(), RxBufferVa, RxBufferReturnContext);
}

void
NxAdapter::DestroyQueueGroup(
    NET_CLIENT_QUEUE_GROUP AdapterQueueGroup
    )
{
    auto queueGroup = reinterpret_cast<NxQueueGroup *>(AdapterQueueGroup);
    for (auto queue : queueGroup->GetQueueArray())
    {
        DestroyQueue(queue);
    }
    WdfObjectDelete(queueGroup->GetFxObject());
}

VOID
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
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter nxAdapter = GetNxAdapterFromHandle((NETADAPTER)NetAdapter);

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
            (PVOID)NxAdapter::_EvtCleanup);
        nxAdapter->m_DefaultRequestQueue = NULL;
    }

    if (nxAdapter->m_DefaultDirectRequestQueue) {
        WdfObjectDereferenceWithTag(nxAdapter->m_DefaultDirectRequestQueue->GetFxObject(),
            (PVOID)NxAdapter::_EvtCleanup);
        nxAdapter->m_DefaultDirectRequestQueue = NULL;
    }

    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());
    NxDriver *nxDriver = nxDevice->GetNxDriver();

    Verifier_VerifyAdapterCanBeDeleted(nxDriver->GetPrivateGlobals(), nxAdapter);

    //
    // If NetAdapterCreate fails the cleanup will be called, but the adapter
    // won't be in the device's collection, neither it will have a NDIS handle
    //
    if (nxDevice->RemoveAdapter(nxAdapter))
    {
        //
        // Since we're cleaning up the adapter we need to make sure we tell NDIS to complete the
        // miniport removal process, and also we need to tell the device this adapter is gone
        //
        NTSTATUS status = NdisWdfPnpPowerEventHandler(
            nxAdapter->GetNdisHandle(),
            NdisWdfActionDeviceObjectCleanup,
            NdisWdfActionPowerNone);

        if (!WIN_VERIFY(NT_SUCCESS(status)))
        {
            LogWarning(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
                "Unexpected NdisWdfPnpPowerEventHandler (NdisWdfActionDeviceObjectCleanup) failure, ignoring it. status=%!STATUS!", status);
        }
    }

    nxAdapter->EnqueueEvent(NxAdapter::Event::EvtCleanup);

    FuncExit(FLAG_ADAPTER);
    return;
}

NDIS_HANDLE
NxAdapter::_EvtNdisWdmDeviceGetNdisAdapterHandle(
    _In_ PDEVICE_OBJECT    DeviceObject
    )
/*++
Routine Description:
    A Ndis Event Callback

    This event callback converts a WDF Device Object to the NDIS's
    miniport Adapter handle.
--*/
{
    FuncEntry(FLAG_ADAPTER);
    WDFDEVICE device = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    PNxDevice nxDevice = GetNxDeviceFromHandle(device);
    FuncExit(FLAG_ADAPTER);
    return nxDevice->GetDefaultNxAdapter()->m_NdisAdapterHandle;
}

VOID
NxAdapter::_EvtNdisUpdatePMParameters(
    _In_ NDIS_HANDLE                    NxAdapterAsContext,
    _In_ PNDIS_PM_PARAMETERS            PMParameters
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
    FuncEntry(FLAG_ADAPTER);

    UNREFERENCED_PARAMETER(PMParameters);
    UNREFERENCED_PARAMETER(NxAdapterAsContext);

    FuncExit(FLAG_ADAPTER);
}

RECORDER_LOG
NxAdapter::GetRecorderLog(
    VOID
    )
{
    return m_NxDriver->GetRecorderLog();
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::InitializingReportToNdis()
{
    m_IsHalted.Clear();

    NTSTATUS status = NdisWdfPnpPowerEventHandler(
        GetNdisHandle(),
        NdisWdfActionPnpStart,
        NdisWdfActionPowerNone);

    if (!WIN_VERIFY(NT_SUCCESS(status)))
    {
        LogWarning(GetRecorderLog(), FLAG_ADAPTER,
            "NdisWdfPnpPowerEventHandler (NdisWdfActionPnpStart) failure. status=%!STATUS!", status);
    }

    // If this adapter was created during EvtDriverDeviceAdd we need to tell the device we're done initializing this adapter
    SignalMiniportStartComplete(status);

    NxDevice *nxDevice = GetNxDeviceFromHandle(m_Device);

    // If this device already started we need to tell NDIS.
    //
    // This assumes drivers can only create adapters in
    // EvtDriverDeviceAdd or after the device is started.
    //
    if (nxDevice->IsStarted())
        NdisWdfMiniportStarted(m_NdisAdapterHandle);

    //
    // If self managed IO started we need to tell NDIS
    //
    // This is fine because self managed IO starts once
    // per adapter's lifetime (it can be suspended and
    // restarted, but not started).
    //
    if (nxDevice->IsSelfManagedIoStarted())
        InitializeSelfManagedIO();

    return NT_SUCCESS(status) ? NxAdapter::Event::SyncSuccess :
                                NxAdapter::Event::SyncFail;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::NdisInitialize(
    _In_ PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters
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
    UNREFERENCED_PARAMETER(MiniportInitParameters);

    NTSTATUS status;
    PNxDevice nxDevice;

    nxDevice = GetNxDeviceFromHandle(m_Device);

    status = SetRegistrationAttributes();
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    // EvtAdapterSetCapabilites is a mandatory event callback
    ClearGeneralAttributes();

    status = m_Config.EvtAdapterSetCapabilities(GetFxObject());

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "Client EvtAdapterSetCapabilities failed %!STATUS!", status);
        goto Exit;
    }

    status = SetGeneralAttributes();
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    //
    // The default pause state for NDIS is paused
    //
    m_Flags.PauseReasonNdis = TRUE;
    m_Flags.PauseReasonWdf = FALSE;

Exit:
    //
    // Inform the device (state machine) that the adapter initialization is
    // complete. It will process the initialization status to handle device
    // failure appropriately.
    //
    nxDevice->AdapterInitialized(status);

    m_StateChangeStatus = status;

    m_StateChangeComplete.Set();

    return status;
}

VOID
NxAdapter::_EvtStartDatapathWorkItem(
    _In_
    WDFWORKITEM WorkItem
    )
{
    PNxAdapter nxAdapter = GetNxAdapterFromHandle((NETADAPTER)WdfWorkItemGetParentObject(WorkItem));

    NdisWdfMiniportDataPathStart(nxAdapter->m_NdisAdapterHandle);

    // Now it is ok to receive a Halt, so let a possibly blocked suspend self managed IO go
    nxAdapter->m_StartDatapathWorkItemDone.Set();
}

_Use_decl_annotations_
VOID
NxAdapter::_EvtStopAdapterWorkItem(
    WDFWORKITEM WorkItem
    )
/*

Description:

    This work item is queued when NetAdapterStop is called while the adapter state machine is in the
    'AdapterRunning' state. This happens because we need to tell NDIS this adapter needs to be removed,
    but we can't do from the state machine thread, otherwise we're going to cause a deadlock.
*/
{
    NETADAPTER adapter = static_cast<NETADAPTER>(WdfWorkItemGetParentObject(WorkItem));
    NxAdapter *nxAdapter = GetNxAdapterFromHandle(adapter);

    //
    // Since this adapter is stopping let's call NdisWdfActionPreReleaseHardware and NdisWdfActionPostReleaseHardware,
    // as if the device was being removed
    //

    NTSTATUS status = NdisWdfPnpPowerEventHandler(
        nxAdapter->GetNdisHandle(),
        NdisWdfActionPreReleaseHardware,
        NdisWdfActionPowerNone);

    if (!WIN_VERIFY(NT_SUCCESS(status)))
    {
        LogWarning(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
            "Unexpected NdisWdfPnpPowerEventHandler (NdisWdfActionPostReleaseHardware) failure, ignoring it. status=%!STATUS!", status);
    }

    status = NdisWdfPnpPowerEventHandler(
        nxAdapter->GetNdisHandle(),
        NdisWdfActionPostReleaseHardware,
        NdisWdfActionPowerNone);

    if (!WIN_VERIFY(NT_SUCCESS(status)))
    {
        LogWarning(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
            "Unexpected NdisWdfPnpPowerEventHandler (NdisWdfActionPostReleaseHardware) failure, ignoring it. status=%!STATUS!", status);
    }
}

_Use_decl_annotations_
VOID
NxAdapter::RestartDatapathAfterStop()
{
    // We need to start the datapath in a separate thread
    m_StartDatapathWorkItemDone.Clear();
    WdfWorkItemEnqueue(m_StartDatapathWorkItem);
}

_Use_decl_annotations_
VOID
NxAdapter::ClientDataPathPaused()
{
    ASSERT(AnyReasonToBePaused());

    m_Flags.NdisMiniportRestartInProgress = FALSE;

    m_IsPaused.Set();
}

_Use_decl_annotations_
VOID
NxAdapter::NdisHalt()
{
    PNxDevice nxDevice;

    nxDevice = GetNxDeviceFromHandle(m_Device);

    m_SharedPacketExtensions.resize(0);

    //
    // Report the adapter halt to the device
    //
    nxDevice->AdapterHalting();
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::ShouldClientDataPathStartForSelfManagedIoRestart()
{
    m_Flags.PauseReasonWdf = FALSE;

    if (AnyReasonToBePaused()) {
        return NxAdapter::Event::No;
    }

    return NxAdapter::Event::Yes;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::ShouldClientDataPathStartForNdisRestart()
{
    m_Flags.PauseReasonNdis = FALSE;

    if (AnyReasonToBePaused()) {
        //
        // We are not starting the client's data path but must tell NDIS
        // it is ok to start the data path.
        //
        m_Flags.NdisMiniportRestartInProgress = FALSE;
        NdisMRestartComplete(m_NdisAdapterHandle,
                            NdisConvertNtStatusToNdisStatus(STATUS_SUCCESS));
        return NxAdapter::Event::No;
    }
    else {
        //
        // No reason to not start the data path.
        //
        m_Flags.NdisMiniportRestartInProgress = TRUE;
        return NxAdapter::Event::Yes;
    }
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::ClientDataPathStarting()
{
    //
    // Allow client to Rx
    //
    m_IsPaused.Clear();

    auto startTranslationResult =
        CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            CreateDatapath(),
            "Failed to start translator queues. Adapter=%p", this);

    return NT_SUCCESS(startTranslationResult) ? NxAdapter::Event::ClientStartSucceeded : NxAdapter::Event::ClientStartFailed;
}

_Use_decl_annotations_
VOID
NxAdapter::ClientDataPathStartFailed()
{
    //
    // Undo what was done in the starting state and call restartComplete if
    // appropraite
    //

    if (m_Flags.NdisMiniportRestartInProgress) {
        m_Flags.NdisMiniportRestartInProgress = FALSE;

        m_Flags.PauseReasonNdis = TRUE;

        NdisMRestartComplete(m_NdisAdapterHandle,
                   NdisConvertNtStatusToNdisStatus(m_StateChangeStatus));
    }

    //
    // Trigger removal of the device
    //
    LogError(GetRecorderLog(), FLAG_ADAPTER,
        "Failed to initialize datapath, will trigger device removal.");

    WdfDeviceSetFailed(m_Device, WdfDeviceFailedNoRestart);
}

_Use_decl_annotations_
VOID
NxAdapter::ClientDataPathStarted()
{
    ASSERT(!AnyReasonToBePaused());

    LogVerbose(GetRecorderLog(), FLAG_ADAPTER, "Calling NdisMRestartComplete");

    if (m_Flags.NdisMiniportRestartInProgress) {
        m_Flags.NdisMiniportRestartInProgress = FALSE;
        NdisMRestartComplete(m_NdisAdapterHandle,
                    NdisConvertNtStatusToNdisStatus(m_StateChangeStatus));
    }
}

_Use_decl_annotations_
VOID
NxAdapter::ClientDataPathPausing()
{
    DestroyDatapath();
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::ClientDataPathPauseComplete()
{
    m_IsPaused.Set();

    if (m_Flags.PauseReasonNdis) {
        //
        // Take into account that we could be pausing due to self managed IO
        // suspend.
        //
        LogVerbose(GetRecorderLog(), FLAG_ADAPTER, "Calling NdisMPauseComplete");
        NdisMPauseComplete(m_NdisAdapterHandle);
    }

    // If NetAdapterStop was called while the adapter was running we need to transition
    // to an intermediate state
    if (m_Flags.PauseReasonStopping)
        return NxAdapter::Event::AdapterStopping;

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
VOID
NxAdapter::PauseClientDataPathForNdisPause()
{
    m_Flags.PauseReasonNdis = TRUE;
}

_Use_decl_annotations_
VOID
NxAdapter::PauseClientDataPathForNdisPause2()
{
    m_Flags.PauseReasonNdis = TRUE;

    //
    // Client is already paused so we can call NdisMPauseComplete right away.
    //
    NdisMPauseComplete(m_NdisAdapterHandle);
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::PauseClientDataPathForSelfManagedIoSuspend()
{
    m_Flags.PauseReasonWdf = TRUE;

    //
    // Tell the SMIOSuspend callback this event has been picked up.
    //
    m_StateChangeComplete.Set();
}

_Use_decl_annotations_
VOID
NxAdapter::PauseClientDataPathForStop()
{
    m_Flags.PauseReasonStopping = TRUE;
}

_Use_decl_annotations_
VOID
NxAdapter::StoppingAdapterReportToNdis()
{
    WdfWorkItemEnqueue(m_StopAdapterWorkItem);
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::ShouldAdapterStartForPnpStart()
{
    m_Flags.StopReasonPnp = FALSE;

    if (AnyReasonToBeStopped())
        return NxAdapter::Event::No;

    return NxAdapter::Event::Yes;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::ShouldAdapterStartForNetAdapterStart()
{
    m_Flags.StopReasonApi = FALSE;

    if (AnyReasonToBeStopped())
        return NxAdapter::Event::No;

    return NxAdapter::Event::Yes;
}

_Use_decl_annotations_
VOID
NxAdapter::AdapterHalted()
{
    m_IsHalted.Set();
}

VOID
NxAdapter::InitializeSelfManagedIO(
    VOID
    )
/*++
Routine Description:
    This is called by the NxDevice to inform the adapter that self managed IO
    initialize callback for the device has been invoked.
--*/
{
    NdisWdfMiniportDataPathStart(m_NdisAdapterHandle);
}

VOID
NxAdapter::SuspendSelfManagedIO(
    VOID
    )
/*++
Routine Description:
    This is called by the NxDevice to inform the adapter that self managed IO
    suspend callback for the device has been invoked.
--*/
{
    EnqueueEvent(NxAdapter::Event::SuspendSelfManagedIo);

    //
    // Cx guarantees data path is paused before client's SMIO suspend callback
    // is invoked and device removal or power down proceeds further.
    // These events are signaled by the adapter state machine once it has processed
    // self managed IO suspend and the adapter is paused.
    //
    (VOID) WaitForStateChangeResult();

    m_IsPaused.Wait();

    // During PnP rebalance, make sure we don't return from SuspendSelfManagedIo before
    // _EvtStartDatapathWorkItem has finished, this will prevent a halt becore we call
    // NdisWdfMiniportDataPathStart

    m_StartDatapathWorkItemDone.Wait();
}

VOID
NxAdapter::RestartSelfManagedIO(
    VOID
    )
/*++
Routine Description:
    This is called by the NxDevice to inform the adapter that self managed IO
    restart callback for the device has been invoked.
--*/
{
    //
    // Cx guarantees client's SMIORestart callback will be called before the
    // data path has started. At this point client's SMIORestart callback
    // should've already been invoked by the NxDevice.
    //
    EnqueueEvent(NxAdapter::Event::RestartSelfManagedIo);
}

NTSTATUS
NxAdapter::WaitForStateChangeResult(
    VOID
    )
{
    m_StateChangeComplete.Wait();

    return m_StateChangeStatus;
}

void
NxAdapter::SignalMiniportStartComplete(
    _In_ NTSTATUS Status
    )
{
    m_MiniportStartStatus = Status;
    m_MiniportStartComplete.Set();
}

NTSTATUS
NxAdapter::WaitForMiniportStart(
    void
    )
{
    m_MiniportStartComplete.Wait();
    return m_MiniportStartStatus;
}

bool
NxAdapter::AnyReasonToBePaused(
    void
    ) const
{
    return m_Flags.PauseReasonNdis || m_Flags.PauseReasonWdf || m_Flags.PauseReasonStopping;
}

bool
NxAdapter::AnyReasonToBeStopped(
    void
    ) const
{
    return m_Flags.StopReasonPnp || m_Flags.StopReasonApi;
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
VOID
NxAdapter::EvtLogTransition(
    _In_ SmFx::TransitionType TransitionType,
    _In_ StateId SourceState,
    _In_ EventId ProcessedEvent,
    _In_ StateId TargetState
    )
{
    FuncEntry(FLAG_ADAPTER);

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
        TraceLoggingHexInt32(static_cast<INT32>(TargetState), "AdapterStateTransitionTo")
        );

    FuncExit(FLAG_ADAPTER);
}

_Use_decl_annotations_
VOID
NxAdapter::EvtLogMachineException(
    _In_ SmFx::MachineException exception,
    _In_ EventId relevantEvent,
    _In_ StateId relevantState
    )
{
    FuncEntry(FLAG_ADAPTER);
    UNREFERENCED_PARAMETER((exception, relevantEvent, relevantState));


    if (!KdRefreshDebuggerNotPresent())
    {
        DbgBreakPoint();
    }

    FuncExit(FLAG_ADAPTER);
}

_Use_decl_annotations_
VOID
NxAdapter::EvtMachineDestroyed()
{
    FuncEntry(FLAG_ADAPTER);
    FuncExit(FLAG_ADAPTER);
}

_Use_decl_annotations_
VOID
NxAdapter::EvtLogEventEnqueue(
    _In_ EventId relevantEvent
    )
{
    FuncEntry(FLAG_ADAPTER);
    UNREFERENCED_PARAMETER(relevantEvent);
    FuncExit(FLAG_ADAPTER);
}

_Use_decl_annotations_
NET_PACKET_EXTENSION_PRIVATE*
NxAdapter::QueryAndGetNetPacketExtension(
    NET_PACKET_EXTENSION_PRIVATE const * ExtensionToQuery
)
{
    for (size_t i = 0; i < m_SharedPacketExtensions.count(); i++)
    {
        if ((0 == wcscmp(m_SharedPacketExtensions[i].m_extension.Name, ExtensionToQuery->Name)) &&
            (m_SharedPacketExtensions[i].m_extension.Version >= ExtensionToQuery->Version))
        {
            return &m_SharedPacketExtensions[i].m_extension;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
NET_PACKET_EXTENSION_PRIVATE*
NxAdapter::QueryAndGetNetPacketExtensionWithLock(
    NET_PACKET_EXTENSION_PRIVATE const * ExtensionToQuery
    )
{
    auto locked = wil::acquire_wdf_wait_lock(m_ExtensionCollectionLock);
    return QueryAndGetNetPacketExtension(ExtensionToQuery);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::RegisterPacketExtension(
    NET_PACKET_EXTENSION_PRIVATE const * ExtensionToAdd
    )
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_DUPLICATE_NAME,
        QueryAndGetNetPacketExtension(ExtensionToAdd) != nullptr);

    NxPacketExtensionPrivate extension;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        extension.Initialize(ExtensionToAdd));

    auto locked = wil::acquire_wdf_wait_lock(m_ExtensionCollectionLock);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_SharedPacketExtensions.append(wistd::move(extension)));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::ReceiveScalingEnable(
    _In_ NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE HashType,
    _In_ NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE ProtocolType
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    m_ReceiveScaling.HashType = HashType;
    m_ReceiveScaling.ProtocolType = ProtocolType;

    // if we are asked to enable receive scaling and the datapath
    // is already running we are free to notify the driver immediately
    // otherwise we have to wait until the datapath is created
    if (m_ReceiveScaling.DatapathRunning)
    {
        if (m_ReceiveScaling.Enabled)
        {
            m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingDisable(
                GetFxObject());
            m_ReceiveScaling.Enabled = false;
        }

        status = m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingEnable(
            GetFxObject());

        if (NT_SUCCESS(status))
        {
            m_ReceiveScaling.Enabled = true;
        }
    }
    else
    {
        m_ReceiveScaling.Enabled = true;
    }

    return status;
}

void
NxAdapter::ReceiveScalingDisable(
    void
    )
{
    m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingDisable(
        GetFxObject());
    m_ReceiveScaling.Enabled = false;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::ReceiveScalingSetIndirectionEntries(
    _Inout_ NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES * IndirectionEntries
    )
{
    auto table = reinterpret_cast<NET_ADAPTER_RECEIVE_SCALING_INDIRECTION_ENTRIES *>(IndirectionEntries);

    for (size_t i = 0; i < IndirectionEntries->Count; i++)
    {
        auto queue = reinterpret_cast<NxRxQueue *>(IndirectionEntries->Entries[i].Queue);

        table->Entries[i].Queue = reinterpret_cast<NETRXQUEUE>(queue->GetWdfObject());
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
    RtlCopyMemory(&m_ReceiveScaling.HashSecretKey, HashSecretKey, sizeof(m_ReceiveScaling.HashSecretKey));

    return m_ReceiveScalingCapabilities.EvtAdapterReceiveScalingSetHashSecretKey(
        GetFxObject(),
        &m_ReceiveScaling.HashSecretKey);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::QueryRegisteredPacketExtension(
    NET_PACKET_EXTENSION_PRIVATE const * ExtensionToQuery
    )
{
    return (QueryAndGetNetPacketExtensionWithLock(ExtensionToQuery) != nullptr) ?
               STATUS_SUCCESS : STATUS_NOT_FOUND;
}

_Use_decl_annotations_
VOID
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

