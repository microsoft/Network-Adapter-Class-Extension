/*++
Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:
    NxAdapter.cpp
Abstract:
    This is the main NetAdapterCx driver framework.


Environment:
    kernel mode only

Revision History:


--*/

#include "Nx.hpp"
#include <nblutil.h>

// Tracing support
extern "C" {
#include "NxAdapter.tmh"
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

    RtlZeroMemory(&m_SmEngineContext, sizeof(m_SmEngineContext));
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

    KeInitializeEvent(&m_IsPaused, SynchronizationEvent, TRUE);

    // Currently we have a 1:1 mapping between WDFDEVICE and NETADAPTER
    m_Flags.IsDefault = TRUE;

    m_PowerRefFailureCount = 0;
    m_PowerRequiredWorkItemPending = FALSE;
    m_AoAcDisengageWorkItemPending = FALSE;




   
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
    KeInitializeEvent(&m_StartDatapathWorkItemDone, NotificationEvent, TRUE);

    status = StateMachineEngine_Init(&m_SmEngineContext,
                (SMOWNER)GetFxObject(),          // Owner object
                WdfDeviceWdmGetDeviceObject(m_Device), // For work item allocation
                (PVOID)this,                     // Context
                NxAdapterStateAdapterHaltedIndex, // Starting state index
                NULL,                            // Optional shared lock
                NxAdapterStateTable,             // State table
                NULL, NULL, NULL, NxAdapterStateTransitionTracing);         // Fn pointers for ref counting and tracing.
    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_ADAPTER,
            "StateMachineEngine_Init failed %!STATUS!", status);
        return status;
    }

    KeInitializeEvent(&m_StateChangeComplete, SynchronizationEvent, FALSE);

    return status;
}

NxAdapter::~NxAdapter()
{
    FuncEntry(FLAG_ADAPTER);

    StateMachineEngine_ReleaseResources(&m_SmEngineContext);

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

    // Opaque Handle for the Net Adapter given by ndis.sys
    NDIS_HANDLE           ndisAdapterHandle;

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
        status = nxDevice->AdapterAdd((NDIS_HANDLE)nxAdapter,
                                        &ndisAdapterHandle);
        if (!NT_SUCCESS(status)) {
            FuncExit(FLAG_ADAPTER);
            return status;
        }
    } else {
        FuncExit(FLAG_ADAPTER);
        return STATUS_NOT_SUPPORTED;
    }

    nxAdapter->m_NdisAdapterHandle = ndisAdapterHandle;

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

    nxAdapter->m_NblDatapath.SetNdisHandle(ndisAdapterHandle);

    *Adapter = nxAdapter;
    
    FuncExit(FLAG_ADAPTER);
    return status;
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

VOID
NxAdapter::_EvtNetPacketClientGetExtensions(PVOID ClientContext)
{
    auto nxAdapter = static_cast<PNxAdapter>(ClientContext);

    UNREFERENCED_PARAMETER(nxAdapter);
}

VOID
NxAdapter::_EvtNetPacketClientStart(PVOID ClientContext)
{
    UNREFERENCED_PARAMETER(ClientContext);

    auto nxAdapter = static_cast<PNxAdapter>(ClientContext);

    StateMachineEngine_EventAdd(&nxAdapter->m_SmEngineContext,
                                NxAdapterEventPacketClientStart);
}

