/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    NxDeviceStateMachine.h

Abstract:

    The NxDevice state machine template base class.

    This is a template class that is expected to be derived by the actual implementation class. The
    class uses the popular "Curiously-Recurring-Template-Pattern" (CRTP) to statically dispatch
    operations to the implementation class.

    The #include for this header can be nested inside a namespace. In order to facilitate that,
    this header cannot include any other headers.

    Note: This is an auto-generated file. Do not edit manually.

Environment:

    Kernel-mode & user-mode. C++ only.

--*/

#pragma once

// Check that the version of SmFx the consumer is linking against matches the version of the
// generated code.
static_assert(SMFX_VER == 2, "Version mismatch between targeted SmFx and generated code");

// The state machine class. This class is typically derived by an implementation class, providing
// the class type itself as the template parameter. The derived class implements the state machine
// operations that this class calls through CRTP dispatch. An implementation class would typically
// inherit this class privately, so as to keep the internals (the Event enumeration, engine methods,
// etc.) private.

template<typename T>
class NxDeviceStateMachine
{
public:

    // Config structure used when creating an instance of the state machine class.
    typedef struct StateMachineEngineConfig
    {
    #ifdef _KERNEL_MODE
        // Constructor for kernel-mode where we have some required parameters.
        StateMachineEngineConfig(
            _In_ PDEVICE_OBJECT deviceObject,
            _In_ uint32_t poolTag) :
            deviceObject(deviceObject),
            poolTag(poolTag)
        {
        }
    #endif // #ifdef _KERNEL_MODE

    #ifdef _KERNEL_MODE
        // In kernel mode, the device object is required for allocating the work item.
        PDEVICE_OBJECT deviceObject = nullptr;
    #endif  // #ifdef _KERNEL_MODE

    #ifdef _KERNEL_MODE
        // Pool tag to be used for any memory the engine allocates. Used only in kernel-mode where
        // pool tagging is supported.
        uint32_t poolTag = 0;
    #endif  // #ifdef _KERNEL_MODE

    } StateMachineEngineConfig;

    // Events for this state machine. The engine interprets these as event indices.
    enum class Event : SmFx::EventIndex
    {
        _noevent_ = 0,
        AdapterHalted = 1,
        CxPostReleaseHardware = 2,
        CxPostSelfManagedIoRestart = 3,
        CxPostSelfManagedIoStart = 4,
        CxPrePrepareHardware = 5,
        CxPrePrepareHardwareFailedCleanup = 6,
        CxPreReleaseHardware = 7,
        CxPreSelfManagedIoSuspend = 8,
        No = 9,
        RefreshAdapterList = 10,
        StartComplete = 11,
        SyncFail = 12,
        SyncSuccess = 13,
        WdfDeviceObjectCleanup = 14,
        Yes = 15,
    };

    // Enumeration of state IDs.
    enum class StateId : SmFx::StateId
    {
        _nostate_ = 0,
        CxPrepareHardwareFailed = 1,
        DeviceAddFailedReportToNdis = 2,
        DeviceReleasingWaitForNdisHalt = 3,
        Initialized = 4,
        InitializedPrePrepareHardware = 5,
        InitializedWaitForStart = 6,
        RebalancingPrepareForStart = 7,
        RebalancingPrePrepareHardware = 8,
        RebalancingReinitializeSelfManagedIo = 9,
        Released = 10,
        ReleasedPrepareRebalance = 11,
        ReleasedReportToNdis = 12,
        ReleasingAreAllAdaptersHalted = 13,
        ReleasingIsSurpriseRemoved = 14,
        ReleasingReportPreReleaseToNdis = 15,
        ReleasingReportSurpriseRemoveToNdis = 16,
        ReleasingSurpriseRemovedAreAllAdaptersHalted = 17,
        ReleasingSurpriseRemovedReportPreReleaseToNdis = 18,
        ReleasingSurpriseRemovedWaitForNdisHalt = 19,
        ReleasingSuspendIo = 20,
        ReleasingWaitClientRelease = 21,
        ReleasingWaitForReleaseHardware = 22,
        Removed = 23,
        StartedD0 = 24,
        StartedDx = 25,
        StartedEnteringHighPower = 26,
        StartedEnteringLowPower = 27,
        StartingCheckPowerPolicyOwnership = 28,
        StartingCompleteStart = 29,
        StartingD0 = 30,
        StartingInitializeSelfManagedIo = 31,
        WaitForCleanup = 32,
    };

    // Enumeration of event IDs.
    enum class EventId : SmFx::EventId
    {
        _noevent_ = 0,
        AdapterHalted = 1,
        CxPostReleaseHardware = 2,
        CxPostSelfManagedIoRestart = 3,
        CxPostSelfManagedIoStart = 4,
        CxPrePrepareHardware = 5,
        CxPrePrepareHardwareFailedCleanup = 6,
        CxPreReleaseHardware = 7,
        CxPreSelfManagedIoSuspend = 8,
        No = 9,
        RefreshAdapterList = 10,
        StartComplete = 11,
        SyncFail = 12,
        SyncSuccess = 13,
        WdfDeviceObjectCleanup = 14,
        Yes = 15,
    };

    // Event specification table.
    // This is made public mainly so that code that enqueues events may look up the event ID
    // corresponding to that event, if necessary.
    static const SmFx::EVENT_SPECIFICATION c_eventTable[];

    // Engine methods. This is a facade for the state machine engine interface, mainly for providing
    // a simpler and more type-safe interface.

    // See SmFx::StateMachineEngine::Initialize in SmFx.h.
    // For user-mode, the config struct doesn't have any required parameters. So specify a default-
    // constructed config struct as a default parameter.
    _Must_inspect_result_
    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    Initialize(
#ifdef _KERNEL_MODE
        _In_ const StateMachineEngineConfig& config
#else // #ifdef _KERNEL_MODE
        _In_ const StateMachineEngineConfig& config = {}
#endif // #ifdef _KERNEL_MODE
        );

