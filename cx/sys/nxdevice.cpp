// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include "NxDevice.tmh"

//
// State machine entry functions aka. "State machine Operations"
//
_Use_decl_annotations_
NxDevice::Event
NxDevice::StartingReportStartToNdis()
{
    //
    // Send this message to each adapter created during EvtDriverDeviceAdd. The state machine
    // should tell NDIS and invoke the adapter's MiniportInitialize in response
    //
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        adapter.PnpStart();

        //
        // For adapters created in EvtDriverDeviceAdd that did not call NetAdapterStart, do it here on behalf
        // of them. This is so that our current drivers that support only 1:1 adapter mapping can keep working
        // without modifications. When we eventually remove EvtAdapterSetCapabilities we can remove this.
        //
        if (adapter.m_Flags.StopReasonApi)
            adapter.ApiStart();

        //
        // We need to hold Pnp start until every adapter has finished initializing
        //
        NTSTATUS status = adapter.WaitForMiniportStart();

        //
        // If NDIS failed start, log the error but there is not much we can do with the status here
        //
        if (!NT_SUCCESS(status))
        {
            LogError(GetRecorderLog(), FLAG_DEVICE,
                "Ndis initialize failed for adapter. adapter=%p",
                &adapter);
        }

        return STATUS_SUCCESS;
    });

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
VOID
NxDevice::PrepareForStart()
{
    // This will let PrePrepareHardware callback continue execution
    m_StateChangeComplete.Set();
}

_Use_decl_annotations_
VOID
NxDevice::StartProcessingComplete()
{
    m_Flags.DeviceStarted = TRUE;
    m_StateChangeComplete.Set();
}

_Use_decl_annotations_
VOID
NxDevice::StartProcessingCompleteFailed()
{
    m_StateChangeComplete.Set();
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
    m_NdisInitializeCount++;

    if (!NT_SUCCESS(InitializationStatus))
    {
        m_Flags.AnyAdapterFailedNdisInitialize = TRUE;
    }

    // If an adapter was initialized as part of the device's IRP_MN_START_DEVICE and
    // we finished initializing all the adapters created during EvtDriverDeviceAdd we
    // move the NxDevice state machine forward
    if (!m_Flags.DeviceStarted && m_NdisInitializeCount == m_AdapterCollection.Count())
    {
        // If any adapter created in EvtDriverDeviceAdd failed
        // NdisMiniportInitializeEx fail device initialization.
        // In the future we might want to make this configurable
        if (m_Flags.AnyAdapterFailedNdisInitialize)
        {
            m_StateChangeStatus = STATUS_INVALID_DEVICE_STATE;
            EnqueueEvent(NxDevice::Event::AllNdisInitializeCompleteFailure);
        }
        else
        {
            EnqueueEvent(NxDevice::Event::AllNdisInitializeCompleteSuccess);
        }
    }
}

NTSTATUS
NxDevice::AdapterAdd(
    _In_ NxAdapter *Adapter
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
                            &Adapter->m_NdisAdapterHandle,
                            reinterpret_cast<NDIS_HANDLE>(Adapter));
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

    m_AdapterCollection.AddAdapter(Adapter);

    return status;
}