VOID
NxAdapter::_EvtNetPacketClientPause(PVOID ClientContext)
{
    UNREFERENCED_PARAMETER(ClientContext);
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
    a context in the WDFDEVICE object. This way we can ensure the miniport block
    will be valid memory during the DEVICE_OBJECT life time.
--*/
{
    FuncEntry(FLAG_ADAPTER);

    NTSTATUS status = STATUS_SUCCESS;
    PNxAdapter nxAdapter = static_cast<PNxAdapter>(NxAdapterAsContext);
    WDFDEVICE device = nxAdapter->GetDevice();


    WDF_OBJECT_ATTRIBUTES contextAttributes;

    // Initialize contextAttributes with our dummy type
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &contextAttributes,
        NDIS_MINIPORT_BLOCK_TYPE
        );

    // Override the memory size to allocate with what NDIS told us
    contextAttributes.ContextSizeOverride = Size;

    status = WdfObjectAllocateContext(device, &contextAttributes, MiniportBlock);

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

    if (m_PowerCapabilities.Size == 0) {
        LogError(GetRecorderLog(), FLAG_ADAPTER, 
                 "Client missed or incorrectly called NetAdapterSetPowerCapabilities");
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
    adapterGeneral.MacAddressLength = m_LinkLayerCapabilities.PhysicalAddress.Length;

    NT_ASSERT(m_LinkLayerCapabilities.PhysicalAddress.Length <= NDIS_MAX_PHYS_ADDRESS_LENGTH);

    if (m_LinkLayerCapabilities.PhysicalAddress.Length != 0) {
        RtlCopyMemory(adapterGeneral.CurrentMacAddress,
                      m_LinkLayerCapabilities.PhysicalAddress.CurrentAddress, 
                      NDIS_MAX_PHYS_ADDRESS_LENGTH);

        RtlCopyMemory(adapterGeneral.PermanentMacAddress,
                      m_LinkLayerCapabilities.PhysicalAddress.PermanentAddress, 
                      NDIS_MAX_PHYS_ADDRESS_LENGTH);
    }

    //
    // Set the power management capabilities.  
    //
    NDIS_PM_CAPABILITIES_INIT_FROM_NET_ADAPTER_POWER_CAPABILITIES(&pmCapabilities, 
                                                                  &m_PowerCapabilities);

    adapterGeneral.PowerManagementCapabilitiesEx = &pmCapabilities;

    adapterGeneral.SupportedOidList = NULL;
    adapterGeneral.SupportedOidListLength = 0;

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
    adapterGeneral.RecvScaleCapabilities = NULL;
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

    nxAdapter->NdisHalt();

    FuncExit(FLAG_ADAPTER);
}

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
    FuncEntry(FLAG_ADAPTER);

    UNREFERENCED_PARAMETER(MiniportInitParameters);

    StateMachineEngine_EventAdd(&m_SmEngineContext, 
                    NxAdapterEventNdisMiniportInitializeEx);

    FuncExit(FLAG_ADAPTER);

    return WaitForStateChangeResult();
}

VOID
NxAdapter::NdisHalt()
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

    StateMachineEngine_EventAdd(&m_SmEngineContext, 
                                NxAdapterEventNdisMiniportHalt);

    m_PacketClient.reset();

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
    return (KeReadStateEvent(&m_IsPaused) == 0);
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

    StateMachineEngine_EventAdd(&(nxAdapter->m_SmEngineContext),
                                NxAdapterEventNdisMiniportPause);

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

    StateMachineEngine_EventAdd(&nxAdapter->m_SmEngineContext,
                                NxAdapterEventNdisMiniportRestart);

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

    switch (NdisOidRequest->RequestType) {
    case NdisRequestSetInformation:

        Set = &NdisOidRequest->DATA.SET_INFORMATION;

        switch(Set->Oid) {
        case OID_PNP_SET_POWER:
            
            //
            // A Wdf Client has no need to handle the OID_PNP_SET_POWER
            // request. It leverages standard Wdf power callbacks. 
            //

            //
            // NOTE: NDIS Contract: After this is completed successfully for a Dx transition
            // client must not indicate any packets, issues status indications, or 
            // generally interact with NDIS. 


            //
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
    _In_ NDIS_HANDLE       NxAdapterAsContext,
    _In_ PNDIS_OID_REQUEST NdisOidRequest
    )
{
    NDIS_STATUS ndisStatus;
    FuncEntry(FLAG_ADAPTER);

    PNxAdapter nxAdapter = (PNxAdapter) NxAdapterAsContext;

    //
    // Send the OID to the client
    //

    if (nxAdapter->m_DefaultDirectRequestQueue == NULL) {
        LogWarning(nxAdapter->GetRecorderLog(), FLAG_ADAPTER,
                   "Rejecting OID, client didn't create a default direct queue");
        return NDIS_STATUS_NOT_SUPPORTED;            
    }

    nxAdapter->m_DefaultDirectRequestQueue->QueueNdisOidRequest(NdisOidRequest);

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
NxAdapter::InitializeDatapathApp(
    _In_ INxAppFactory *factory)
{
    INxApp *newApp;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        factory->CreateApp(&newApp),
        "Create NxApp failed. NxAdapter=%p", this);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_Apps.append(wistd::unique_ptr<INxApp> { newApp }));

    return STATUS_SUCCESS;
}

