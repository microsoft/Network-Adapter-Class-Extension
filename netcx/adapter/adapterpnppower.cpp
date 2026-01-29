// Copyright (C) Microsoft Corporation. All rights reserved.
#include "Nx.hpp"
#include "AdapterPnpPower.hpp"
#include "NxAdapter.hpp"
#include "ndisminiport/NxNdisMiniport.hpp"
#include "ndisminiport/NxNdisPnpPower.hpp"
#include "NxPrivateGlobals.hpp"

#include "AdapterPnpPower.tmh"

#define SMENGINE_TAG 'tSdA'

static
EVT_WDF_TIMER
    EvtInitializePowerManagement;

_Use_decl_annotations_
void
EvtInitializePowerManagement(
    WDFTIMER Timer
)
{
    auto adapter = GetNxAdapterFromHandle(WdfTimerGetParentObject(Timer));
    adapter->InitializePowerManagement();
}

_Use_decl_annotations_
AdapterPnpPower::AdapterPnpPower(
    NxAdapter * Adapter
)
    : m_adapter(*Adapter)
{
    InitializeListHead(&m_startRequests);
    InitializeListHead(&m_stopRequests);
}

_Use_decl_annotations_
AdapterPnpPower::~AdapterPnpPower(
    void
)
{
    if (IsInitialized())
    {
        EnqueueEvent(Event::Destroy);
    }

    auto ndisHandle = m_adapter.GetNdisHandle();

    if (ndisHandle != nullptr)
    {
        auto const ntStatus = m_adapter.m_ndisMiniport->RemoveNdisMiniport();

        NT_FRE_ASSERT(ntStatus == STATUS_SUCCESS);
    }
}