_Use_decl_annotations_
bool
NxDevice::RemoveAdapter(
    NxAdapter *Adapter
    )
{
    return m_AdapterCollection.RemoveAdapter(Adapter);
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingIsSurpriseRemoved()
{
    m_Flags.DeviceStarted = FALSE;

    if (m_SurpriseRemoved) {
        return NxDevice::Event::Yes;
    } else {
        return NxDevice::Event::No;
    }
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingReportSurpriseRemoveToNdis()
{
    NTSTATUS status = m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter const &adapter)
    {
        LogVerbose(GetRecorderLog(), FLAG_DEVICE, "Calling NdisWdfPnpPowerEventHandler(SurpriseRemove)");

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                NdisWdfActionPnpSurpriseRemove, NdisWdfActionPowerNone),
            "NdisWdfActionPnpSurpriseRemove failed. adapter=%p, device=%p", &adapter, this);

        return STATUS_SUCCESS;
    });

    NT_ASSERT(NT_SUCCESS(status));

    if (!NT_SUCCESS(status)) {
        m_StateChangeStatus = status;

        LogError(GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpPowerEventHandler NdisWdfActionPnpSurpriseRemove failed NxDevice 0x%p, 0x%!STATUS!",
            this, status);
        return NxDevice::Event::SyncFail;
    }

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
VOID
NxDevice::ReleasingReportDeviceAddFailureToNdis()
{
    //
    // If EvtDriverDeviceAdd fails after a NETADAPTER object is created the WDFDEVICE won't receive
    // any PrepareHardware/ReleaseHardware events, and the device state machine will receive a
    // WdfDeviceObjectCleanup event directly.
    //
    // By calling NdisWdfPnpPowerEventHandler with NdisWdfActionPreReleaseHardware and
    // NdisWdfActionPostReleaseHardware we ensure the miniport will have its resources cleaned up
    // properly
    //

    NTSTATUS status = m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter const &adapter) 
    {
        LogVerbose(GetRecorderLog(), FLAG_DEVICE, "Calling NdisWdfPnpPowerEventHandler(PreReleaseHardware)");

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                NdisWdfActionPreReleaseHardware, NdisWdfActionPowerNone),
            "NdisWdfActionPreReleaseHardware failed. adapter=%p, device=%p", &adapter, this);

        LogVerbose(GetRecorderLog(), FLAG_DEVICE, "Calling NdisWdfPnpPowerEventHandler(PostReleaseHardware)");

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                NdisWdfActionPostReleaseHardware, NdisWdfActionPowerNone),
            "NdisWdfActionPostReleaseHardware failed. adapter=%p, device%p", &adapter, this);

        return STATUS_SUCCESS;
    });

    NT_ASSERT(status == STATUS_SUCCESS);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpPowerEventHandler failed NxDevice 0x%p, 0x%!STATUS!, Ignoring it",
            this, status);
    }
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingReportPreReleaseToNdis()
{
    NTSTATUS status = m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter const &adapter)
    {
        LogVerbose(GetRecorderLog(), FLAG_DEVICE, "Calling NdisWdfPnpPowerEventHandler(PreRelease)");

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                NdisWdfActionPreReleaseHardware, NdisWdfActionPowerNone),
            "NdisWdfActionPreReleaseHardware failed. adapter=%p, device=%p", &adapter, this);

        return STATUS_SUCCESS;
    });

    NT_ASSERT(NT_SUCCESS(status));

    if (!NT_SUCCESS(status)) {
        m_StateChangeStatus = status;

        LogError(GetRecorderLog(), FLAG_DEVICE,
            "EvtDeviceReleaseHardware NdisWdfActionPreReleaseHardware failed NxDevice 0x%p, 0x%!STATUS!\n",
            this, status);
        return NxDevice::Event::SyncFail;
    }

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
VOID
NxDevice::ReleasingReleaseClient()
{
    // Let the client's EvtDeviceReleaseHardware be called
    m_StateChangeComplete.Set();
}

_Use_decl_annotations_
VOID
NxDevice::ReleasingReportPostReleaseToNdis()
{
    NTSTATUS status = m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter const &adapter)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                NdisWdfActionPostReleaseHardware, NdisWdfActionPowerNone),
            "NdisWdfActionPostReleaseHardware failed. adapter=%p, device=%p", &adapter, this);

        return STATUS_SUCCESS;
    });

    NT_ASSERT(status == STATUS_SUCCESS);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpPowerEventHandler NdisWdfActionPostReleaseHardware failed NxDevice 0x%p, 0x%!STATUS!, Ignoring it",
            this, status);
    }
}

