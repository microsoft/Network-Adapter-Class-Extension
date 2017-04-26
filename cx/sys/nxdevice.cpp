/*++
 
Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxDevice.cpp

Abstract:

    This is the main NetAdapterCx driver framework.





Environment:

    kernel mode only

Revision History:

--*/

#include "Nx.hpp"

// Tracing support
extern "C" {
#include "NxDevice.tmh"
}

//
// State machine entry functions aka. "State machine Operations"
//
__drv_maxIRQL(DISPATCH_LEVEL)
SM_ENGINE_EVENT
NxDevice::_StateEntryFn_StartingReportStartToNdis(
    _In_ PNxDevice  This
    )
{
    NTSTATUS status;

    LogVerbose(This->GetRecorderLog(), FLAG_DEVICE, "Calling NdisWdfPnpPowerEventHandler(Start)");

    //
    // Send this message to Ndis. NDIS should invoke adapter's MiniportInitialize 
    // in response
    //
    status = NdisWdfPnpPowerEventHandler(This->m_DefaultNxAdapter->m_NdisAdapterHandle,
                                    NdisWdfActionPnpStart, NdisWdfActionPowerNone);
    if (!NT_SUCCESS(status)) {
        This->m_StateChangeStatus = status;
        LogError(This->GetRecorderLog(), FLAG_DEVICE,
            "EvtDevicePrepareHardware NdisWdfActionPnpStart failed Device 0x%p, %!STATUS!",
            This->GetFxObject(), status);
        FuncExit(FLAG_DEVICE);
        return NxDevicePnPPwrEventSyncFail;
    }

    return NxDevicePnPPwrEventSyncSuccess;
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxDevice::_StateEntryFn_StartProcessingComplete(
    _In_ PNxDevice  This
    )
{
    KeSetEvent(&(This->m_StateChangeComplete), IO_NO_INCREMENT, FALSE);
}

VOID
NxDevice::AdapterInitialized(
    _In_ NTSTATUS InitializationStatus
    )
/*++
Routine Description:
    Invoked by adapter object as part of adapter initialization that is performed
    when NdisMiniportInitializeEx is invoked. InitializationStatus indicates 
    the status of the initialization.
--*/
{
    if (NT_SUCCESS(InitializationStatus)) {
        StateMachineEngine_EventAdd(&m_SmEngineContext,
                                NxDevicePnPPwrEventAdapterInitializeComplete);
    }
    else {
        m_StateChangeStatus = InitializationStatus;
        StateMachineEngine_EventAdd(&m_SmEngineContext, 
                                NxDevicePnPPwrEventAdapterInitializeFailed);
    }
}

NTSTATUS
NxDevice::AdapterAdd(
    _In_  NDIS_HANDLE  MiniportAdapterContext,
    _Out_ NDIS_HANDLE* NdisAdapterHandle
)
/*++
Routine Description:
    Invoked by adapter object to inform the device about adapter creation 
    in response to a client calling NetAdapterCreate from it's device add 
    callback. In response NxDevice will report NdisWdfPnPAddDevice to NDIS.
--*/
{
    NTSTATUS status;
    WDFDEVICE device;
    WDFDRIVER driver;

    device = GetFxObject();
    NT_ASSERT(device != NULL);
    driver = WdfDeviceGetDriver(device);

    status = NdisWdfPnPAddDevice(WdfDriverWdmGetDriverObject(driver),
                            WdfDeviceWdmGetPhysicalDevice(device),
                            NdisAdapterHandle,
                            MiniportAdapterContext);
    if (!NT_SUCCESS(status)) {
        //
        // NOTE: In case of failure, WDF object cleanup for WDFDEVICE will post 
        // the cleanup event to the device state machine. 
        //
        LogError(GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpAddDevice failed %!STATUS!", status);
        FuncExit(FLAG_DEVICE);
        return status;
    }

    StateMachineEngine_EventAdd(&m_SmEngineContext, 
                            NxDevicePnPPwrEventAdapterAdded);
    return status;
}

__drv_maxIRQL(DISPATCH_LEVEL)
SM_ENGINE_EVENT
NxDevice::_StateEntryFn_ReleasingIsSurpriseRemoved(
    _In_ PNxDevice  This
)
{
    if (This->m_SurpriseRemoved) {
        return NxDevicePnPPwrEventYes;
    }
    else {
        return NxDevicePnPPwrEventNo;
    }
}

__drv_maxIRQL(DISPATCH_LEVEL)
SM_ENGINE_EVENT
NxDevice::_StateEntryFn_ReleasingReportSurpriseRemoveToNdis(
    _In_ PNxDevice  This
    )
{
    NTSTATUS status;

    LogVerbose(This->GetRecorderLog(), FLAG_DEVICE, "Calling NdisWdfPnpPowerEventHandler(SurpriseRemove)");

    status = NdisWdfPnpPowerEventHandler(This->m_DefaultNxAdapter->m_NdisAdapterHandle,
                                        NdisWdfActionPnpSurpriseRemove, NdisWdfActionPowerNone);

    NT_ASSERT(NT_SUCCESS(status));

    if (!NT_SUCCESS(status)) {
        This->m_StateChangeStatus = status;

        LogError(This->GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpPowerEventHandler NdisWdfActionPnpSurpriseRemove failed NxDevice 0x%p, 0x%!STATUS!, Ignoring it",
            This, status);
        return NxDevicePnPPwrEventSyncFail;
    }

    return NxDevicePnPPwrEventSyncSuccess;
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxDevice::_StateEntryFn_RemovingReportRemoveToNdis(
    _In_ PNxDevice This
    )
{
    NTSTATUS status;

    LogVerbose(This->GetRecorderLog(), FLAG_DEVICE, "Calling NdisWdfPnpPowerEventHandler(Remove)");

    status = NdisWdfPnpPowerEventHandler(This->m_DefaultNxAdapter->m_NdisAdapterHandle,
        NdisWdfActionPnpRemove, NdisWdfActionPowerNone);

    NT_ASSERT(status == STATUS_SUCCESS);

    if (!NT_SUCCESS(status)) {
        LogError(This->GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpPowerEventHandler NdisWdfActionPnpRemove failed NxDevice 0x%p, 0x%!STATUS!, Ignoring it",
            This, status);
    }
}

__drv_maxIRQL(DISPATCH_LEVEL)
SM_ENGINE_EVENT
NxDevice::_StateEntryFn_ReleasingReportPreReleaseToNdis(
    _In_ PNxDevice This
)
{
    NTSTATUS status;

    LogVerbose(This->GetRecorderLog(), FLAG_DEVICE, "Calling NdisWdfPnpPowerEventHandler(PreRelease)");

    status = NdisWdfPnpPowerEventHandler(This->m_DefaultNxAdapter->m_NdisAdapterHandle,
                                        NdisWdfActionPreReleaseHardware, NdisWdfActionPowerNone);
    NT_ASSERT(NT_SUCCESS(status));
    if (!NT_SUCCESS(status)) {
        This->m_StateChangeStatus = status;

        LogError(This->GetRecorderLog(), FLAG_DEVICE,
            "EvtDeviceReleaseHardware NdisWdfActionPreReleaseHardware failed NxDevice 0x%p, 0x%!STATUS!\n",
            This, status);
        return NxDevicePnPPwrEventSyncFail;
    }

    return NxDevicePnPPwrEventSyncSuccess;
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxDevice::_StateEntryFn_ReleasingReleaseClient(
    _In_ PNxDevice  This
)
{
    // Let the client's EvtDeviceReleaseHardware be called
    KeSetEvent(&This->m_StateChangeComplete, IO_NO_INCREMENT, FALSE);
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxDevice::_StateEntryFn_ReleasingReportPostReleaseToNdis(
    _In_ PNxDevice This
    )
{
    NTSTATUS status;

    status = NdisWdfPnpPowerEventHandler(This->GetDefaultNxAdapter()->m_NdisAdapterHandle,
        NdisWdfActionPostReleaseHardware, NdisWdfActionPowerNone);

    NT_ASSERT(status == STATUS_SUCCESS);

    if (!NT_SUCCESS(status)) {
        LogError(This->GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpPowerEventHandler NdisWdfActionPostReleaseHardware failed NxDevice 0x%p, 0x%!STATUS!, Ignoring it",
            This, status);
    }
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxDevice::_StateEntryFn_RemovedReportRemoveToNdis(
    _In_ PNxDevice This
    )
{
    NTSTATUS status;
    PNxAdapter nxAdapter = This->GetDefaultNxAdapter();

    if (nxAdapter != nullptr)
    {
        status = NdisWdfPnpPowerEventHandler(nxAdapter->m_NdisAdapterHandle,
            NdisWdfActionDeviceObjectCleanup, NdisWdfActionPowerNone);

        NT_ASSERT(status == STATUS_SUCCESS);

        if (!NT_SUCCESS(status)) {
            LogError(This->GetRecorderLog(), FLAG_DEVICE,
                "NdisWdfPnpPowerEventHandler NdisWdfActionDeviceObjectCleanup failed NxDevice 0x%p, 0x%!STATUS!, Ignoring it",
                This, status);
        }
    }
}

__drv_maxIRQL(DISPATCH_LEVEL)
VOID
NxDevice::_StateEntryFn_StoppedPrepareForStart(
    _In_ PNxDevice  This
    )
/*
Description:

    This state entry function handles a PnP start after a PnP stop due to a
    resource rebalance
*/
{
    NTSTATUS status;
    PNxAdapter nxAdapter = This->GetDefaultNxAdapter();

    NT_ASSERT(nxAdapter != nullptr);

    // Call into NDIS to put the miniport in stopped state
    status = NdisWdfPnpPowerEventHandler(nxAdapter->m_NdisAdapterHandle,
        NdisWdfActionPnpRebalance, NdisWdfActionPowerNone);

    NT_ASSERT(status == STATUS_SUCCESS);

    if (!NT_SUCCESS(status)) {
        LogError(This->GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpPowerEventHandler NdisWdfActionPnpRebalance failed NxDevice 0x%p, 0x%!STATUS!, Ignoring it",
            This, status);
    }

    // Unblock PrePrepareHardware
    This->SignalStateChange(STATUS_SUCCESS);
}

NTSTATUS
NxDevice::_Create(
    _In_  PNX_PRIVATE_GLOBALS           PrivateGlobals,
    _In_  WDFDEVICE                     Device,
    _Out_ PNxDevice*                    NxDeviceParam
)
/*++
Routine Description: 
    Static method that creates the NXDEVICE object, corresponding to the
    WDFDEVICE.
 
    Currently NxDevice creation is closely tied to the creation of the
    default NxAdapter object. 
--*/
{
    FuncEntry(FLAG_DEVICE);
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES attributes;
    PNxDevice             nxDevice;

    PVOID                 nxDeviceMemory;

    //
    // First Apply a context on the device
    //
    
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxDevice);

    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxDevice::_EvtCleanup;

    //
    // Ensure that the destructor would be called when this object is distroyed.
    //
    NxDevice::_SetObjectAttributes(&attributes);

    status = WdfObjectAllocateContext(Device,
                                      &attributes,
                                      (PVOID*)&nxDeviceMemory);

    if (!NT_SUCCESS(status)) {
        LogError(PrivateGlobals->NxDriver->GetRecorderLog(), FLAG_DEVICE,
                 "WdfObjectAllocateContext failed %!STATUS!", status);
        FuncExit(FLAG_DEVICE);
        return status;
    }

    //
    // Use the inplacement new and invoke the constructor on the 
    // NxAdapter's memory
    //
    nxDevice = new (nxDeviceMemory) NxDevice(PrivateGlobals,
                                             Device);

    __analysis_assume(nxDevice != NULL);

    NT_ASSERT(nxDevice);

    KeInitializeEvent(&nxDevice->m_StateChangeComplete, SynchronizationEvent, FALSE);

    status = StateMachineEngine_Init(&nxDevice->m_SmEngineContext,
                    (SMOWNER)Device,                     // Owner object
                    WdfDeviceWdmGetDeviceObject(Device), // For work item allocation
                    (PVOID)nxDevice,                     // Context
                    NxDevicePnPPwrStateDeviceInitializedIndex, // Starting state index
                    NULL,                     // Optional shared lock
                    NxDevicePnPPwrStateTable, // State table
                    NULL, NULL, NULL, NxDeviceStateTransitionTracing);  // Fn pointers for ref counting and tracing.
    if (!NT_SUCCESS(status)) {
        LogError(PrivateGlobals->NxDriver->GetRecorderLog(), FLAG_DEVICE,
            "StateMachineEngine_Init failed %!STATUS!", status);
        FuncExit(FLAG_DEVICE);
        return status;
    }

    *NxDeviceParam = nxDevice;

    FuncExit(FLAG_DEVICE);
    return status;
}

VOID
NxDevice::SetDefaultNxAdapter(
    PNxAdapter NxAdapter
    )
{
    //
    // Store the nxAdapter pointer in NxDevice
    //
    NT_ASSERT(m_DefaultNxAdapter == NULL);
    m_DefaultNxAdapter = NxAdapter;
    WdfObjectReferenceWithTag(m_DefaultNxAdapter->GetFxObject(), (PVOID)NxDevice::_EvtCleanup);
}

NxDevice::NxDevice(
    _In_ PNX_PRIVATE_GLOBALS           NxPrivateGlobals,
    _In_ WDFDEVICE                     Device
    ):
    CFxObject(Device), 
    m_NxDriver(NxPrivateGlobals->NxDriver),
    m_StateChangeStatus(STATUS_SUCCESS),
    m_SystemPowerAction(PowerActionNone),
    m_LastReportedDxAction(NdisWdfActionPowerNone)
{
    FuncEntry(FLAG_DEVICE);

    RtlZeroMemory(&m_SmEngineContext, sizeof(m_SmEngineContext));

    IoInitializeRemoveLock(&m_RemoveLock, NETADAPTERCX_TAG, 0, 0);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    WDF_DEVICE_PROPERTY_DATA data;
    WDF_OBJECT_ATTRIBUTES objAttributes;
    DEVPROPTYPE devPropType;
    WDFMEMORY memory = nullptr;

    WDF_DEVICE_PROPERTY_DATA_INIT(&data, &DEVPKEY_Device_Address);
    WDF_OBJECT_ATTRIBUTES_INIT(&objAttributes);
    objAttributes.ParentObject = Device;

    status = WdfDeviceAllocAndQueryPropertyEx(
        Device,
        &data,
        PagedPool,
        &objAttributes,
        &memory,
        &devPropType);

    if ((NT_SUCCESS(status)) &&
        (devPropType == DEVPROP_TYPE_UINT32))
    {
        size_t bufferSize;
        PVOID buffer = WdfMemoryGetBuffer(memory, &bufferSize);
        if (bufferSize == sizeof(UINT32))
        {
            m_DeviceBusAddress = *((PUINT32)buffer);
        }
    }

    if (memory != nullptr)
    {
        WdfObjectDelete(memory);
    }

    m_Flags.InDxPowerTransition = FALSE;




   
    FuncExit(FLAG_DEVICE);
}

NxDevice::~NxDevice()
{
    FuncEntry(FLAG_DEVICE);

    StateMachineEngine_ReleaseResources(&m_SmEngineContext);

    FuncExit(FLAG_DEVICE);
}

NTSTATUS
NxDevice::_EvtCxDevicePrePrepareHardware(
    _In_ WDFDEVICE    Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    FuncEntry(FLAG_DEVICE);

    UNREFERENCED_PARAMETER((ResourcesRaw, ResourcesTranslated));
    NTSTATUS status;
    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    StateMachineEngine_EventAdd(&nxDevice->m_SmEngineContext,
        NxDevicePnPPwrEventCxPrePrepareHardware);

    status = nxDevice->WaitForStateChangeResult();

    NT_ASSERT(status == STATUS_SUCCESS);

    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::_EvtCxDevicePostPrepareHardware(
    _In_ WDFDEVICE    Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Called by WDF after a successful call to the client's
    EvtDevicePrepareHardware.

--*/
{
    FuncEntry(FLAG_DEVICE);

    UNREFERENCED_PARAMETER((ResourcesRaw, ResourcesTranslated));

    NTSTATUS  status;
    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    StateMachineEngine_EventAdd(&(nxDevice->m_SmEngineContext),
        NxDevicePnPPwrEventCxPostPrepareHardware);

    // This wait ensure NetAdapterCx won't return from PrepareHardware
    // before the state machine is in DeviceStarted or DeviceStartFailedWaitForReleaseHardware
    // state
    status = nxDevice->WaitForStateChangeResult();

    FuncExit(FLAG_DEVICE);

    return status;
}

VOID
NxDevice::_EvtCxDevicePrePrepareHardwareFailedCleanup(
    _In_ WDFDEVICE    Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    FuncEntry(FLAG_DEVICE);

    UNREFERENCED_PARAMETER((ResourcesRaw, ResourcesTranslated));
    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    StateMachineEngine_EventAdd(&nxDevice->m_SmEngineContext,
        NxDevicePnPPwrEventCxPrePrepareHardwareFailedCleanup);

    (VOID)nxDevice->WaitForStateChangeResult();

    FuncExit(FLAG_DEVICE);
}

NTSTATUS
NxDevice::_EvtCxDevicePreReleaseHardware(
    _In_ WDFDEVICE    Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Called by WDF before calling the client's EvtDeviceReleaseHardware.

    The state machine will manage posting the surprise remove or remove
    notification to NDIS.
--*/
{
    FuncEntry(FLAG_DEVICE);

    NTSTATUS  status;
    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    UNREFERENCED_PARAMETER(ResourcesTranslated);
    //
    // For now we expect a DefaultNxAdapter
    //
    NT_ASSERT(nxDevice->m_DefaultNxAdapter != NULL);

    nxDevice->m_StateChangeStatus = STATUS_SUCCESS;

    StateMachineEngine_EventAdd(&(nxDevice->m_SmEngineContext),
        NxDevicePnPPwrEventCxPreReleaseHardware);

    // This will ensure the client's release hardware won't be called
    // before we are in DeviceRemovingReleaseClient state where the 
    // adapter has been halted and surprise remove has been handled
    status = nxDevice->WaitForStateChangeResult();

    FuncExit(FLAG_DEVICE);

    // Even if the status is not a success code WDF will call the client's
    // EvtDeviceReleaseHardware
    return status;
}

NTSTATUS
NxDevice::_EvtCxDevicePostReleaseHardware(
    _In_ WDFDEVICE    Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Called by WDF after calling the client's EvtDeviceReleaseHardware.

--*/
{
    FuncEntry(FLAG_DEVICE);
    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    UNREFERENCED_PARAMETER(ResourcesTranslated);
    //
    // For now we expect a DefaultNxAdapter
    //
    NT_ASSERT(nxDevice->m_DefaultNxAdapter != NULL);

    nxDevice->m_StateChangeStatus = STATUS_SUCCESS;

    StateMachineEngine_EventAdd(&(nxDevice->m_SmEngineContext),
        NxDevicePnPPwrEventCxPostReleaseHardware);

    FuncExit(FLAG_DEVICE);

    return STATUS_SUCCESS;
}

VOID
NxDevice::AdapterHalting(
    VOID
    )
{
    KIRQL irql;

    //
    // Raise the IRQL to dispatch level so the device state machine doesnt
    // process removal on the current thread and deadlock.
    // 
    KeRaiseIrql(DISPATCH_LEVEL, &irql);
    StateMachineEngine_EventAdd(&m_SmEngineContext, 
                                NxDevicePnPPwrEventAdapterHaltComplete);
    KeLowerIrql(irql);
}

NTSTATUS
NxDevice::InitializeWdfDevicePowerReferences(
    _In_ WDFDEVICE              Device
)
/*
Routine Description:
    Called during D0Entry to manage the WDFDEVICE power references based on
    AOAC and SS support.

    When SS and AOAC structures are initialized in NDIS, they are both initialized 
    with stop flags so that SS/AoAC engines dont try to drop/acquire power refs 
    until the WDFDEVICE is ready. Now that we're ready this routine will clear 
    those SS/AoAC stop flags by calling into NdisWdfActionStartPowerManagement.
    
    This routine also ensures we have one (and only one) WDFDEVICE 
    power reference. This is done to account for :
    - Cases where both AOAC and SS are enabled 
    - One of them is enabled.
    - Neither of them are enabled.

Arguments: WDFDEVICE

Return:
    NTSTATUS - Failures are unexpected as we've already validated device is power
        policy owner earlier.
--*/
{
    NTSTATUS status;

    status = WdfDeviceStopIdleWithTag(Device, FALSE,
                            (PVOID)(ULONG_PTR)_EvtCxDevicePostD0Entry);

    if(!WIN_VERIFY(NT_SUCCESS(status))) {
        // Unexpected. We expect a pending status
        return status;
    }

    status = NdisWdfPnpPowerEventHandler(m_DefaultNxAdapter->m_NdisAdapterHandle,
                            NdisWdfActionStartPowerManagement, NdisWdfActionPowerNone);

    if(!WIN_VERIFY(NT_SUCCESS(status))) {
        LogError(GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfActionStartPowerManagement failed %!STATUS!", status);
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NxDevice::_EvtCxDevicePostD0Entry(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Called by WDF after a successful call to the client's 
    EvtDeviceD0Entry.

    In case of a Explicit Power up (other than Pnp Start), this routine
    send a PowerD0 message to NDIS.sys

--*/
{
    FuncEntry(FLAG_DEVICE);
    NTSTATUS status = STATUS_SUCCESS;
    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);
    PNxAdapter nxAdapter;

    nxAdapter = nxDevice->GetDefaultNxAdapter();

    if (nxDevice->m_PowerReferenceAcquired == FALSE) {
        nxDevice->m_PowerReferenceAcquired = TRUE;

        //
        // Set the power management capabilities.
        //
        status = nxAdapter->ValidatePowerCapabilities();
        if (!NT_SUCCESS(status)) {
            return status; // Error logged by validate function
        }

        if (nxAdapter->m_PowerCapabilities.ManageS0IdlePowerReferences == WdfTrue) {
            status = nxDevice->InitializeWdfDevicePowerReferences(Device);
            if(!WIN_VERIFY(NT_SUCCESS(status))) {
                // Unexpected.
                LogError(nxDevice->GetRecorderLog(), FLAG_DEVICE,
                    "InitializeWdfDevicePowerReferences failed %!STATUS!", status );
                status = STATUS_SUCCESS;
            }
        }
    }

    if (PreviousState != WdfPowerDeviceD3Final) {
        //
        // Inform NDIS we're returning from a low power state. 
        //
        nxAdapter = nxDevice->GetDefaultNxAdapter();

        ASSERT(nxDevice->m_LastReportedDxAction != NdisWdfActionPowerNone);
        status = NdisWdfPnpPowerEventHandler(nxDevice->m_DefaultNxAdapter->m_NdisAdapterHandle,
                                        NdisWdfActionPowerD0,
                                        nxDevice->m_LastReportedDxAction);
        if (!NT_SUCCESS(status)) {
            LogError(nxDevice->GetRecorderLog(), FLAG_DEVICE,
                "NdisWdfPnpPowerEventHandler failed %!STATUS!", status);
            FuncExit(FLAG_DEVICE);
            return status;
        }

        nxDevice->m_LastReportedDxAction = NdisWdfActionPowerNone;
    }

    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::_EvtCxDevicePreD0Exit(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    Called by WDF before calling the client's
    EvtDeviceD0Exit.

    In case of a Explicit Power Down (other than Pnp Stop/Remove), this routine
    send a PowerDX message to NDIS.sys

--*/
{
    FuncEntry(FLAG_DEVICE);

    NTSTATUS status = STATUS_SUCCESS;
#if DBG
    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);
#else
    UNREFERENCED_PARAMETER(Device);
#endif

    if (TargetState == WdfPowerDeviceD1 ||
        TargetState == WdfPowerDeviceD2 ||
        TargetState == WdfPowerDeviceD3) {

        //
        // SMIO suspend should have determined the right power action for Dx
        // and reported it to NDIS
        //
        ASSERT(nxDevice->m_LastReportedDxAction != NdisWdfActionPowerNone);

    } else if (TargetState == WdfPowerDeviceD3Final) {
        //
        // this is WdfPowerDeviceD3Final. This represents the final time that the device 
        // enters the D3 device power state. Typically, this enumerator means that the system
        // is being turned off, the device is about to be removed, or a resource rebalance is in 
        // progress. The device might have been already removed.
        //

        // there is no Dx irp involved. So no need to poke NDIS. Eventually, we'll get removal 
        // irp and NDIS will be notified of that.
    } else {
        //
        // this is WdfPowerDevicePrepareForHibernation. The device supports hibernation files, 
        // and the system is ready to hibernate by entering system state S4. The driver must
        // not turn off the device. For more information, see Supporting Special Files.
        // 

        ASSERT(TargetState == WdfPowerDevicePrepareForHibernation);
        NT_ASSERTMSG("NEED To determine how to handle WdfPowerDevicePrepareForHibernation", FALSE);
    }

    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::_EvtCxDevicePostSelfManagedIoInit(
    _In_  WDFDEVICE Device
    )
/*++

Routine Description:

    Called by WDF after a successful call to the client's 
    EvtDeviceSelfManagedIoInit.

    If the client's EvtDeviceSelfManagedIoInit fails this
    callback won't be called.

    This routine informs NDIS that it is ok to start the data path for the
    client.

--*/
{
    FuncEntry(FLAG_DEVICE);

    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    nxDevice->m_DefaultNxAdapter->InitializeSelfManagedIO();

    FuncExit(FLAG_DEVICE);

    return STATUS_SUCCESS;
}

NTSTATUS
NxDevice::_EvtCxDevicePostSelfManagedIoRestart(
    _In_  WDFDEVICE Device
    )
/*++

Routine Description:

    Called by WDF after a successful call to the client's
    EvtDeviceSelfManagedIoRestart.

    If the client's EvtDeviceSelfManagedIoRestart fails
    this callback won't be called.

    This routine informs the adapter that it is ok to start the data path
    for the client from WDF perspective.

--*/
{
    FuncEntry(FLAG_DEVICE);

    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    // 
    // Adapter will ensure it triggers restart of the data path
    //
    nxDevice->m_DefaultNxAdapter->RestartSelfManagedIO();

    FuncExit(FLAG_DEVICE);

    return STATUS_SUCCESS;
}

NTSTATUS
NxDevice::_EvtCxDevicePreSelfManagedIoSuspend(
    _In_  WDFDEVICE Device
    )
/*++

Routine Description:

    Called by WDF before calling the client's
    EvtDeviceSelfManagedIoSuspend.

    This routine informs NDIS that it must synchronously pause the clients
    datapath.
--*/
{
    FuncEntry(FLAG_DEVICE);
    NTSTATUS status;

    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    // 
    // Adapter will ensure it pauses the data path before the call returns.
    //
    nxDevice->m_DefaultNxAdapter->SuspendSelfManagedIO();

    //
    // If we're going through suspend because of a power down, then notify
    // NDIS now. We do this now as opposed to D0Exit so NDIS can send the 
    // appropriate PM parameter OIDs before the client driver is armed for wake.
    //
    status = nxDevice->NotifyNdisDevicePowerDown();
    
    FuncExit(FLAG_DEVICE);

    return status;
}

NTSTATUS
NxDevice::NotifyNdisDevicePowerDown(
    VOID
)
/*++
Routine Description:
    If the device is in the process of being powered down then it notifies 
    NDIS of the power transition along with the reason for the device power
    transition.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    POWER_ACTION systemPowerAction;

    if (m_Flags.InDxPowerTransition) {
        systemPowerAction = WdfDeviceGetSystemPowerAction(GetFxObject());

        if (m_SystemPowerAction != PowerActionNone
            &&
            (systemPowerAction == PowerActionSleep || systemPowerAction == PowerActionHibernate))
        {
            m_LastReportedDxAction = NdisWdfActionPowerDxOnSystemSx;
        }
        else if (m_SystemPowerAction != PowerActionNone &&
            (systemPowerAction == PowerActionShutdown || systemPowerAction == PowerActionShutdownReset ||
            systemPowerAction == PowerActionShutdownOff))
        {
            m_LastReportedDxAction = NdisWdfActionPowerDxOnSystemShutdown;
        }
        else 
        {
            m_LastReportedDxAction = NdisWdfActionPowerDx;
        }

        status = NdisWdfPnpPowerEventHandler(m_DefaultNxAdapter->m_NdisAdapterHandle,
                                            m_LastReportedDxAction, NdisWdfActionPowerNone);
        if (!NT_SUCCESS(status)) {
            LogError(GetRecorderLog(), FLAG_DEVICE,
                "NdisWdfPnpPowerEventHandler %d failed %!STATUS!", 
                m_LastReportedDxAction, status);
        }

        m_Flags.InDxPowerTransition = FALSE;
    }

    return status;
}

VOID
NxDevice::_EvtCxDevicePreSurpriseRemoval(
    _In_  WDFDEVICE Device
    )
/*++

Routine Description:

    Called by WDF before calling the client's
    EvtDeviceSurpriseRemoval.

    This routine keeps a note that this is a case of Surprise Remove.

--*/
{
    FuncEntry(FLAG_DEVICE);

    PNxDevice nxDevice = GetNxDeviceFromHandle(Device);

    nxDevice->m_SurpriseRemoved = TRUE;

    FuncExit(FLAG_DEVICE);
}

NTSTATUS
NxDevice::_WdmIrpCompleteFromNdis(
    PDEVICE_OBJECT DeviceObject,
    PIRP           Irp,
    PVOID          Context
    )
/*++
Routine Description: 
 
    This is a wdm irp completion routine for the IRPs that were forwarded to
    ndis.sys directly from a pre-processor routine.
 
    It release a removelock that was acquired in the preprocessor routine
 
--*/
{    
    PNxDevice nxDevice = (PNxDevice) Context;

    UNREFERENCED_PARAMETER(DeviceObject);

    IoReleaseRemoveLock(&nxDevice->m_RemoveLock, Irp);

    //
    // Propogate the Pending Flag
    //
    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS
NxDevice::_WdmIrpCompleteSetPower(
    PDEVICE_OBJECT DeviceObject,
    PIRP           Irp,
    PVOID          Context
    )
/*++
Routine Description:
    This completion routine is registered when the device 
    receives a power IRP requesting it to go to D0/Dx.

--*/
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION irpStack;
    DEVICE_POWER_STATE devicePowerState;
    PNxDevice nxDevice = static_cast<PNxDevice>(Context);

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(irpStack->Parameters.Power.Type == DevicePowerState);
    devicePowerState = irpStack->Parameters.Power.State.DeviceState;

    if (devicePowerState != PowerDeviceD0) {
        nxDevice->m_Flags.InDxPowerTransition = FALSE;
    }

    //
    // Propogate the Pending Flag
    //
    if (Irp->PendingReturned)
    {
        IoMarkIrpPending(Irp);
    }

    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS
NxDevice::WdmCreateIrpPreProcess(
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
    )
/*++
Routine Description:

    This is a pre-processor routine for IRP_MJ_CREATE.

    From this routine NetAdapterCx hands the IRP to NDIS in order
    to do some checks and initialize the handle context. Then it
    checks to see if the handle is marked as being on a private
    interface, if yes it hands the request to the client.
--*/
{
    FuncEntry(FLAG_DEVICE);

    NTSTATUS                       status;
    PIO_STACK_LOCATION             irpStack;
    WDFDEVICE                      device;
    PNxAdapter                     nxAdapter;
    BOOLEAN                        wdfOwnsIrp;

    device = GetFxObject();
    nxAdapter = m_DefaultNxAdapter;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    status = NdisWdfCreateIrpHandler(nxAdapter->m_NdisAdapterHandle, Irp, &wdfOwnsIrp);

    // If WDF doesn't own this IRP there is no work to do
    if (!wdfOwnsIrp)
    {
        return status;
    }

    // If NDIS create handler succeeded and marked the handle as being
    // on a side band device interface, forward the request to the client
    if (NT_SUCCESS(status) && NDIS_WDF_IS_PRIV_DEV_INTERFACE(irpStack))
    {
        IoSkipCurrentIrpStackLocation(Irp);
        status = WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);

        return status;
    }
    
    // In case NDIS create handler failed or the handle is not on a side
    // band device interface, complete the IRP
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::WdmCloseIrpPreProcess(
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
    )
/*++
Routine Description:

    This is a pre-processor routine for IRP_MJ_CLOSE

    From this routine NetAdapterCx hands the IRP to NDIS so it
    can perform cleanup associated with the handle being closed.

Notes:
    NdisWdfCloseIrpHandler will free the context associated with
    the handle. So we check if the handle is on a private interface
    before calling that function.
--*/
{
    FuncEntry(FLAG_DEVICE);

    NTSTATUS                       status;
    PIO_STACK_LOCATION             irpStack;
    WDFDEVICE                      device;
    PNxAdapter                     nxAdapter;
    BOOLEAN                        isPrivateIntf;
    BOOLEAN                        wdfOwnsIrp;

    device = GetFxObject();
    nxAdapter = m_DefaultNxAdapter;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    isPrivateIntf = NDIS_WDF_IS_PRIV_DEV_INTERFACE(irpStack);

    status = NdisWdfCloseIrpHandler(nxAdapter->m_NdisAdapterHandle, Irp, &wdfOwnsIrp);

    // If WDF doesn't own this IRP there is no work to do
    if (!wdfOwnsIrp)
    {
        return status;
    }

    // If the handle was opened in a side band device interface,
    // forward the request to the client
    if (isPrivateIntf)
    {
        IoSkipCurrentIrpStackLocation(Irp);
        status = WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);

        return status;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::WdmIoIrpPreProcess(
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
    )
{
    FuncEntry(FLAG_DEVICE);

    NTSTATUS                       status;
    PIO_STACK_LOCATION             irpStack;
    WDFDEVICE                      device;
    PNxAdapter                     nxAdapter;

    device = GetFxObject();
    nxAdapter = m_DefaultNxAdapter;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_ASSERT(
        irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL || 
        irpStack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL ||
        irpStack->MajorFunction == IRP_MJ_WRITE ||
        irpStack->MajorFunction == IRP_MJ_READ ||
        irpStack->MajorFunction == IRP_MJ_CLEANUP
        );

    // Check if this IO should go to the client
    if (NDIS_WDF_IS_PRIV_DEV_INTERFACE(irpStack))
    {
        IoSkipCurrentIrpStackLocation(Irp);
        status = WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);

        return status;
    }

    // If not, forward to NDIS
    switch (irpStack->MajorFunction)
    {
    case IRP_MJ_DEVICE_CONTROL:
        status = NdisWdfDeviceControlIrpHandler(nxAdapter->m_NdisAdapterHandle, Irp);
        break;
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
        status = NdisWdfDeviceInternalControlIrpHandler(nxAdapter->m_NdisAdapterHandle, Irp);
        break;
    case IRP_MJ_CLEANUP:
        // NDIS always completes IRP_MJ_CLEANUP with STATUS_SUCCESS
        status = STATUS_SUCCESS;

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        break;
    default:
        // NDIS does not support IRP_MJ_READ and IRP_MJ_WRITE
        status = STATUS_NOT_SUPPORTED;

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        break;
    }
       
    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::WdmSystemControlIrpPreProcess(
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
    )
{
    FuncEntry(FLAG_DEVICE);

    NTSTATUS                       status;
    WDFDEVICE                      device;
    PNxAdapter                     nxAdapter;

    UNREFERENCED_PARAMETER(DispatchContext);

    device = GetFxObject();
    nxAdapter = m_DefaultNxAdapter;

    //
    // Acquire a wdm remove lock, the lock is released in the 
    // completion routine: NxDevice::_WdmIrpCompleteFromNdis
    //
    status = IoAcquireRemoveLock(&m_RemoveLock, Irp);

    if (!NT_SUCCESS(status)) {

        NT_ASSERT(status == STATUS_DELETE_PENDING);
        LogInfo(GetRecorderLog(), FLAG_DEVICE,
            "IoAcquireRemoveLock failed, irp 0x%p, %!STATUS!", Irp, status);

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    IoCopyCurrentIrpStackLocationToNext(Irp);

#pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    SetCompletionRoutineSmart(WdfDeviceWdmGetDeviceObject(device),
        Irp,
        NxDevice::_WdmIrpCompleteFromNdis,
        (PVOID)this,
        TRUE,
        TRUE,
        TRUE);

    //
    // We need to explicitly adjust the IrpStackLocation since we do not call 
    // IoCallDriver to send this Irp to NDIS.sys. Rather we hand this irp to 
    // Ndis.sys in function call.
    //
    IoSetNextIrpStackLocation(Irp);

    status = NdisWdfDeviceWmiHandler(nxAdapter->m_NdisAdapterHandle, Irp);
    
    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::_EvtWdfCxDeviceWdmIrpPreProcess(
    _In_    WDFDEVICE  Device,
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
    )
/*++
Routine Description: 
 
    This is a pre-processor routine for several IRPs.
 
    From this routine NetAdapterCx forwards the following irps directly to NDIS.sys
        CREATE, CLOSE, DEVICE_CONTROL, INTERNAL_DEVICE_CONTROL, SYSTEM_CONTRL
 
--*/
{
    FuncEntry(FLAG_DEVICE);
    PIO_STACK_LOCATION             irpStack;
    NTSTATUS                       status = STATUS_SUCCESS;
    PDEVICE_OBJECT                 deviceObj;
    PNxDevice                      nxDevice;
    PNxAdapter                     nxAdapter;

    deviceObj = WdfDeviceWdmGetDeviceObject(Device);
    nxDevice = GetNxDeviceFromHandle(Device);
    nxAdapter = nxDevice->m_DefaultNxAdapter;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch(irpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ_CREATE Device 0x%p, IRP 0x%p\n", deviceObj, Irp);

        status = nxDevice->WdmCreateIrpPreProcess(Irp, DispatchContext);
        break;
    case IRP_MJ_CLOSE:
        LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ_CLOSE Device 0x%p, IRP 0x%p\n", deviceObj, Irp);

        status = nxDevice->WdmCloseIrpPreProcess(Irp, DispatchContext);
        break;
    case IRP_MJ_DEVICE_CONTROL:
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
    case IRP_MJ_WRITE:
    case IRP_MJ_READ:
    case IRP_MJ_CLEANUP:
        LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ code 0x%x, Device 0x%p, IRP 0x%p\n", irpStack->MajorFunction, deviceObj, Irp);

        status = nxDevice->WdmIoIrpPreProcess(Irp, DispatchContext);
        break;
    case IRP_MJ_SYSTEM_CONTROL:
        LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ_SYSTEM_CONTROL Device 0x%p, IRP 0x%p\n", deviceObj, Irp);

        status = nxDevice->WdmSystemControlIrpPreProcess(Irp, DispatchContext);
        break;
    default:
        NT_ASSERTMSG("Unexpected Irp", FALSE);        
        status = STATUS_INVALID_PARAMETER;

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        break;
    }

    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::_EvtWdfCxDeviceWdmPnpPowerIrpPreProcess(
    _In_    WDFDEVICE  Device,
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
    )
/*++
Routine Description: 
 
    This is a pre-processor routine for PNP and Power IRPs.
 
    In case this is a IRP_MN_REMOVE_DEVICE irp, we want to call
    IoReleaseRemoveLockAndWait to ensure that all existing remove locks have
    been release and no new remove locks can be acquired. 

    In case this is an IRP_MN_SET_POWER we inspect the IRP to mark
    the adapter associated with this device as going in or out of a
    power transition
--*/
{
    FuncEntry(FLAG_DEVICE);

    PIO_STACK_LOCATION             irpStack;
    PNxDevice                      nxDevice;
    PDEVICE_OBJECT                 deviceObject;
    BOOLEAN                        skipCurrentStackLocation = TRUE;
    NTSTATUS                       status;
    SYSTEM_POWER_STATE_CONTEXT     systemPowerStateContext;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_ASSERT(irpStack->MajorFunction == IRP_MJ_PNP ||
              irpStack->MajorFunction == IRP_MJ_POWER);

    nxDevice = GetNxDeviceFromHandle(Device);

    switch (irpStack->MajorFunction)
    {
        case IRP_MJ_PNP:
            if (irpStack->MinorFunction == IRP_MN_REMOVE_DEVICE) {
                //
                // Acquiring a RemoveLock is required before calling 
                // IoReleaseRemoveLockAndWait
                //
                status = IoAcquireRemoveLock(&nxDevice->m_RemoveLock, Irp);

                NT_ASSERT(NT_SUCCESS(status));

                IoReleaseRemoveLockAndWait(&nxDevice->m_RemoveLock, Irp);
            }
            break;
        case IRP_MJ_POWER:
            if (irpStack->MinorFunction == IRP_MN_SET_POWER) {
                if (irpStack->Parameters.Power.Type == SystemPowerState) {
                    systemPowerStateContext = irpStack->Parameters.Power.SystemPowerStateContext;

                    if (systemPowerStateContext.TargetSystemState != PowerSystemWorking) {
                        nxDevice->m_SystemPowerAction = irpStack->Parameters.Power.ShutdownType;
                    }
                    else {
                        nxDevice->m_SystemPowerAction = PowerActionNone;
                    }

                    break;
                }

                if (irpStack->Parameters.Power.Type == DevicePowerState) {
                    if (irpStack->Parameters.Power.State.DeviceState == PowerDeviceD3 ||
                        irpStack->Parameters.Power.State.DeviceState == PowerDeviceD2 ||
                        irpStack->Parameters.Power.State.DeviceState == PowerDeviceD1) {
                        nxDevice->m_Flags.InDxPowerTransition = TRUE;
                    }

                    deviceObject = WdfDeviceWdmGetDeviceObject(Device);

                    IoCopyCurrentIrpStackLocationToNext(Irp);

                    //
                    // Set completion routine to clear the InDxPowerTransition flag
                    //
                    SetCompletionRoutineSmart(
                        deviceObject, 
                        Irp, 
                        NxDevice::_WdmIrpCompleteSetPower, 
                        nxDevice, 
                        TRUE, 
                        TRUE, 
                        TRUE);

                    skipCurrentStackLocation = FALSE;
                }
            }
            break;
        default:
            NT_ASSERTMSG("Should never hit", FALSE);
            break;
    }

    //
    // Allow WDF to process the Pnp Irp normally.
    //
    if (skipCurrentStackLocation) {
        IoSkipCurrentIrpStackLocation(Irp);
    }

    status = WdfDeviceWdmDispatchIrp(Device, Irp, DispatchContext);

    FuncExit(FLAG_DEVICE);
    return status;
}

VOID
NxDevice::_EvtCleanup(
    _In_  WDFOBJECT Device
    )
/*++
Routine Description: 
    A WDF Event Callback
 
    This routine is called when the client driver WDFDEVICE object is being
    deleted. This happens when WDF is processing the REMOVE irp for the client.
 
--*/
{
    FuncEntry(FLAG_DEVICE);

    PNxDevice nxDevice = GetNxDeviceFromHandle((WDFDEVICE)Device);
    PNxAdapter nxAdapter = nxDevice->m_DefaultNxAdapter;

    StateMachineEngine_EventAdd(&nxDevice->m_SmEngineContext, 
                                NxDevicePnPPwrEventWdfDeviceObjectCleanup);

    if (nxAdapter != NULL) {
        WdfObjectDereferenceWithTag(nxAdapter->GetFxObject(), (PVOID)NxDevice::_EvtCleanup);
    }

    FuncExit(FLAG_DEVICE);
    return;
}

NTSTATUS
NxDevice::WaitForStateChangeResult(
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

VOID
NxDevicePnPPwrEventHandler_Ignore(
    PVOID MiniportAsPVOID
    )
{
    UNREFERENCED_PARAMETER(MiniportAsPVOID);
}

VOID
NxDevicePnPPwrEventHandler_PrePrepareHardware(
    _In_ PVOID ContextAsPVOID
    )
{
    // This auto event handler is used to unblock PrePrepareHardware
    // during NxDevice state machine first start
    PNxDevice nxDevice = (PNxDevice)ContextAsPVOID;
    nxDevice->SignalStateChange(STATUS_SUCCESS);
}

NTSTATUS
EvtWdmIrpPreprocessRoutine(
    WDFDEVICE  Device,
    PIRP       Irp,
    WDFCONTEXT DispatchContext
    )
/*++
Routine Description:

    A WDF event callback.

    The EvtWdmIrpPreprocessRoutine is just a wrapper since
    we cannot specify a function class for a static class member

    Please see description of _EvtWdfDeviceWdmIrpPreProcess

--*/
{
    NTSTATUS status;
    FuncEntry(FLAG_DEVICE);
    status = NxDevice::_EvtWdfCxDeviceWdmIrpPreProcess(Device, Irp, DispatchContext);
    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
EvtWdmPnpPowerIrpPreprocessRoutine(
    WDFDEVICE  Device,
    PIRP       Irp,
    WDFCONTEXT DispatchContext
    )
/*++
Routine Description:

    A WDF event callback.

    The EvtWdmPnpPowerIrpPreprocessRoutine is just a wrapper since
    we cannot specify a function class for a static class member

    Please see description of _EvtWdfCxDeviceWdmPnpPowerIrpPreProcess

--*/
{
    NTSTATUS status;
    FuncEntry(FLAG_DEVICE);
    status = NxDevice::_EvtWdfCxDeviceWdmPnpPowerIrpPreProcess(Device, Irp, DispatchContext);
    FuncExit(FLAG_DEVICE);
    return status;
}

VOID
NxDeviceStateTransitionTracing(
    _In_        SMOWNER         Owner,
    _In_        PVOID           StateMachineContext,
    _In_        SM_ENGINE_STATE FromState,
    _In_        SM_ENGINE_EVENT Event,
    _In_        SM_ENGINE_STATE ToState
    )
{
    // SMOWNER is WDFDEVICE
    UNREFERENCED_PARAMETER(Owner);

    // StateMachineContext is NxDevice object;
    NxDevice* nxDevice = reinterpret_cast<NxDevice*>(StateMachineContext);

    TraceLoggingWrite(
        g_hNetAdapterCxEtwProvider,
        "NxDeviceStateTransition",
        TraceLoggingDescription("NxDevicePnPStateTransition"),
        TraceLoggingKeyword(NET_ADAPTER_CX_NXDEVICE_PNP_STATE_TRANSITION),
        TraceLoggingUInt32(nxDevice->GetDeviceBusAddress(), "DeviceBusAddress"),
        TraceLoggingHexInt32(FromState, "DeviceStateTransitionFrom"),
        TraceLoggingHexInt32(Event, "DeviceStateTransitionEvent"),
        TraceLoggingHexInt32(ToState, "DeviceStateTransitionTo")
    );
}