    // See SmFx::StateMachineEngine::IsInitialized in SmFx.h.
    bool
    IsInitialized();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    EnqueueEvent(
        _In_ Event event);

    // Typedefs for the different operation types.

    typedef
    _IRQL_requires_max_(DISPATCH_LEVEL)
    Event
    SyncOperationDispatch();

    typedef
    _IRQL_requires_max_(PASSIVE_LEVEL)
    Event
    SyncOperationPassive();

    typedef
    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    AsyncOperationDispatch();

    typedef
    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    AsyncOperationPassive();

    typedef
    _IRQL_requires_max_(DISPATCH_LEVEL)
    SmFx::StopTimerResult
    StopTimerOperation();

protected:

    // This class cannot be instantiated stand-alone; it has to be inherited by a class
    // that implements the operations. Hence, the constructor is protected.
    NxDeviceStateMachine() = default;

    // This class is expected to be a base class, but polymorphic access through this base class is
    // not allowed. Hence, the destructor is protected.
    ~NxDeviceStateMachine();

    // No copy semantices, until we can decide what exactly it means to "copy" a state machine.

    NxDeviceStateMachine(
        const NxDeviceStateMachine&) = delete;

    NxDeviceStateMachine&
    operator=(
        const NxDeviceStateMachine&) = delete;

    // Typedef for the machine exception callback method. The derived class is expected to implement
    // a method of this type, with the name "EvtLogMachineException".
    typedef
    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    EvtLogMachineExceptionFunc(
        _In_ SmFx::MachineException exception,
        _In_ EventId relevantEvent,
        _In_ StateId relevantState);

    // Typedef for the log event enqueue callback method. The derived class is expected to implement
    // a method of this type, with the name "EvtLogEventEnqueue".
    typedef
    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    EvtLogEventEnqueueFunc(
        _In_ EventId relevantEvent);

    // Typedef for the log transition callback method. The derived class is expected to implement
    // a method of this type, with the name "EvtLogTransition".
    typedef
    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    EvtLogTransitionFunc(
        _In_ SmFx::TransitionType transitionType,
        _In_ StateId sourceState,
        _In_ EventId processedEvent,
        _In_ StateId targetState);

    // Typedef for the machine destroyed callback method. The derived class is expected to implement
    // a method of this type, with the name "EvtMachineDestroyed".
    typedef
    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    EvtMachineDestroyedFunc();

private:

    // State specification table.
    static const SmFx::STATE_SPECIFICATION c_stateTable[];

    // A note on the SFINAE usages below:
    // For each engine callback, this autogenerated class sets a function pointer with the engine
    // only if the implementation class has an appropriately-named method for that callback. This
    // is achieved using SFINAE. The last parameter of each overload is simply a dummy parameter
    // to establish a precedence order between the overloads. The method is called with a bool
    // value, so that the bool version gets precendence over the int version in case both overloads
    // are valid, which will be the case if the implementation class does implement the method in
    // question.

    // Use SFINAE to populate the machine exception callback pointer only if the implementation
    // class has provided a log machine exception callback method named EvtLogMachineException.

    template<typename U>
    void
    SetLogMachineExceptionCallback(
        _Inout_ SmFx::STATE_MACHINE_ENGINE_CONFIG* config,
        _In_ int)
    {
        // This is the version of the overload that gets picked if the implementation class
        // does not have an EvtLogMachineException method.
        config->logExceptionCallback = nullptr;
    }

    template<typename U>
    decltype(&U::EvtLogMachineException)
    SetLogMachineExceptionCallback(
        _Inout_ SmFx::STATE_MACHINE_ENGINE_CONFIG* config,
        _In_ bool)
    {
        // This is the version of the overload that gets picked if the implementation class
        // does have an EvtLogMachineException method.
        config->logExceptionCallback = &EvtLogMachineExceptionThunk;
        return nullptr;
    }

    // Machine exception callback. Performs CRTP dispatch to a method that is expected to be
    // implemented by the derived class.

    static SmFx::EvtLogMachineExceptionFunc EvtLogMachineExceptionThunk;

    // Use SFINAE to populate the logging callback pointers only if the implementation
    // class has provided a log event enqueue callback method named EvtLogEventEnqueue.

    template<typename U>
    void
    SetLogEventEnqueueCallback(
        _Inout_ SmFx::STATE_MACHINE_ENGINE_CONFIG* config,
        _In_ int)
    {
        // This is the version of the overload that gets picked if the implementation class
        // does not have an EvtLogEventEnqueue method.
        config->logEventEnqueueCallback = nullptr;
    }

    template<typename U>
    decltype(&U::EvtLogEventEnqueue)
    SetLogEventEnqueueCallback(
        _Inout_ SmFx::STATE_MACHINE_ENGINE_CONFIG* config,
        _In_ bool)
    {
        // This is the version of the overload that gets picked if the implementation class
        // does have an EvtLogEventEnqueue method.
        config->logEventEnqueueCallback = &EvtLogEventEnqueueThunk;
        return nullptr;
    }

    static SmFx::EvtLogEventEnqueueFunc EvtLogEventEnqueueThunk;

    // Use SFINAE to populate the logging callback pointers only if the implementation
    // class has provided a log transition callback method named EvtLogTransition.

    template<typename U>
    void
    SetLogTransitionCallback(
        _Inout_ SmFx::STATE_MACHINE_ENGINE_CONFIG* config,
        _In_ int)
    {
        // This is the version of the overload that gets picked if the implementation class
        // does not have an EvtLogTransition method.
        config->logTransitionCallback = nullptr;
    }