NTSTATUS NxAdapter::StartDatapath()
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDRIVER driver = WdfGetDriver();
    CxDriverContext *driverContext = GetCxDriverContextFromHandle(driver);

    status = InitializeDatapathApp(&driverContext->TranslationAppFactory);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    for (auto &app : m_Apps)
    {
        NTSTATUS appStartStatus = CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            app->Start(this),
            "Failed to start datapath app. Adapter=%p", this);

        status = NT_SUCCESS(status) ? appStartStatus : status;
    }

    if (!NT_SUCCESS(status))
    {
        // If any app failed to start, unwind them all.
        StopDatapath();
    }

    return status;
}

void NxAdapter::StopDatapath()
{
    m_Apps.resize(0);
}

VOID
NxAdapter::DataPathStart()
/*++
Routine Description: 
    This member function calls into Ndis to report that Data path can be started
 
    Note: the data path may not start right away since it is paused do to other
    reasons. 
--*/
{
    FuncEntry(FLAG_ADAPTER);

    LogVerbose(GetRecorderLog(), FLAG_ADAPTER, "Calling NdisWdfMiniportDataPathStart");

    FuncExit(FLAG_ADAPTER);

}

VOID
NxAdapter::DataPathPause()
/*++
Routine Description: 
    This member function calls into Ndis to report that Data path must be
    paused synchronously. 
--*/
{
    FuncEntry(FLAG_ADAPTER);

    LogVerbose(GetRecorderLog(), FLAG_ADAPTER, "Calling NdisWdfMiniportDataPathPause");

    FuncExit(FLAG_ADAPTER);

}

NTSTATUS 
NxAdapter::OpenQueue(
    _In_ INxAppQueue *appQueue,
    _In_ NxQueueType queueType,
    _Out_ INxAdapterQueue **adapterQueue)
{
    *adapterQueue = nullptr;

    QUEUE_CREATION_CONTEXT context;
    context.CurrentThread = KeGetCurrentThread();
    context.AppQueue = appQueue;
    context.Adapter = this;


    context.QueueId = (ULONG)queueType;

    switch (queueType)
    {
    case NxQueueType::Tx:
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            m_Config.EvtAdapterCreateTxQueue(GetFxObject(), reinterpret_cast<NETTXQUEUE_INIT*>(&context)),
            "Client failed tx queue creation. NETADAPTER=%p", GetFxObject());
    }
        break;
    case NxQueueType::Rx:
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            m_Config.EvtAdapterCreateRxQueue(GetFxObject(), reinterpret_cast<NETRXQUEUE_INIT*>(&context)),
            "Client failed rx queue creation. NETADAPTER=%p", GetFxObject());
    }
        break;
    default:
        WIN_ASSERT(FALSE);
        return STATUS_INVALID_PARAMETER_2;
    }

    CX_RETURN_NTSTATUS_IF(STATUS_UNSUCCESSFUL, !context.CreatedQueueObject);

    NxQueue* queue;
    if (queueType == NxQueueType::Tx)
    {
        queue = GetTxQueueFromHandle((NETTXQUEUE)context.CreatedQueueObject.get());
    }
    else
    {
        queue = GetRxQueueFromHandle((NETRXQUEUE)context.CreatedQueueObject.get());
    }

    wistd::unique_ptr<NxAdapterQueue> newAdapterQueue{ new (std::nothrow) NxAdapterQueue(wistd::move(context.CreatedQueueObject), *queue) };
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !newAdapterQueue);

    *adapterQueue = newAdapterQueue.release();

    return STATUS_SUCCESS;
}