_Use_decl_annotations_
VOID
NxDevice::RemovedReportRemoveToNdis()
{
    Verifier_VerifyDeviceAdapterCollectionIsEmpty(
        m_NxDriver->GetPrivateGlobals(),
        this,
        &m_AdapterCollection);
}

_Use_decl_annotations_
VOID
NxDevice::StoppedPrepareForStart()
/*
Description:

    This state entry function handles a PnP start after a PnP stop due to a
    resource rebalance
*/
{
    NTSTATUS status = m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter const &adapter)
    {
        // Call into NDIS to put the miniport in stopped state
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
            NdisWdfActionPnpRebalance, NdisWdfActionPowerNone),
            "Failed NdisWdfActionPnpRebalance. adapter=%p, device=%p", &adapter, this);

        return STATUS_SUCCESS;
    });

    NT_ASSERT(status == STATUS_SUCCESS);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpPowerEventHandler NdisWdfActionPnpRebalance failed NxDevice 0x%p, 0x%!STATUS!, Ignoring it",
            this, status);
    }

    // Unblock PrePrepareHardware
    SignalStateChange(STATUS_SUCCESS);
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

    status = nxDevice->Init();

    if (!NT_SUCCESS(status)) {
        LogError(PrivateGlobals->NxDriver->GetRecorderLog(), FLAG_DEVICE,
            "Failed to initialize NxDevice, nxDevice=%p, status=%!STATUS!", nxDevice, status);
        FuncExit(FLAG_DEVICE);
        return status;
    }

    *NxDeviceParam = nxDevice;

    FuncExit(FLAG_DEVICE);
    return status;
}

NTSTATUS
NxDevice::Init(
    void
    )
{
    #ifdef _KERNEL_MODE
    StateMachineEngineConfig smConfig(WdfDeviceWdmGetDeviceObject(GetFxObject()), NETADAPTERCX_TAG);
    #else
    StateMachineEngineConfig smConfig;
    #endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        Initialize(smConfig),
        "StateMachineEngine_Init failed");

    CX_RETURN_IF_NOT_NT_SUCCESS(m_AdapterCollection.Initialize());

    return STATUS_SUCCESS;
}

NxDriver *
NxDevice::GetNxDriver(
    void
    ) const
{
    return m_NxDriver;
}

NxAdapter *
NxDevice::GetDefaultNxAdapter(
    VOID
    ) const
{
    return m_AdapterCollection.GetDefaultAdapter();
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

    Mx::MxInitializeRemoveLock(&m_RemoveLock, NETADAPTERCX_TAG, 0, 0);

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






    NT_ASSERT(nxDevice != nullptr);

    if (nxDevice == nullptr)
    {
        FuncExit(FLAG_DEVICE);
        return STATUS_INVALID_DEVICE_STATE;
    }

    nxDevice->EnqueueEvent(NxDevice::Event::CxPrePrepareHardware);

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

    nxDevice->EnqueueEvent(NxDevice::Event::CxPostPrepareHardware);

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

    nxDevice->EnqueueEvent(NxDevice::Event::CxPrePrepareHardwareFailedCleanup);

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

    nxDevice->m_StateChangeStatus = STATUS_SUCCESS;

    nxDevice->EnqueueEvent(NxDevice::Event::CxPreReleaseHardware);

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

    nxDevice->m_StateChangeStatus = STATUS_SUCCESS;

    nxDevice->EnqueueEvent(NxDevice::Event::CxPostReleaseHardware);

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
    // Wait until all the adapters started halting to let the device
    // state machine (and EvtDeviceReleaseHardware) go.
    //

    m_NdisInitializeCount--;

    if (m_NdisInitializeCount == 0)
    {
        //
        // Raise the IRQL to dispatch level so the device state machine doesnt
        // process removal on the current thread and deadlock.
        //
        KeRaiseIrql(DISPATCH_LEVEL, &irql);
        EnqueueEvent(NxDevice::Event::AllAdapterHaltComplete);
        KeLowerIrql(irql);
    }
}