    template<typename U>
    decltype(&U::EvtLogTransition)
    SetLogTransitionCallback(
        _Inout_ SmFx::STATE_MACHINE_ENGINE_CONFIG* config,
        _In_ bool)
    {
        // This is the version of the overload that gets picked if the implementation class
        // does have an EvtLogTransition method.
        config->logTransitionCallback = &EvtLogTransitionThunk;
        return nullptr;
    }

    static SmFx::EvtLogTransitionFunc EvtLogTransitionThunk;

    // Use SFINAE to populate the machine-destroyed callback pointer only if the implementation
    // class has provided a callback method named EvtMachineDestroyed.

    template<typename U>
    void
    SetMachineDestroyedCallback(
        _Inout_ SmFx::STATE_MACHINE_ENGINE_CONFIG* config,
        _In_ int)
    {
        // This is the version of the overload that gets picked if the implementation class
        // does not have an EvtMachineDestroyed method.
        config->machineDestroyedCallback = nullptr;
    }

    template<typename U>
    decltype(&U::EvtMachineDestroyed)
    SetMachineDestroyedCallback(
        _Inout_ SmFx::STATE_MACHINE_ENGINE_CONFIG* config,
        _In_ bool)
    {
        // This is the version of the overload that gets picked if the implementation class
        // does have an EvtMachineDestroyed method.
        config->machineDestroyedCallback = &EvtMachineDestroyedThunk;
        return nullptr;
    }

    static SmFx::EvtMachineDestroyedFunc EvtMachineDestroyedThunk;

    // Auto-generated declarations are placed into private inner classes to avoid
    // name collisions with other auto-generated declarations.

    // State entry functions.
    struct EntryFuncs
    {
        static SmFx::StateEntryFunction CxPrepareHardwareFailedEntry;
        static SmFx::StateEntryFunction DeviceAddFailedReportToNdisEntry;
        static SmFx::StateEntryFunction InitializedPrePrepareHardwareEntry;
        static SmFx::StateEntryFunction RebalancingPrePrepareHardwareEntry;
        static SmFx::StateEntryFunction RebalancingReinitializeSelfManagedIoEntry;
        static SmFx::StateEntryFunction ReleasedPrepareRebalanceEntry;
        static SmFx::StateEntryFunction ReleasedReportToNdisEntry;
        static SmFx::StateEntryFunction ReleasingAreAllAdaptersHaltedEntry;
        static SmFx::StateEntryFunction ReleasingIsSurpriseRemovedEntry;
        static SmFx::StateEntryFunction ReleasingReportPreReleaseToNdisEntry;
        static SmFx::StateEntryFunction ReleasingReportSurpriseRemoveToNdisEntry;
        static SmFx::StateEntryFunction ReleasingSurpriseRemovedAreAllAdaptersHaltedEntry;
        static SmFx::StateEntryFunction ReleasingSurpriseRemovedReportPreReleaseToNdisEntry;
        static SmFx::StateEntryFunction ReleasingSuspendIoEntry;
        static SmFx::StateEntryFunction RemovedEntry;
        static SmFx::StateEntryFunction StartedEnteringHighPowerEntry;
        static SmFx::StateEntryFunction StartedEnteringLowPowerEntry;
        static SmFx::StateEntryFunction StartingCheckPowerPolicyOwnershipEntry;
        static SmFx::StateEntryFunction StartingCompleteStartEntry;
        static SmFx::StateEntryFunction StartingInitializeSelfManagedIoEntry;
    };

    // Internal transition actions.
    struct Actions
    {
        static SmFx::InternalTransitionAction DeviceReleasingWaitForNdisHaltActionOnRefreshAdapterList;
        static SmFx::InternalTransitionAction InitializedActionOnRefreshAdapterList;
        static SmFx::InternalTransitionAction InitializedWaitForStartActionOnRefreshAdapterList;
        static SmFx::InternalTransitionAction RebalancingPrepareForStartActionOnRefreshAdapterList;
        static SmFx::InternalTransitionAction ReleasingSurpriseRemovedWaitForNdisHaltActionOnRefreshAdapterList;
        static SmFx::InternalTransitionAction ReleasingWaitClientReleaseActionOnRefreshAdapterList;
        static SmFx::InternalTransitionAction StartedD0ActionOnRefreshAdapterList;
        static SmFx::InternalTransitionAction StartedDxActionOnRefreshAdapterList;
        static SmFx::InternalTransitionAction StartingD0ActionOnRefreshAdapterList;
    };

    // Enumeration of state indices.
    enum class StateIndex : SmFx::StateIndex
    {
        _nostate_ = 0,
        CxPrepareHardwareFailed = 1,
        DeviceAddFailedReportToNdis = 2,
        DeviceReleasingWaitForNdisHalt = 3,
        Initialized = 4,
        InitializedPrePrepareHardware = 5,
        InitializedWaitForStart = 6,
        RebalancingPrepareForStart = 7,
        RebalancingPrePrepareHardware = 8,
        RebalancingReinitializeSelfManagedIo = 9,
        Released = 10,
        ReleasedPrepareRebalance = 11,
        ReleasedReportToNdis = 12,
        ReleasingAreAllAdaptersHalted = 13,
        ReleasingIsSurpriseRemoved = 14,
        ReleasingReportPreReleaseToNdis = 15,
        ReleasingReportSurpriseRemoveToNdis = 16,
        ReleasingSurpriseRemovedAreAllAdaptersHalted = 17,
        ReleasingSurpriseRemovedReportPreReleaseToNdis = 18,
        ReleasingSurpriseRemovedWaitForNdisHalt = 19,
        ReleasingSuspendIo = 20,
        ReleasingWaitClientRelease = 21,
        ReleasingWaitForReleaseHardware = 22,
        Removed = 23,
        StartedD0 = 24,
        StartedDx = 25,
        StartedEnteringHighPower = 26,
        StartedEnteringLowPower = 27,
        StartingCheckPowerPolicyOwnership = 28,
        StartingCompleteStart = 29,
        StartingD0 = 30,
        StartingInitializeSelfManagedIo = 31,
        WaitForCleanup = 32,
    };