_Use_decl_annotations_
NTSTATUS
AdapterPnpPower::Initialize(
    void
)
{
#ifdef _KERNEL_MODE
    DEVICE_OBJECT * wdmDeviceObject = nullptr;
    if (m_adapter.IsDevicelessAdapter())
    {
        wdmDeviceObject = reinterpret_cast<DEVICE_OBJECT *>(WdfDriverWdmGetDriverObject(WdfGetDriver()));
    }
    else
    {
        wdmDeviceObject = WdfDeviceWdmGetDeviceObject(m_adapter.GetDevice());
    }
    StateMachineEngineConfig smConfig(wdmDeviceObject, SMENGINE_TAG);
#else
    StateMachineEngineConfig smConfig;
#endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AdapterPnpPowerStateMachine::Initialize(smConfig),
        "Failed to initialize AdapterPnpPower state machine");

    // Acquire a reference to ensure the object is not destroyed while there are
    // PnP/Power or user events queued in the state machine
    WdfObjectReferenceWithTag(m_adapter.GetFxObject(), this);

    if (!m_adapter.IsDevicelessAdapter())
    {
        WDF_TIMER_CONFIG timerConfig;
        WDF_TIMER_CONFIG_INIT(&timerConfig, EvtInitializePowerManagement);

        WDF_OBJECT_ATTRIBUTES timerAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
        timerAttributes.ParentObject = m_adapter.GetFxObject();
        timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;

        CX_RETURN_IF_NOT_NT_SUCCESS(
            WdfTimerCreate(
                &timerConfig,
                &timerAttributes,
                &m_initializePowerManagementTimer));
    }

    auto const ownerGlobals = m_adapter.GetPrivateGlobals();
    size_t numberOfAllowedRequestorDrivers = 2;

    for (auto const & extension : m_adapter.m_adapterExtensions)
    {
        // The adapter could have been created by an extension, don't count it twice
        if (extension.GetPrivateGlobals() != ownerGlobals)
        {
            numberOfAllowedRequestorDrivers++;
        }
    }

    m_driverState = make_unique_array<DriverState>(numberOfAllowedRequestorDrivers);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_driverState);

    // NetCx itself and the driver that invoked NetAdapterCreate are allowed to start/stop
    m_driverState[0].Driver = WdfGetDriver();
    m_driverState[1].Driver = ownerGlobals->ClientDriverGlobals->Driver;

    // Also give permission to adapter extension drivers
    for (size_t i = 0, j = 0; i < m_adapter.m_adapterExtensions.count(); i++)
    {
        auto const extensionGlobals = m_adapter.m_adapterExtensions[i].GetPrivateGlobals();

        // Skip this extension driver if it invoked NetAdapterCreate, it is already in the list
        if (extensionGlobals != ownerGlobals)
        {
            m_driverState[2 + j++].Driver = extensionGlobals->ClientDriverGlobals->Driver;
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
bool
AdapterPnpPower::DriverStartedAdapter(
    NX_PRIVATE_GLOBALS const * RequestorGlobals
)
{
    auto state = GetDriverState(RequestorGlobals->ClientDriverGlobals->Driver);
    return state->Started;
}

_Use_decl_annotations_
AdapterPnpPower::DriverState *
AdapterPnpPower::GetDriverState(
    WDFDRIVER Driver
)
{
    for (auto & state : m_driverState)
    {
        if (state.Driver == Driver)
        {
            return &state;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
NTSTATUS
AdapterPnpPower::NetAdapterStart(
    NX_PRIVATE_GLOBALS const * RequestorGlobals
)
{
    return NetAdapterStart(RequestorGlobals->ClientDriverGlobals->Driver);
}

_Use_decl_annotations_
NTSTATUS
AdapterPnpPower::NetAdapterStart(
    WDFDRIVER RequestorDriver
)
{
    NetAdapterStartContext context;
    context.Requestor = GetDriverState(RequestorDriver);

    // Queue the request and fire an event to the state machine
    KAcquireSpinLock lock{ m_lock };
    InsertTailList(&m_startRequests, &context.Entry);
    lock.Release();

    EnqueueEvent(Event::NetAdapterStart);

    // Wait until the state machine has processed the request
    context.Processed.Wait();
    return context.Result;
}

_Use_decl_annotations_
void
AdapterPnpPower::NetAdapterStop(
    NX_PRIVATE_GLOBALS const * RequestorGlobals
)
{
    NetAdapterStop(RequestorGlobals->ClientDriverGlobals->Driver);
}

_Use_decl_annotations_
void
AdapterPnpPower::NetAdapterStop(
    WDFDRIVER RequestorDriver
)
{
    NetAdapterStopContext context;
    context.Requestor = GetDriverState(RequestorDriver);

    // Queue the request and fire an event to the state machine
    KAcquireSpinLock lock{ m_lock };
    InsertTailList(&m_stopRequests, &context.Entry);
    lock.Release();

    EnqueueEvent(Event::NetAdapterStop);

    // Wait until the state machine has processed the request
    context.Processed.Wait();
}

_Use_decl_annotations_
void
AdapterPnpPower::IoStart(
    void
)
{
    EnqueueEvent(Event::IoStart);
    m_ioEventProcessed.Wait();
}

_Use_decl_annotations_
void
AdapterPnpPower::IoStartNoWait(
    void
)
{
    EnqueueEvent(Event::IoStart);
}

_Use_decl_annotations_
void
AdapterPnpPower::IoStop(
    WDF_POWER_DEVICE_STATE TargetPowerState
)
{
    switch (TargetPowerState)
    {
        case WdfPowerDeviceD1:
            EnqueueEvent(Event::IoStopD1);
            break;

        case WdfPowerDeviceD2:
            EnqueueEvent(Event::IoStopD2);
            break;

        case WdfPowerDeviceD3:
            EnqueueEvent(Event::IoStopD3);
            break;

        case WdfPowerDeviceD3Final:
            EnqueueEvent(Event::IoStopD3Final);
            break;
    }

    m_ioEventProcessed.Wait();
}

_Use_decl_annotations_
void
AdapterPnpPower::IoStopNoWait(
    void
)
{
    EnqueueEvent(Event::IoStopD3Final);
}


_Use_decl_annotations_
void
AdapterPnpPower::IoInterfaceArrived(
    void
)
{
    //
    // This method can only be invoked by NetInterfaceChangeNotification
    // for PnP device backed NETADAPTER.
    //
    EnqueueEvent(Event::IoInterfaceArrival);
    m_adapter.m_ndisPnpPower->NotifyWmiNdisMiniportArrival();
}

_Use_decl_annotations_
void
AdapterPnpPower::SurpriseRemoved(
    void
)
{
    EnqueueEvent(Event::SurpriseRemove);
}

_Use_decl_annotations_
void
AdapterPnpPower::PnPRebalance(
    void
)
{
    EnqueueEvent(Event::DeviceRestart);
}

_Use_decl_annotations_
void
AdapterPnpPower::Cleanup(
    void
)
{
    if (IsInitialized())
    {
        EnqueueEvent(Event::Cleanup);
    }
}

//
// Below are the implementations for state machine entry functions. To understand
// what is their purpose please refer to the state machine diagram.
//

_Use_decl_annotations_
AdapterPnpPower::Event
AdapterPnpPower::ShouldStartNetworkInterface(
    void
)
{
    //
    // If any driver still wants this adapter stopped we can't start the network interface
    //

    for (auto const & state : m_driverState)
    {
        if (!state.Started && !state.StartInProgress)
        {
            return Event::No;
        }
    }

    return Event::Yes;
}

_Use_decl_annotations_
void
AdapterPnpPower::DeferStartNetworkInterface(
    void
)
{
    m_netAdapterStartContext->Result = STATUS_SUCCESS;
}

_Use_decl_annotations_
void
AdapterPnpPower::StartNetworkInterfaceInvalidDeviceState(
    void
)
{
    m_netAdapterStartContext->Result = STATUS_INVALID_DEVICE_REQUEST;
}

_Use_decl_annotations_
AdapterPnpPower::Event
AdapterPnpPower::StartNetworkInterface(
    void
)
{
    auto & ntStatus = m_netAdapterStartContext->Result;

    ntStatus = m_adapter.RegisterRingExtensions();

    if (ntStatus != STATUS_SUCCESS)
    {
        return Event::Fail;
    }

    m_adapter.m_packetMonitor.RegisterAdapter();

#if NETCX_UMDF
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES datapathCapabilities{};
    m_adapter.GetDatapathCapabilities(&datapathCapabilities);

    BridgeToKernel & redirector = m_adapter.GetBridgeToKernel();
    ntStatus = redirector.SetDatapathCapabilities(datapathCapabilities);

    if (ntStatus != STATUS_SUCCESS)
    {
        LogInfo(
            FLAG_ADAPTER,
            "Failed to cache datapath capabilities on kernel redirector. NETADAPTER %p",
            m_adapter.GetFxObject());

        return Event::Fail;
    }
#endif

    ntStatus = m_adapter.m_ndisMiniport->StartNdisMiniport();

    if (ntStatus != STATUS_SUCCESS)
    {
        return Event::Fail;
    }

    return Event::Success;
}

_Use_decl_annotations_
void
AdapterPnpPower::StopNetworkInterface(
    void
)
{
    m_adapter.m_ndisMiniport->StopNdisMiniport();
    m_adapter.m_packetMonitor.UnregisterAdapter();

    if (m_initializePowerManagementTimer != WDF_NO_HANDLE)
    {
        WdfTimerStop(m_initializePowerManagementTimer, TRUE);
    }

    // At this point no NDIS initiated IO is possible. Drop the power reference
    // acquired during initialization
    m_adapter.ReleaseOutstandingPowerReferences();

    m_areBindingsAllowed = false;
}

_Use_decl_annotations_
void
AdapterPnpPower::StopNetworkInterfaceNoOp(
    void
)
{
}

_Use_decl_annotations_
void
AdapterPnpPower::StartIo(
    void
)
{
    m_adapter.EnqueueEvent(NxAdapter::Event::DatapathCreate);
    m_adapter.m_transitionComplete.Wait();

    m_adapter.m_ndisMiniport->StartNdisMiniportDataPath();
}

_Use_decl_annotations_
void
AdapterPnpPower::StartIoD0(
    void
)
{
    m_adapter.EnqueueEvent(NxAdapter::Event::DatapathCreate);
    m_adapter.m_transitionComplete.Wait();

    m_adapter.m_ndisPnpPower->SetNdisMiniportPower(PowerDeviceD0);
}

_Use_decl_annotations_
void
AdapterPnpPower::IoEventProcessed(
    void
)
{
    m_ioEventProcessed.Set();
}

_Use_decl_annotations_
void
AdapterPnpPower::StopIoD1(
    void
)
{
    m_adapter.m_ndisPnpPower->SetNdisMiniportPower(PowerDeviceD1);

    m_adapter.EnqueueEvent(NxAdapter::Event::DatapathDestroy);
    m_adapter.m_transitionComplete.Wait();
}

_Use_decl_annotations_
void
AdapterPnpPower::StopIoD2(
    void
)
{
    m_adapter.m_ndisPnpPower->SetNdisMiniportPower(PowerDeviceD2);

    m_adapter.EnqueueEvent(NxAdapter::Event::DatapathDestroy);
    m_adapter.m_transitionComplete.Wait();
}

_Use_decl_annotations_
void
AdapterPnpPower::StopIoD3(
    void
)
{
    m_adapter.m_ndisPnpPower->SetNdisMiniportPower(PowerDeviceD3);

    m_adapter.EnqueueEvent(NxAdapter::Event::DatapathDestroy);
    m_adapter.m_transitionComplete.Wait();
}

_Use_decl_annotations_
void
AdapterPnpPower::StopIoD3Final(
    void
)
{
    m_adapter.EnqueueEvent(NxAdapter::Event::DatapathDestroy);
    m_adapter.m_transitionComplete.Wait();
}

_Use_decl_annotations_
void
AdapterPnpPower::AllowBindings(
    void
)
{
    //
    // IoInterfaceArrival event can only be enqueued by NetInterfaceChangeNotification
    // for PnP device backed NETADAPTER. This is sent following NetAdapterStart and
    // after resume from Sx. Only want to handle the first IoInterfaceArrival after NetAdapterStart.
    //
    if (m_areBindingsAllowed)
    {
        LogInfo(FLAG_DEVICE, "%!FUNC! Bindings already allowed on NETADAPTER: %p.", m_adapter.GetFxObject());
        // Already allowed, no-op
        return;
    }
    m_areBindingsAllowed = true;

    LogInfo(FLAG_DEVICE, "%!FUNC! Allowing bindings on NETADAPTER: %p", m_adapter.GetFxObject());

    WdfTimerStart(m_initializePowerManagementTimer, WDF_REL_TIMEOUT_IN_SEC(1));

    m_adapter.m_ndisPnpPower->NdisMiniportDeviceStarted();
}

_Use_decl_annotations_
void
AdapterPnpPower::ReportSurpriseRemoval(
    void
)
{
    m_adapter.m_ndisPnpPower->ReportNdisMiniportSurpriseRemove();
}

_Use_decl_annotations_
void
AdapterPnpPower::DeviceReleased(
    void
)
{
}

_Use_decl_annotations_
void
AdapterPnpPower::Deleted(
    void
)
{
    WdfObjectDereferenceWithTag(m_adapter.GetFxObject(), this);
}

_Use_decl_annotations_
void
AdapterPnpPower::DequeueNetAdapterStart(
    void
)
{
    NT_FRE_ASSERT(m_netAdapterStartContext == nullptr);

    // Pop a NetAdapterStop request from the queue. At least one request is guaranteed to exist
    KAcquireSpinLock lock{ m_lock };
    m_netAdapterStartContext = CONTAINING_RECORD(RemoveHeadList(&m_startRequests), NetAdapterStartContext, Entry);
    lock.Release();

    m_netAdapterStartContext->Requestor->StartInProgress = true;
}

_Use_decl_annotations_
void
AdapterPnpPower::CompleteNetAdapterStart(
    void
)
{
    // Update driver state
    m_netAdapterStartContext->Requestor->LastStartResult = m_netAdapterStartContext->Result;
    m_netAdapterStartContext->Requestor->StartInProgress = false;
    m_netAdapterStartContext->Requestor->Started = NT_SUCCESS(m_netAdapterStartContext->Result);

    // Unblock NetAdapterStart calling thread
    m_netAdapterStartContext->Processed.Set();
    m_netAdapterStartContext = nullptr;
}

_Use_decl_annotations_
void
AdapterPnpPower::DequeueNetAdapterStop(
    void
)
{
    NT_FRE_ASSERT(m_netAdapterStopContext == nullptr);

    // Pop a NetAdapterStop request from the queue. At least one request is guaranteed to exist
    KAcquireSpinLock lock{ m_lock };
    m_netAdapterStopContext = CONTAINING_RECORD(RemoveHeadList(&m_stopRequests), NetAdapterStopContext, Entry);
    lock.Release();
}

_Use_decl_annotations_
void
AdapterPnpPower::CompleteNetAdapterStop(
    void
)
{
    // Update driver state
    m_netAdapterStopContext->Requestor->Started = false;

    // Unblock NetAdapterStop calling thread
    m_netAdapterStopContext->Processed.Set();
    m_netAdapterStopContext = nullptr;
}

_Use_decl_annotations_
void
AdapterPnpPower::EvtLogTransition(
    _In_ SmFx::TransitionType TransitionType,
    _In_ StateId SourceState,
    _In_ EventId ProcessedEvent,
    _In_ StateId TargetState
)
{
    LogInfo(FLAG_DEVICE, "NETADAPTER: %p"
        " [%!SMFXTRANSITIONTYPE!]"
        " From: %!ADAPTERPNPPOWERSTATE!,"
        " Event: %!ADAPTERPNPPOWEREVENT!,"
        " To: %!ADAPTERPNPPOWERSTATE!",
        m_adapter.GetFxObject(),
        static_cast<unsigned>(TransitionType),
        static_cast<unsigned>(SourceState),
        static_cast<unsigned>(ProcessedEvent),
        static_cast<unsigned>(TargetState));
}