bool
NxDevice::IsStarted(
    void
    ) const
{
    return !!m_Flags.DeviceStarted;
}

bool
NxDevice::IsSelfManagedIoStarted(
    void
    ) const
{
    return !!m_Flags.SelfManagedIoStarted;
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

    status = m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter const &adapter)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                NdisWdfActionStartPowerManagement, NdisWdfActionPowerNone),
            "NdisWdfActionStartPowerManagement failed. adapter=%p, device=%p", &adapter, this);

        return STATUS_SUCCESS;
    });

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
    PNxAdapter defaultNxAdapter = nxDevice->GetDefaultNxAdapter();

    if (nxDevice->m_PowerReferenceAcquired == FALSE) {
        nxDevice->m_PowerReferenceAcquired = TRUE;

        // Each adapter has it's own set of power capabilities, this is a problem because
        // ManageS0IdlePowerReferences is actully a property of the NxDevice. For now use
        // the capability of the default adapter.

        //
        // Set the power management capabilities.
        //
        status = defaultNxAdapter->ValidatePowerCapabilities();
        if (!NT_SUCCESS(status)) {
            return status; // Error logged by validate function
        }

        if (defaultNxAdapter->m_PowerCapabilities.ManageS0IdlePowerReferences == WdfTrue) {
            status = nxDevice->InitializeWdfDevicePowerReferences(Device);
            if(!WIN_VERIFY(NT_SUCCESS(status))) {
                // Unexpected.
                LogError(nxDevice->GetRecorderLog(), FLAG_DEVICE,
                    "InitializeWdfDevicePowerReferences failed %!STATUS!", status );
                status = STATUS_SUCCESS;
            }
        }
    }

    status = nxDevice->EvtPostD0Entry(PreviousState);

    FuncExit(FLAG_DEVICE);
    return status;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::EvtPostD0Entry(
    WDF_POWER_DEVICE_STATE PreviousState
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (PreviousState != WdfPowerDeviceD3Final)
    {
        //
        // Inform NDIS we're returning from a low power state.
        //

        NT_ASSERT(m_LastReportedDxAction != NdisWdfActionPowerNone);

        ntStatus = m_AdapterCollection.ForEachAdapterLocked(
            [this](NxAdapter const &adapter)
        {
            CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
                NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                    NdisWdfActionPowerD0,
                    m_LastReportedDxAction),
                "NdisWdfActionPowerD0 failed. adapter=%p, device=%p", &adapter, this);

            return STATUS_SUCCESS;
        });

        if (!NT_SUCCESS(ntStatus)) {
            LogError(GetRecorderLog(), FLAG_DEVICE,
                "NdisWdfPnpPowerEventHandler failed %!STATUS!", ntStatus);
            FuncExit(FLAG_DEVICE);
            return ntStatus;
        }

        m_LastReportedDxAction = NdisWdfActionPowerNone;
    }

    return ntStatus;
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

    nxDevice->InitializeSelfManagedIo();

    FuncExit(FLAG_DEVICE);
    return STATUS_SUCCESS;
}

void
NxDevice::InitializeSelfManagedIo(
    void
    )
{
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter &adapter)
    {
        adapter.InitializeSelfManagedIO();
        return STATUS_SUCCESS;
    });

    m_Flags.SelfManagedIoStarted = TRUE;
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

    nxDevice->RestartSelfManagedIo();

    FuncExit(FLAG_DEVICE);

    return STATUS_SUCCESS;
}

void
NxDevice::RestartSelfManagedIo(
    void
    )
{
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter &adapter)
    {
        //
        // Adapter will ensure it triggers restart of the data path
        //
        adapter.RestartSelfManagedIO();
        return STATUS_SUCCESS;
    });

    m_Flags.SelfManagedIoStarted = TRUE;
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

    nxDevice->SuspendSelfManagedIo();

    //
    // If we're going through suspend because of a power down, then notify
    // NDIS now. We do this now as opposed to D0Exit so NDIS can send the
    // appropriate PM parameter OIDs before the client driver is armed for wake.
    //
    status = nxDevice->NotifyNdisDevicePowerDown();

    FuncExit(FLAG_DEVICE);

    return status;
}