    // Enumeration of submachine names.
    enum class SubmachineName : SmFx::SubmachineIndex
    {
        _nosubmachine_ = 0,
        StateMachine = 1,
    };

    // Table of submachine specifications.
    static const SmFx::SUBMACHINE_SPECIFICATION c_machineTable[];

    // Declarations of pop transitions for each state that has any.
    struct PopTransitions
    {
        static const SmFx::POP_TRANSITION c_removed[];
    };

    // Declarations of internal transitions for each state that has any.
    struct InternalTransitions
    {
        static const SmFx::INTERNAL_TRANSITION c_deviceReleasingWaitForNdisHalt[];
        static const SmFx::INTERNAL_TRANSITION c_initialized[];
        static const SmFx::INTERNAL_TRANSITION c_initializedWaitForStart[];
        static const SmFx::INTERNAL_TRANSITION c_rebalancingPrepareForStart[];
        static const SmFx::INTERNAL_TRANSITION c_released[];
        static const SmFx::INTERNAL_TRANSITION c_releasingSurpriseRemovedWaitForNdisHalt[];
        static const SmFx::INTERNAL_TRANSITION c_releasingWaitClientRelease[];
        static const SmFx::INTERNAL_TRANSITION c_startedD0[];
        static const SmFx::INTERNAL_TRANSITION c_startedDx[];
        static const SmFx::INTERNAL_TRANSITION c_startingD0[];
    };