NDIS_MEDIUM
NxAdapter::GetMediaType()
{
    return m_MediaType;
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

    if (nxAdapter != NULL) {
        
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

    }

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

__drv_maxIRQL(DISPATCH_LEVEL)
SM_ENGINE_EVENT
NxAdapter::_StateEntryFn_NdisInitialize(
    _In_ PNxAdapter This
    )
{
    NTSTATUS status;
    PNxDevice nxDevice;

    nxDevice = GetNxDeviceFromHandle(This->m_Device);

    status = This->SetRegistrationAttributes();
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    // EvtAdapterSetCapabilites is a mandatory event callback
    This->ClearGeneralAttributes();

    status = This->m_Config.EvtAdapterSetCapabilities(This->GetFxObject());

    if (!NT_SUCCESS(status)) {
        LogError(This->GetRecorderLog(), FLAG_ADAPTER,
            "Client EvtAdapterSetCapabilities failed %!STATUS!", status);
        goto Exit;
    }

    status = This->SetGeneralAttributes();
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    //
    // The default pause state for NDIS and the PacketClient is paused
    //
    This->m_Flags.PauseReasonNdis = TRUE;
    This->m_Flags.PauseReasonPacketClient = TRUE;
    This->m_Flags.PauseReasonWdf = FALSE;

Exit:
    //
    // Inform the device (state machine) that the adapter initialization is 
    // complete. It will process the initialization status to handle device  
    // failure appropriately.
    //
    nxDevice->AdapterInitialized(status);

    This->m_StateChangeStatus = status;

    KeSetEvent(&This->m_StateChangeComplete, IO_NO_INCREMENT, FALSE);

    return NT_SUCCESS(status) ? NxAdapterEventSyncSuccess : 
                                NxAdapterEventSyncFail;
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
    KeSetEvent(&nxAdapter->m_StartDatapathWorkItemDone, IO_NO_INCREMENT, FALSE);
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_RestartDatapathAfterStop(
    _In_ PNxAdapter This
    )
{
    This->m_PacketClient.reset(
        NetPacketRegisterClient(
            _EvtNetPacketClientGetExtensions,
            _EvtNetPacketClientStart,
            _EvtNetPacketClientPause,
            nullptr,
            This));

    NT_FRE_ASSERT(This->m_PacketClient);

    // We need to start the datapath in a separate thread
    KeClearEvent(&This->m_StartDatapathWorkItemDone);
    WdfWorkItemEnqueue(This->m_StartDatapathWorkItem);
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_ClientDataPathPaused(
    _In_ PNxAdapter  This
    )
{
    ASSERT(This->AnyReasonToBePaused());

    This->m_Flags.NdisMiniportRestartInProgress = FALSE;

    KeSetEvent(&This->m_IsPaused, IO_NO_INCREMENT, FALSE);
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_NdisHalt(
    _In_ PNxAdapter This
    )
{
    PNxDevice nxDevice;

    nxDevice = GetNxDeviceFromHandle(This->m_Device);

    //
    // Report the adapter halt to the device
    //
    nxDevice->AdapterHalting();
}

__drv_maxIRQL(DISPATCH_LEVEL)
SM_ENGINE_EVENT
NxAdapter::_StateEntryFn_ShouldClientDataPathStartForSelfManagedIoRestart(
    _In_ PNxAdapter This
    )
{
    This->m_Flags.PauseReasonWdf = FALSE;

    if (This->AnyReasonToBePaused()) {
        return NxAdapterEventNo;
    }

    return NxAdapterEventYes;
}

__drv_maxIRQL(DISPATCH_LEVEL) 
SM_ENGINE_EVENT 
NxAdapter::_StateEntryFn_ShouldClientDataPathStartForNdisRestart(
    _In_ PNxAdapter This
    )
{
    This->m_Flags.PauseReasonNdis = FALSE;

    if (This->AnyReasonToBePaused()) {
        //
        // We are not starting the client's data path but must tell NDIS
        // it is ok to start the data path. 
        //
        This->m_Flags.NdisMiniportRestartInProgress = FALSE;
        NdisMRestartComplete(This->m_NdisAdapterHandle, 
                            NdisConvertNtStatusToNdisStatus(STATUS_SUCCESS));
        return NxAdapterEventNo;
    }
    else {
        //
        // No reason to not start the data path.
        //
        This->m_Flags.NdisMiniportRestartInProgress = TRUE;
        return NxAdapterEventYes;
    }
}


__drv_maxIRQL(DISPATCH_LEVEL)
SM_ENGINE_EVENT
NxAdapter::_StateEntryFn_ShouldClientDataPathStartForPacketClientStart(
    _In_ PNxAdapter  This
    )
{
    This->m_Flags.PauseReasonPacketClient = FALSE;

    if (This->AnyReasonToBePaused()) {
        return NxAdapterEventNo;
    }

    return NxAdapterEventYes;
}

__drv_maxIRQL(DISPATCH_LEVEL)
SM_ENGINE_EVENT
NxAdapter::_StateEntryFn_ClientDataPathStarting(
    _In_ PNxAdapter This
    )
{
    //
    // Allow client to Rx
    //
    KeClearEvent(&This->m_IsPaused);

    auto startTranslationResult =
        CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            This->StartDatapath(),
            "Failed to start translator queues. Adapter=%p", This);

    return NT_SUCCESS(startTranslationResult) ? NxAdapterEventClientStartSucceeded : NxAdapterEventClientStartFailed;
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_ClientDataPathStartFailed(
    _In_ PNxAdapter This
    )
{
    //
    // Undo what was done in the starting state and call restartComplete if
    // appropraite
    //
    This->DataPathPause();

    if (This->m_Flags.NdisMiniportRestartInProgress) {
        This->m_Flags.NdisMiniportRestartInProgress = FALSE;

        This->m_Flags.PauseReasonNdis = TRUE;

        NdisMRestartComplete(This->m_NdisAdapterHandle,
                   NdisConvertNtStatusToNdisStatus(This->m_StateChangeStatus));
    }

    //
    // Trigger removal of the device
    //
    LogError(This->GetRecorderLog(), FLAG_ADAPTER,
        "Failed to initialize datapath, will trigger device removal.");

    WdfDeviceSetFailed(This->m_Device, WdfDeviceFailedNoRestart);
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_ClientDataPathStarted(
    _In_ PNxAdapter This
    )
{
    ASSERT(! This->AnyReasonToBePaused());

    LogVerbose(This->GetRecorderLog(), FLAG_ADAPTER, "Calling NdisMRestartComplete");

    if (This->m_Flags.NdisMiniportRestartInProgress) {
        This->m_Flags.NdisMiniportRestartInProgress = FALSE;
        NdisMRestartComplete(This->m_NdisAdapterHandle,
                    NdisConvertNtStatusToNdisStatus(This->m_StateChangeStatus));
    }
}

__drv_maxIRQL(DISPATCH_LEVEL) 
SM_ENGINE_EVENT
NxAdapter::_StateEntryFn_ClientDataPathPausing(
    _In_ PNxAdapter This
    )
{
    This->StopDatapath();

    return NxAdapterEventClientPauseComplete;
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_ClientDataPathPauseComplete(
    _In_ PNxAdapter This
    )
{
    KeSetEvent(&This->m_IsPaused, IO_NO_INCREMENT, FALSE);

    if (This->m_Flags.PauseReasonNdis) {
        // 
        // Take into account that we could be pausing due to self managed IO 
        // suspend.
        //
        LogVerbose(This->GetRecorderLog(), FLAG_ADAPTER, "Calling NdisMPauseComplete");
        NdisMPauseComplete(This->m_NdisAdapterHandle);
    }

    if (This->m_Flags.PauseReasonPacketClient) {
        // 
        // Take into account that we could be pausing due to a packet client pause.
        //
        LogVerbose(This->GetRecorderLog(), FLAG_ADAPTER, "Calling NetPacketClientPauseComplete");
        NetPacketClientPauseComplete(This->m_PacketClient.get());
    }
}

__drv_maxIRQL(DISPATCH_LEVEL) 
VOID 
NxAdapter::_StateEntryFn_PauseClientDataPathForNdisPause(
    _In_ PNxAdapter This
    )
{
    This->m_Flags.PauseReasonNdis = TRUE;
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_PauseClientDataPathForNdisPause2(
    _In_ PNxAdapter This
    )
{
    This->m_Flags.PauseReasonNdis = TRUE;

    //
    // Client is already paused so we can call NdisMPauseComplete right away.
    //
    NdisMPauseComplete(This->m_NdisAdapterHandle);
}


__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_PauseClientDataPathForPacketClientPauseWhilePaused(
    _In_ PNxAdapter  This
    )
{
    This->m_Flags.PauseReasonPacketClient = TRUE;

    NetPacketClientPauseComplete(This->m_PacketClient.get());
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxAdapter::_StateEntryFn_PauseClientDataPathForPacketClientPauseWhileRunning(
    _In_ PNxAdapter  This
    )
{
    This->m_Flags.PauseReasonPacketClient = TRUE;
}

__drv_maxIRQL(DISPATCH_LEVEL) 
VOID 
NxAdapter::_StateEntryFn_PauseClientDataPathForSelfManagedIoSuspend(
    _In_ PNxAdapter This
    )
{
    This->m_Flags.PauseReasonWdf = TRUE;

    // 
    // Tell the SMIOSuspend callback this event has been picked up.
    //
    KeSetEvent(&This->m_StateChangeComplete, IO_NO_INCREMENT, FALSE);
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
    StateMachineEngine_EventAdd(&m_SmEngineContext,
                            NxAdapterEventInitializeSelfManagedIO);

    m_PacketClient.reset(
        NetPacketRegisterClient(
            _EvtNetPacketClientGetExtensions,
            _EvtNetPacketClientStart, 
            _EvtNetPacketClientPause,
            nullptr,
            this));
    NT_FRE_ASSERT(m_PacketClient);

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
    StateMachineEngine_EventAdd(&m_SmEngineContext,
                                NxAdapterEventSuspendSelfManagedIO);


    //
    // Cx guarantees data path is paused before client's SMIO suspend callback
    // is invoked and device removal or power down proceeds further. 
    // These events are signaled by the adapter state machine once it has processed
    // self managed IO suspend and the adapter is paused. 
    //
    (VOID) WaitForStateChangeResult();

    KeWaitForSingleObject(&m_IsPaused,
                        Executive,
                        KernelMode,
                        FALSE,
                        NULL);

    // During PnP rebalance, make sure we don't return from SuspendSelfManagedIo before
    // _EvtStartDatapathWorkItem has finished, this will prevent a halt becore we call
    // NdisWdfMiniportDataPathStart
    
    KeWaitForSingleObject(&m_StartDatapathWorkItemDone,
                        Executive,
                        KernelMode,
                        FALSE,
                        NULL);
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
    StateMachineEngine_EventAdd(&m_SmEngineContext,
                                NxAdapterEventRestartSelfManagedIO);

}

NTSTATUS
NxAdapter::WaitForStateChangeResult(
    VOID
    )
{
    KeWaitForSingleObject(&m_StateChangeComplete,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    return m_StateChangeStatus;
}

bool
NxAdapter::AnyReasonToBePaused()
{
    return m_Flags.PauseReasonNdis || m_Flags.PauseReasonPacketClient || m_Flags.PauseReasonWdf;
}

VOID
NxAdapterStateTransitionTracing(
    _In_        SMOWNER         Owner,
    _In_        PVOID           StateMachineContext,
    _In_        SM_ENGINE_STATE FromState,
    _In_        SM_ENGINE_EVENT Event,
    _In_        SM_ENGINE_STATE ToState
)
{
    // SMOWNER is NETADAPTER
    UNREFERENCED_PARAMETER(Owner);

    // StateMachineContext is NxAdapter object;
    NxAdapter* nxAdapter = reinterpret_cast<NxAdapter*>(StateMachineContext);
    WDFDEVICE wdfDevice = nxAdapter->GetDevice();
    NxDevice* nxDevice = GetNxDeviceFromHandle(wdfDevice);

    TraceLoggingWrite(
        g_hNetAdapterCxEtwProvider,
        "NxAdapterStateTransition",
        TraceLoggingDescription("NxAdapterStateTransition"),
        TraceLoggingKeyword(NET_ADAPTER_CX_NXADAPTER_STATE_TRANSITION),
        TraceLoggingUInt32(nxDevice->GetDeviceBusAddress(), "DeviceBusAddress"),
        TraceLoggingHexUInt64(nxAdapter->GetNetLuid().Value, "AdapterNetLuid"),
        TraceLoggingHexInt32(FromState, "AdapterStateTransitionFrom"),
        TraceLoggingHexInt32(Event, "AdapterStateTransitionEvent"),
        TraceLoggingHexInt32(ToState, "AdapterStateTransitionTo")
    );
}