void
NxDevice::SuspendSelfManagedIo(
    void
    )
{
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter &adapter)
    {
        //
        // Adapter will ensure it pauses the data path before the call returns.
        //
        adapter.SuspendSelfManagedIO();
        return STATUS_SUCCESS;
    });

    m_Flags.SelfManagedIoStarted = FALSE;
}

void
NxDevice::StartComplete(
    void
    )
{
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter &adapter)
    {
        NdisWdfMiniportStarted(adapter.GetNdisHandle());
        return STATUS_SUCCESS;
    });
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

        status = m_AdapterCollection.ForEachAdapterLocked(
            [this](NxAdapter const &adapter)
        {
            CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
                NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                    m_LastReportedDxAction, NdisWdfActionPowerNone),
                "NdisWdfPnpPowerEventHandler failed. adapter=%p, device=%p", &adapter, this);
            
            return STATUS_SUCCESS;
        });

        if (!NT_SUCCESS(status)) {
            LogError(GetRecorderLog(), FLAG_DEVICE,
                "NdisWdfPnpPowerEventHandler %d failed %!STATUS!",
                m_LastReportedDxAction, status);
        }

        m_Flags.InDxPowerTransition = FALSE;
    }

    return status;
}

BOOLEAN
NxDevice::IsDeviceInPowerTransition(
    VOID
    )
{
    return (m_LastReportedDxAction != NdisWdfActionPowerNone);
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

    Mx::MxReleaseRemoveLock(&nxDevice->m_RemoveLock, Irp);

    //
    // Propagate the Pending Flag
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
    // Propagate the Pending Flag
    //
    if (Irp->PendingReturned)
    {
        IoMarkIrpPending(Irp);
    }

    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS
NxDevice::_WdmIrpCompleteCreate(
    DEVICE_OBJECT *DeviceObject,
    IRP *Irp,
    VOID *Context
    )
/*++
Routine Description:
    This completion routine is registered when we decide to dispatch
    an IRP_MJ_CREATE to the client driver.

    This is only invoked if the IRP is completed with an error or canceled.

--*/
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NxDevice *nxDevice = static_cast<NxDevice *>(Context);
    NxAdapter *nxAdapter = nxDevice->GetDefaultNxAdapter();

    IO_STACK_LOCATION *irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_ASSERT(
        irpStack->FileObject != nullptr &&
        irpStack->FileObject->FsContext != nullptr);

    // Call NDIS to cleanup the resources associated with this
    // handle. We don't expect this to fail
    NdisWdfCleanupUserOpenContext(
        nxAdapter->m_NdisAdapterHandle,
        irpStack->FileObject->FsContext);

    // NdisWdfCleanupUserOpenContext frees the memory pointed by
    // FileObject->FsContext, it should not be touched from here on
    irpStack->FileObject->FsContext = nullptr;

    //
    // Propagate the Pending Flag
    //
    if (Irp->PendingReturned)
        IoMarkIrpPending(Irp);

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
    nxAdapter = GetDefaultNxAdapter();
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
        // If the WDF client driver decides to fail the IRP we need to know
        // and tell NDIS about it
        IoCopyCurrentIrpStackLocationToNext(Irp);

        SetCompletionRoutineSmart(
            WdfDeviceWdmGetDeviceObject(GetFxObject()),
            Irp,
            NxDevice::_WdmIrpCompleteCreate,
            this,
            FALSE, // InvokeOnSuccess
            TRUE,  // InvokeOnError
            TRUE); // InvokeOnCancel

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
    nxAdapter = GetDefaultNxAdapter();
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
    nxAdapter = GetDefaultNxAdapter();
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
    nxAdapter = GetDefaultNxAdapter();

    //
    // Acquire a wdm remove lock, the lock is released in the
    // completion routine: NxDevice::_WdmIrpCompleteFromNdis
    //
    status = Mx::MxAcquireRemoveLock(&m_RemoveLock, Irp);

    if (!NT_SUCCESS(status)) {

        NT_ASSERT(status == STATUS_DELETE_PENDING);
        LogInfo(GetRecorderLog(), FLAG_DEVICE,
            "Mx::MxAcquireRemoveLock failed, irp 0x%p, %!STATUS!", Irp, status);

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
    nxAdapter = nxDevice->GetDefaultNxAdapter();

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
    Mx::MxReleaseRemoveLockAndWait to ensure that all existing remove locks have
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
                // Mx::MxReleaseRemoveLockAndWait
                //
                status = Mx::MxAcquireRemoveLock(&nxDevice->m_RemoveLock, Irp);

                NT_ASSERT(NT_SUCCESS(status));

                Mx::MxReleaseRemoveLockAndWait(&nxDevice->m_RemoveLock, Irp);
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

    nxDevice->EnqueueEvent(NxDevice::Event::WdfDeviceObjectCleanup);

    FuncExit(FLAG_DEVICE);
    return;
}

RECORDER_LOG
NxDevice::GetRecorderLog(
    VOID
    )
{
    return m_NxDriver->GetRecorderLog();
}

VOID
NxDevice::SignalStateChange(
    _In_ NTSTATUS Status
    )
{
    m_StateChangeStatus = Status;
    m_StateChangeComplete.Set();
}

NTSTATUS
NxDevice::WaitForStateChangeResult(
    VOID
    )
{
    m_StateChangeComplete.Wait();

    return m_StateChangeStatus;
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

_Use_decl_annotations_
VOID
NxDevice::EvtLogTransition(
    _In_ SmFx::TransitionType TransitionType,
    _In_ StateId SourceState,
    _In_ EventId ProcessedEvent,
    _In_ StateId TargetState
    )
{
    FuncEntry(FLAG_DEVICE);

    UNREFERENCED_PARAMETER(TransitionType);

    TraceLoggingWrite(
        g_hNetAdapterCxEtwProvider,
        "NxDeviceStateTransition",
        TraceLoggingDescription("NxDevicePnPStateTransition"),
        TraceLoggingKeyword(NET_ADAPTER_CX_NXDEVICE_PNP_STATE_TRANSITION),
        TraceLoggingUInt32(GetDeviceBusAddress(), "DeviceBusAddress"),
        TraceLoggingHexInt32(static_cast<INT32>(SourceState), "DeviceStateTransitionFrom"),
        TraceLoggingHexInt32(static_cast<INT32>(ProcessedEvent), "DeviceStateTransitionEvent"),
        TraceLoggingHexInt32(static_cast<INT32>(TargetState), "DeviceStateTransitionTo")
        );

    FuncExit(FLAG_DEVICE);
}

_Use_decl_annotations_
VOID
NxDevice::EvtLogMachineException(
    _In_ SmFx::MachineException exception,
    _In_ EventId relevantEvent,
    _In_ StateId relevantState
    )
{
    FuncEntry(FLAG_DEVICE);
    UNREFERENCED_PARAMETER((exception, relevantEvent, relevantState));


    if (!KdRefreshDebuggerNotPresent())
    {
        DbgBreakPoint();
    }

    FuncExit(FLAG_DEVICE);
}

_Use_decl_annotations_
VOID
NxDevice::EvtMachineDestroyed()
{
    FuncEntry(FLAG_DEVICE);
    FuncExit(FLAG_DEVICE);
}

_Use_decl_annotations_
VOID
NxDevice::EvtLogEventEnqueue(
    _In_ EventId relevantEvent
    )
{
    FuncEntry(FLAG_DEVICE);
    UNREFERENCED_PARAMETER(relevantEvent);
    FuncExit(FLAG_DEVICE);
}