    // Declarations of external transitions for each state that has any.
    struct ExternalTransitions
    {
        static const SmFx::EXTERNAL_TRANSITION c_cxPrepareHardwareFailed[];
        static const SmFx::EXTERNAL_TRANSITION c_deviceAddFailedReportToNdis[];
        static const SmFx::EXTERNAL_TRANSITION c_deviceReleasingWaitForNdisHalt[];
        static const SmFx::EXTERNAL_TRANSITION c_initialized[];
        static const SmFx::EXTERNAL_TRANSITION c_initializedPrePrepareHardware[];
        static const SmFx::EXTERNAL_TRANSITION c_initializedWaitForStart[];
        static const SmFx::EXTERNAL_TRANSITION c_rebalancingPrepareForStart[];
        static const SmFx::EXTERNAL_TRANSITION c_rebalancingPrePrepareHardware[];
        static const SmFx::EXTERNAL_TRANSITION c_rebalancingReinitializeSelfManagedIo[];
        static const SmFx::EXTERNAL_TRANSITION c_released[];
        static const SmFx::EXTERNAL_TRANSITION c_releasedPrepareRebalance[];
        static const SmFx::EXTERNAL_TRANSITION c_releasedReportToNdis[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingAreAllAdaptersHalted[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingIsSurpriseRemoved[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingReportPreReleaseToNdis[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingReportSurpriseRemoveToNdis[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingSurpriseRemovedAreAllAdaptersHalted[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingSurpriseRemovedReportPreReleaseToNdis[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingSurpriseRemovedWaitForNdisHalt[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingSuspendIo[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingWaitClientRelease[];
        static const SmFx::EXTERNAL_TRANSITION c_releasingWaitForReleaseHardware[];
        static const SmFx::EXTERNAL_TRANSITION c_startedD0[];
        static const SmFx::EXTERNAL_TRANSITION c_startedDx[];
        static const SmFx::EXTERNAL_TRANSITION c_startedEnteringHighPower[];
        static const SmFx::EXTERNAL_TRANSITION c_startedEnteringLowPower[];
        static const SmFx::EXTERNAL_TRANSITION c_startingCheckPowerPolicyOwnership[];
        static const SmFx::EXTERNAL_TRANSITION c_startingCompleteStart[];
        static const SmFx::EXTERNAL_TRANSITION c_startingD0[];
        static const SmFx::EXTERNAL_TRANSITION c_startingInitializeSelfManagedIo[];
    };

    // Declarations of deferred events for each state that has any.
    struct DeferredEvents
    {
        static const SmFx::EventIndex c_rebalancingPrepareForStart[];
        static const SmFx::EventIndex c_startedDx[];
    };

    // Declarations of purged events for each state that has any.
    struct PurgeEvents
    {
        static const SmFx::EventIndex c_releasedPrepareRebalance[];
        static const SmFx::EventIndex c_releasingWaitClientRelease[];
    };

    // Declarations of stop-timer-on-exit details for each state that has it.
    struct StopTimerOnExitDetails
    {
    };

    // Declarations of slot arrays for states that have a non-empty slot array.
    struct SlotArrays
    {
        static const SmFx::StateSlot c_cxPrepareHardwareFailed[];
        static const SmFx::StateSlot c_deviceAddFailedReportToNdis[];
        static const SmFx::StateSlot c_deviceReleasingWaitForNdisHalt[];
        static const SmFx::StateSlot c_initialized[];
        static const SmFx::StateSlot c_initializedPrePrepareHardware[];
        static const SmFx::StateSlot c_initializedWaitForStart[];
        static const SmFx::StateSlot c_rebalancingPrepareForStart[];
        static const SmFx::StateSlot c_rebalancingPrePrepareHardware[];
        static const SmFx::StateSlot c_rebalancingReinitializeSelfManagedIo[];
        static const SmFx::StateSlot c_released[];
        static const SmFx::StateSlot c_releasedPrepareRebalance[];
        static const SmFx::StateSlot c_releasedReportToNdis[];
        static const SmFx::StateSlot c_releasingAreAllAdaptersHalted[];
        static const SmFx::StateSlot c_releasingIsSurpriseRemoved[];
        static const SmFx::StateSlot c_releasingReportPreReleaseToNdis[];
        static const SmFx::StateSlot c_releasingReportSurpriseRemoveToNdis[];
        static const SmFx::StateSlot c_releasingSurpriseRemovedAreAllAdaptersHalted[];
        static const SmFx::StateSlot c_releasingSurpriseRemovedReportPreReleaseToNdis[];
        static const SmFx::StateSlot c_releasingSurpriseRemovedWaitForNdisHalt[];
        static const SmFx::StateSlot c_releasingSuspendIo[];
        static const SmFx::StateSlot c_releasingWaitClientRelease[];
        static const SmFx::StateSlot c_releasingWaitForReleaseHardware[];
        static const SmFx::StateSlot c_removed[];
        static const SmFx::StateSlot c_startedD0[];
        static const SmFx::StateSlot c_startedDx[];
        static const SmFx::StateSlot c_startedEnteringHighPower[];
        static const SmFx::StateSlot c_startedEnteringLowPower[];
        static const SmFx::StateSlot c_startingCheckPowerPolicyOwnership[];
        static const SmFx::StateSlot c_startingCompleteStart[];
        static const SmFx::StateSlot c_startingD0[];
        static const SmFx::StateSlot c_startingInitializeSelfManagedIo[];
    };

    // Declaration of the complete state machine specification.
    static const SmFx::STATE_MACHINE_SPECIFICATION c_specification;

    // The state machine engine.
    SmFx::StateMachineEngine m_engine;
};

// Engine method implementations.

template<typename T>
NTSTATUS
NxDeviceStateMachine<T>::Initialize(
    const StateMachineEngineConfig& config)
/*++

Routine Description:

    Initializes the engine and also induces the implicit initial transition. So the caller must be
    prepared to execute state machine operations even before this method returns.

--*/
{
    SmFx::STATE_MACHINE_ENGINE_CONFIG engineConfig{};

    engineConfig.machineSpec = &c_specification;
    engineConfig.context = reinterpret_cast<void*>(this);

#ifdef _KERNEL_MODE
    engineConfig.poolTag = config.poolTag;
    engineConfig.deviceObject = config.deviceObject;
#else // #ifdef _KERNEL_MODE
    // There is nothing configurable in user-mode at the moment.
    UNREFERENCED_PARAMETER(config);
#endif // #ifdef _KERNEL_MODE

    // The generation script has automatically determined if a worker is required or not.
#ifdef _KERNEL_MODE
    engineConfig.isWorkerRequired = true;
#else // #ifdef _KERNEL_MODE
    engineConfig.isWorkerRequired = true;
#endif // #ifdef _KERNEL_MODE

    // The generation script has automatically determined the maximum stack size required.
    engineConfig.stackSize = 1;

    SetLogMachineExceptionCallback<T>(&engineConfig, true);
    SetLogEventEnqueueCallback<T>(&engineConfig, true);
    SetLogTransitionCallback<T>(&engineConfig, true);
    SetMachineDestroyedCallback<T>(&engineConfig, true);

    return m_engine.Initialize(engineConfig);
}

template<typename T>
bool
NxDeviceStateMachine<T>::IsInitialized()
{
    return m_engine.IsInitialized();
}

template<typename T>
NxDeviceStateMachine<T>::~NxDeviceStateMachine()
{
}

template<typename T>
void
NxDeviceStateMachine<T>::EnqueueEvent(
    typename NxDeviceStateMachine<T>::Event event)
{
    m_engine.EnqueueEvent(static_cast<SmFx::EventIndex>(event));
}

template<typename T>
void
NxDeviceStateMachine<T>::EvtLogMachineExceptionThunk(
    void* context,
    SmFx::MachineException exception,
    SmFx::EventId relevantEvent,
    SmFx::StateId relevantState)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->EvtLogMachineException(
        exception,
        static_cast<EventId>(relevantEvent),
        static_cast<StateId>(relevantState));
}

template<typename T>
void
NxDeviceStateMachine<T>::EvtLogEventEnqueueThunk(
    void* context,
    SmFx::EventId relevantEvent)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->EvtLogEventEnqueue(static_cast<EventId>(relevantEvent));
}

template<typename T>
void
NxDeviceStateMachine<T>::EvtLogTransitionThunk(
    void* context,
    SmFx::TransitionType transitionType,
    SmFx::StateId sourceState,
    SmFx::EventId processedEvent,
    SmFx::StateId targetState)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->EvtLogTransition(
        transitionType,
        static_cast<StateId>(sourceState),
        static_cast<EventId>(processedEvent),
        static_cast<StateId>(targetState));
}

template<typename T>
void
NxDeviceStateMachine<T>::EvtMachineDestroyedThunk(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->EvtMachineDestroyed();
}

// Definitions for each of the state entry functions.

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::CxPrepareHardwareFailedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->PrepareHardwareFailedCleanup();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::DeviceAddFailedReportToNdisEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->ReleasingReportDeviceAddFailureToNdis();

    return static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::InitializedPrePrepareHardwareEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->PrepareHardware();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::RebalancingPrePrepareHardwareEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->PrepareHardware();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::RebalancingReinitializeSelfManagedIoEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->ReinitializeSelfManagedIo();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasedPrepareRebalanceEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->PrepareForRebalance();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasedReportToNdisEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->ReleasingReportPostReleaseToNdis();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasingAreAllAdaptersHaltedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->AreAllAdaptersHalted();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasingIsSurpriseRemovedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->ReleasingIsSurpriseRemoved();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasingReportPreReleaseToNdisEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    NxDeviceStateMachine<T>::Event returnEvent = derived->ReleasingReportPreReleaseToNdis();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasingReportSurpriseRemoveToNdisEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->ReleasingReportSurpriseRemoveToNdis();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasingSurpriseRemovedAreAllAdaptersHaltedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->AreAllAdaptersHalted();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasingSurpriseRemovedReportPreReleaseToNdisEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    NxDeviceStateMachine<T>::Event returnEvent = derived->ReleasingReportPreReleaseToNdis();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::ReleasingSuspendIoEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->SuspendSelfManagedIo();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::RemovedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RemovedReportRemoveToNdis();

    return static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::StartedEnteringHighPowerEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->RestartSelfManagedIo();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::StartedEnteringLowPowerEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->SuspendSelfManagedIo();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::StartingCheckPowerPolicyOwnershipEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->CheckPowerPolicyOwnership();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::StartingCompleteStartEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    NxDeviceStateMachine<T>::Event returnEvent = derived->Started();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::StartingInitializeSelfManagedIoEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    NxDeviceStateMachine<T>::Event returnEvent = derived->InitializeSelfManagedIo();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

// Definitions for each of the internal transition actions.

template<typename T>
void
NxDeviceStateMachine<T>::Actions::DeviceReleasingWaitForNdisHaltActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

template<typename T>
void
NxDeviceStateMachine<T>::Actions::InitializedActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

template<typename T>
void
NxDeviceStateMachine<T>::Actions::InitializedWaitForStartActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

template<typename T>
void
NxDeviceStateMachine<T>::Actions::RebalancingPrepareForStartActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

template<typename T>
void
NxDeviceStateMachine<T>::Actions::ReleasingSurpriseRemovedWaitForNdisHaltActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

template<typename T>
void
NxDeviceStateMachine<T>::Actions::ReleasingWaitClientReleaseActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

template<typename T>
void
NxDeviceStateMachine<T>::Actions::StartedD0ActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

template<typename T>
void
NxDeviceStateMachine<T>::Actions::StartedDxActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

template<typename T>
void
NxDeviceStateMachine<T>::Actions::StartingD0ActionOnRefreshAdapterList(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->RefreshAdapterList();
}

// Pop transitions.

template <typename T>
const SmFx::POP_TRANSITION NxDeviceStateMachine<T>::PopTransitions::c_removed[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Return event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

// Internal transitions.

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_deviceReleasingWaitForNdisHalt[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::DeviceReleasingWaitForNdisHaltActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_initialized[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::InitializedActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_initializedWaitForStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::InitializedWaitForStartActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_rebalancingPrepareForStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::RebalancingPrepareForStartActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_released[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_releasingSurpriseRemovedWaitForNdisHalt[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::ReleasingSurpriseRemovedWaitForNdisHaltActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_releasingWaitClientRelease[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::ReleasingWaitClientReleaseActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_startedD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::StartedD0ActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_startedDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::StartedDxActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_startingD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RefreshAdapterList),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &NxDeviceStateMachine<T>::Actions::StartingD0ActionOnRefreshAdapterList
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Ignored.
        nullptr
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

// External transitions.

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_cxPrepareHardwareFailed[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::Released)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_deviceAddFailedReportToNdis[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::Removed)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_deviceReleasingWaitForNdisHalt[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingAreAllAdaptersHalted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_initialized[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPrePrepareHardware),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::InitializedPrePrepareHardware)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::Removed)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_initializedPrePrepareHardware[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::InitializedWaitForStart)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_initializedWaitForStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPostSelfManagedIoStart),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartingCheckPowerPolicyOwnership)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPrePrepareHardwareFailedCleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::CxPrepareHardwareFailed)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPreReleaseHardware),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingSurpriseRemovedReportPreReleaseToNdis)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::DeviceAddFailedReportToNdis)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_rebalancingPrepareForStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPostSelfManagedIoStart),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::RebalancingReinitializeSelfManagedIo)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPrePrepareHardwareFailedCleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::CxPrepareHardwareFailed)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPreReleaseHardware),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingSurpriseRemovedReportPreReleaseToNdis)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_rebalancingPrePrepareHardware[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::RebalancingPrepareForStart)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_rebalancingReinitializeSelfManagedIo[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartingD0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_released[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPrePrepareHardware),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasedPrepareRebalance)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::Removed)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasedPrepareRebalance[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::RebalancingPrePrepareHardware)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasedReportToNdis[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::Released)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingAreAllAdaptersHalted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::No),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::DeviceReleasingWaitForNdisHalt)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::Yes),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingWaitClientRelease)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingIsSurpriseRemoved[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::No),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingReportPreReleaseToNdis)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::Yes),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingReportSurpriseRemoveToNdis)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingReportPreReleaseToNdis[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingAreAllAdaptersHalted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingReportSurpriseRemoveToNdis[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingSurpriseRemovedAreAllAdaptersHalted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingSurpriseRemovedAreAllAdaptersHalted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::No),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingSurpriseRemovedWaitForNdisHalt)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::Yes),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingSurpriseRemovedReportPreReleaseToNdis)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingSurpriseRemovedReportPreReleaseToNdis[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingWaitClientRelease)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingSurpriseRemovedWaitForNdisHalt[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingSurpriseRemovedAreAllAdaptersHalted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingSuspendIo[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingWaitForReleaseHardware)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingWaitClientRelease[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPostReleaseHardware),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasedReportToNdis)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_releasingWaitForReleaseHardware[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPreReleaseHardware),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingIsSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_startedD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPreSelfManagedIoSuspend),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartedEnteringLowPower)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartingCompleteStart)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_startedDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPostSelfManagedIoRestart),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartedEnteringHighPower)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPreReleaseHardware),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::ReleasingIsSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_startedEnteringHighPower[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartedD0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_startedEnteringLowPower[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartedDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_startingCheckPowerPolicyOwnership[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartingInitializeSelfManagedIo)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_startingCompleteStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartedD0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_startingD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::CxPreSelfManagedIoSuspend),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartedEnteringLowPower)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartingCompleteStart)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_startingInitializeSelfManagedIo[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::StartingD0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

// Deferred events.

template <typename T>
const SmFx::EventIndex NxDeviceStateMachine<T>::DeferredEvents::c_rebalancingPrepareForStart[] =
{
    static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
    static_cast<SmFx::EventIndex>(0)
};

template <typename T>
const SmFx::EventIndex NxDeviceStateMachine<T>::DeferredEvents::c_startedDx[] =
{
    static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
    static_cast<SmFx::EventIndex>(0)
};

// Purged events.

template <typename T>
const SmFx::EventIndex NxDeviceStateMachine<T>::PurgeEvents::c_releasedPrepareRebalance[] =
{
    static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::StartComplete),
    static_cast<SmFx::EventIndex>(0)
};

template <typename T>
const SmFx::EventIndex NxDeviceStateMachine<T>::PurgeEvents::c_releasingWaitClientRelease[] =
{
    static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::AdapterHalted),
    static_cast<SmFx::EventIndex>(0)
};

// Stop-timer-on-exit details.

// Event table.
template<typename T>
const SmFx::EVENT_SPECIFICATION NxDeviceStateMachine<T>::c_eventTable[] =
{
    {
        // Unused.
        static_cast<SmFx::EventId>(0),
        SmFx::EventQueueingDisposition::Invalid
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::AdapterHalted),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::CxPostReleaseHardware),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::CxPostSelfManagedIoRestart),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::CxPostSelfManagedIoStart),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::CxPrePrepareHardware),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::CxPrePrepareHardwareFailedCleanup),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::CxPreReleaseHardware),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::CxPreSelfManagedIoSuspend),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::No),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::RefreshAdapterList),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::StartComplete),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::SyncFail),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::SyncSuccess),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::WdfDeviceObjectCleanup),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::Yes),
        SmFx::EventQueueingDisposition::Unlimited
    },
};

// Submachine table.
template<typename T>
const SmFx::SUBMACHINE_SPECIFICATION NxDeviceStateMachine<T>::c_machineTable[] =
{
    {},
    // Submachine: StateMachine
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::Initialized)
    },
};

// Slot arrays.

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_cxPrepareHardwareFailed[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::CxPrepareHardwareFailedEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_cxPrepareHardwareFailed,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_deviceAddFailedReportToNdis[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::DeviceAddFailedReportToNdisEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_deviceAddFailedReportToNdis,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_deviceReleasingWaitForNdisHalt[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_deviceReleasingWaitForNdisHalt,
    NxDeviceStateMachine<T>::InternalTransitions::c_deviceReleasingWaitForNdisHalt,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_initialized[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_initialized,
    NxDeviceStateMachine<T>::InternalTransitions::c_initialized,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_initializedPrePrepareHardware[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::InitializedPrePrepareHardwareEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_initializedPrePrepareHardware,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_initializedWaitForStart[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_initializedWaitForStart,
    NxDeviceStateMachine<T>::InternalTransitions::c_initializedWaitForStart,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_rebalancingPrepareForStart[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_rebalancingPrepareForStart,
    NxDeviceStateMachine<T>::InternalTransitions::c_rebalancingPrepareForStart,
    NxDeviceStateMachine<T>::DeferredEvents::c_rebalancingPrepareForStart,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_rebalancingPrePrepareHardware[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::RebalancingPrePrepareHardwareEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_rebalancingPrePrepareHardware,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_rebalancingReinitializeSelfManagedIo[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::RebalancingReinitializeSelfManagedIoEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_rebalancingReinitializeSelfManagedIo,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_released[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_released,
    NxDeviceStateMachine<T>::InternalTransitions::c_released,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasedPrepareRebalance[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasedPrepareRebalanceEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasedPrepareRebalance,
    NxDeviceStateMachine<T>::PurgeEvents::c_releasedPrepareRebalance,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasedReportToNdis[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasedReportToNdisEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasedReportToNdis,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingAreAllAdaptersHalted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasingAreAllAdaptersHaltedEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingAreAllAdaptersHalted,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingIsSurpriseRemoved[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasingIsSurpriseRemovedEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingIsSurpriseRemoved,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingReportPreReleaseToNdis[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasingReportPreReleaseToNdisEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingReportPreReleaseToNdis,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingReportSurpriseRemoveToNdis[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasingReportSurpriseRemoveToNdisEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingReportSurpriseRemoveToNdis,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingSurpriseRemovedAreAllAdaptersHalted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasingSurpriseRemovedAreAllAdaptersHaltedEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingSurpriseRemovedAreAllAdaptersHalted,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingSurpriseRemovedReportPreReleaseToNdis[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasingSurpriseRemovedReportPreReleaseToNdisEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingSurpriseRemovedReportPreReleaseToNdis,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingSurpriseRemovedWaitForNdisHalt[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingSurpriseRemovedWaitForNdisHalt,
    NxDeviceStateMachine<T>::InternalTransitions::c_releasingSurpriseRemovedWaitForNdisHalt,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingSuspendIo[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::ReleasingSuspendIoEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingSuspendIo,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingWaitClientRelease[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingWaitClientRelease,
    NxDeviceStateMachine<T>::InternalTransitions::c_releasingWaitClientRelease,
    NxDeviceStateMachine<T>::PurgeEvents::c_releasingWaitClientRelease,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_releasingWaitForReleaseHardware[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_releasingWaitForReleaseHardware,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_removed[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::RemovedEntry),
    NxDeviceStateMachine<T>::PopTransitions::c_removed,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_startedD0[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_startedD0,
    NxDeviceStateMachine<T>::InternalTransitions::c_startedD0,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_startedDx[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_startedDx,
    NxDeviceStateMachine<T>::InternalTransitions::c_startedDx,
    NxDeviceStateMachine<T>::DeferredEvents::c_startedDx,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_startedEnteringHighPower[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::StartedEnteringHighPowerEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_startedEnteringHighPower,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_startedEnteringLowPower[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::StartedEnteringLowPowerEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_startedEnteringLowPower,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_startingCheckPowerPolicyOwnership[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::StartingCheckPowerPolicyOwnershipEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_startingCheckPowerPolicyOwnership,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_startingCompleteStart[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::StartingCompleteStartEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_startingCompleteStart,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_startingD0[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_startingD0,
    NxDeviceStateMachine<T>::InternalTransitions::c_startingD0,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_startingInitializeSelfManagedIo[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::StartingInitializeSelfManagedIoEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_startingInitializeSelfManagedIo,
};

// State table.
template<typename T>
const SmFx::STATE_SPECIFICATION NxDeviceStateMachine<T>::c_stateTable[] =
{
    {},
    // State: CxPrepareHardwareFailed
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::CxPrepareHardwareFailed),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_cxPrepareHardwareFailed,
    },
    // State: DeviceAddFailedReportToNdis
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::DeviceAddFailedReportToNdis),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_deviceAddFailedReportToNdis,
    },
    // State: DeviceReleasingWaitForNdisHalt
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::DeviceReleasingWaitForNdisHalt),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_deviceReleasingWaitForNdisHalt,
    },
    // State: Initialized
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::Initialized),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_initialized,
    },
    // State: InitializedPrePrepareHardware
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::InitializedPrePrepareHardware),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_initializedPrePrepareHardware,
    },
    // State: InitializedWaitForStart
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::InitializedWaitForStart),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_initializedWaitForStart,
    },
    // State: RebalancingPrepareForStart
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::RebalancingPrepareForStart),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::DeferredEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_rebalancingPrepareForStart,
    },
    // State: RebalancingPrePrepareHardware
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::RebalancingPrePrepareHardware),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_rebalancingPrePrepareHardware,
    },
    // State: RebalancingReinitializeSelfManagedIo
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::RebalancingReinitializeSelfManagedIo),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_rebalancingReinitializeSelfManagedIo,
    },
    // State: Released
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::Released),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_released,
    },
    // State: ReleasedPrepareRebalance
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasedPrepareRebalance),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PurgeEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasedPrepareRebalance,
    },
    // State: ReleasedReportToNdis
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasedReportToNdis),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasedReportToNdis,
    },
    // State: ReleasingAreAllAdaptersHalted
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingAreAllAdaptersHalted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingAreAllAdaptersHalted,
    },
    // State: ReleasingIsSurpriseRemoved
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingIsSurpriseRemoved),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingIsSurpriseRemoved,
    },
    // State: ReleasingReportPreReleaseToNdis
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingReportPreReleaseToNdis),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingReportPreReleaseToNdis,
    },
    // State: ReleasingReportSurpriseRemoveToNdis
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingReportSurpriseRemoveToNdis),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingReportSurpriseRemoveToNdis,
    },
    // State: ReleasingSurpriseRemovedAreAllAdaptersHalted
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingSurpriseRemovedAreAllAdaptersHalted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingSurpriseRemovedAreAllAdaptersHalted,
    },
    // State: ReleasingSurpriseRemovedReportPreReleaseToNdis
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingSurpriseRemovedReportPreReleaseToNdis),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingSurpriseRemovedReportPreReleaseToNdis,
    },
    // State: ReleasingSurpriseRemovedWaitForNdisHalt
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingSurpriseRemovedWaitForNdisHalt),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingSurpriseRemovedWaitForNdisHalt,
    },
    // State: ReleasingSuspendIo
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingSuspendIo),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingSuspendIo,
    },
    // State: ReleasingWaitClientRelease
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingWaitClientRelease),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PurgeEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingWaitClientRelease,
    },
    // State: ReleasingWaitForReleaseHardware
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::ReleasingWaitForReleaseHardware),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_releasingWaitForReleaseHardware,
    },
    // State: Removed
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::Removed),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_removed,
    },
    // State: StartedD0
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::StartedD0),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_startedD0,
    },
    // State: StartedDx
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::StartedDx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::DeferredEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_startedDx,
    },
    // State: StartedEnteringHighPower
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::StartedEnteringHighPower),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_startedEnteringHighPower,
    },
    // State: StartedEnteringLowPower
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::StartedEnteringLowPower),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_startedEnteringLowPower,
    },
    // State: StartingCheckPowerPolicyOwnership
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::StartingCheckPowerPolicyOwnership),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_startingCheckPowerPolicyOwnership,
    },
    // State: StartingCompleteStart
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::StartingCompleteStart),
        // State flags.
        static_cast<SmFx::StateFlags>(static_cast<uint8_t>(SmFx::StateFlags::RequiresPassiveLevel) | static_cast<uint8_t>(SmFx::StateFlags::RequiresDedicatedThread)),
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_startingCompleteStart,
    },
    // State: StartingD0
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::StartingD0),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_startingD0,
    },
    // State: StartingInitializeSelfManagedIo
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::StartingInitializeSelfManagedIo),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_startingInitializeSelfManagedIo,
    },
    // State: WaitForCleanup
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::WaitForCleanup),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        SmFx::StateSlotType::None,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        nullptr,
    },
};

// Machine specification.
template<typename T>
const SmFx::STATE_MACHINE_SPECIFICATION NxDeviceStateMachine<T>::c_specification =
{
    static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::StateMachine),
    static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
    NxDeviceStateMachine<T>::c_machineTable,
    NxDeviceStateMachine<T>::c_eventTable,
    NxDeviceStateMachine<T>::c_stateTable
};
