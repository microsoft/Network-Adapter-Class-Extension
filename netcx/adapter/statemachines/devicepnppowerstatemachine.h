/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    DevicePnpPowerStateMachine.h

Abstract:

    The DevicePnpPower state machine template base class.

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
class DevicePnpPowerStateMachine
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
        AdapterCreated = 1,
        Cleanup = 2,
        Destroy = 3,
        GoToNextState = 4,
        PowerDownD1 = 5,
        PowerDownD2 = 6,
        PowerDownD3 = 7,
        PowerDownD3Final = 8,
        PowerUp = 9,
        Start = 10,
        Stop = 11,
        SurpriseRemove = 12,
    };

    // Enumeration of state IDs.
    enum class StateId : SmFx::StateId
    {
        _nostate_ = 0,
        Created = 1,
        D0 = 2,
        D3Final = 3,
        DeviceAddFailed = 4,
        Dx = 5,
        PoweringDownD1 = 6,
        PoweringDownD2 = 7,
        PoweringDownD3 = 8,
        PoweringDownD3Final = 9,
        PoweringUp = 10,
        Removed = 11,
        Restarting = 12,
        SignalD0 = 13,
        SignalD3Final = 14,
        SignalDx = 15,
        SignalStarted = 16,
        SignalStoppingD3Final = 17,
        Started = 18,
        Stopped = 19,
        StoppingD3Final = 20,
        SurpriseRemovedInD0 = 21,
        SurpriseRemovedInDx = 22,
        SurpriseRemovedNotStarted = 23,
        SurpriseRemovedSignalD0 = 24,
        SurpriseRemovedSignalD3Final = 25,
        SurpriseRemovedSignalDx = 26,
        SurpriseRemovedSignalStopDx = 27,
        SurpriseRemovedStopped = 28,
        SurpriseRemovedStopping = 29,
        SurpriseRemovingInD0 = 30,
        SurpriseRemovingInDx = 31,
        SurpriseRemovingNotStarted = 32,
    };

    // Enumeration of event IDs.
    enum class EventId : SmFx::EventId
    {
        _noevent_ = 0,
        AdapterCreated = 1,
        Cleanup = 2,
        Destroy = 3,
        GoToNextState = 4,
        PowerDownD1 = 5,
        PowerDownD2 = 6,
        PowerDownD3 = 7,
        PowerDownD3Final = 8,
        PowerUp = 9,
        Start = 10,
        Stop = 11,
        SurpriseRemove = 12,
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
    DevicePnpPowerStateMachine() = default;

    // This class is expected to be a base class, but polymorphic access through this base class is
    // not allowed. Hence, the destructor is protected.
    ~DevicePnpPowerStateMachine();

    // No copy semantices, until we can decide what exactly it means to "copy" a state machine.

    DevicePnpPowerStateMachine(
        const DevicePnpPowerStateMachine&) = delete;

    DevicePnpPowerStateMachine&
    operator=(
        const DevicePnpPowerStateMachine&) = delete;

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
        static SmFx::StateEntryFunction DeviceAddFailedEntry;
        static SmFx::StateEntryFunction PoweringDownD1Entry;
        static SmFx::StateEntryFunction PoweringDownD2Entry;
        static SmFx::StateEntryFunction PoweringDownD3Entry;
        static SmFx::StateEntryFunction PoweringDownD3FinalEntry;
        static SmFx::StateEntryFunction PoweringUpEntry;
        static SmFx::StateEntryFunction RemovedEntry;
        static SmFx::StateEntryFunction RestartingEntry;
        static SmFx::StateEntryFunction SignalD0Entry;
        static SmFx::StateEntryFunction SignalD3FinalEntry;
        static SmFx::StateEntryFunction SignalDxEntry;
        static SmFx::StateEntryFunction SignalStartedEntry;
        static SmFx::StateEntryFunction SignalStoppingD3FinalEntry;
        static SmFx::StateEntryFunction StoppingD3FinalEntry;
        static SmFx::StateEntryFunction SurpriseRemovedSignalD0Entry;
        static SmFx::StateEntryFunction SurpriseRemovedSignalD3FinalEntry;
        static SmFx::StateEntryFunction SurpriseRemovedSignalDxEntry;
        static SmFx::StateEntryFunction SurpriseRemovedSignalStopDxEntry;
        static SmFx::StateEntryFunction SurpriseRemovedStoppingEntry;
        static SmFx::StateEntryFunction SurpriseRemovingInD0Entry;
        static SmFx::StateEntryFunction SurpriseRemovingInDxEntry;
        static SmFx::StateEntryFunction SurpriseRemovingNotStartedEntry;
    };

    // Internal transition actions.
    struct Actions
    {
        static SmFx::InternalTransitionAction CreatedActionOnAdapterCreated;
        static SmFx::InternalTransitionAction D0ActionOnAdapterCreated;
        static SmFx::InternalTransitionAction D3FinalActionOnAdapterCreated;
        static SmFx::InternalTransitionAction DxActionOnAdapterCreated;
        static SmFx::InternalTransitionAction StartedActionOnAdapterCreated;
        static SmFx::InternalTransitionAction StoppedActionOnAdapterCreated;
        static SmFx::InternalTransitionAction SurpriseRemovedInD0ActionOnAdapterCreated;
        static SmFx::InternalTransitionAction SurpriseRemovedInDxActionOnAdapterCreated;
        static SmFx::InternalTransitionAction SurpriseRemovedNotStartedActionOnAdapterCreated;
        static SmFx::InternalTransitionAction SurpriseRemovedStoppedActionOnAdapterCreated;
    };

    // Enumeration of state indices.
    enum class StateIndex : SmFx::StateIndex
    {
        _nostate_ = 0,
        Created = 1,
        D0 = 2,
        D3Final = 3,
        DeviceAddFailed = 4,
        Dx = 5,
        PoweringDownD1 = 6,
        PoweringDownD2 = 7,
        PoweringDownD3 = 8,
        PoweringDownD3Final = 9,
        PoweringUp = 10,
        Removed = 11,
        Restarting = 12,
        SignalD0 = 13,
        SignalD3Final = 14,
        SignalDx = 15,
        SignalStarted = 16,
        SignalStoppingD3Final = 17,
        Started = 18,
        Stopped = 19,
        StoppingD3Final = 20,
        SurpriseRemovedInD0 = 21,
        SurpriseRemovedInDx = 22,
        SurpriseRemovedNotStarted = 23,
        SurpriseRemovedSignalD0 = 24,
        SurpriseRemovedSignalD3Final = 25,
        SurpriseRemovedSignalDx = 26,
        SurpriseRemovedSignalStopDx = 27,
        SurpriseRemovedStopped = 28,
        SurpriseRemovedStopping = 29,
        SurpriseRemovingInD0 = 30,
        SurpriseRemovingInDx = 31,
        SurpriseRemovingNotStarted = 32,
    };

    // Enumeration of submachine names.
    enum class SubmachineName : SmFx::SubmachineIndex
    {
        _nosubmachine_ = 0,
        DevicePnpPower = 1,
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
        static const SmFx::INTERNAL_TRANSITION c_created[];
        static const SmFx::INTERNAL_TRANSITION c_d0[];
        static const SmFx::INTERNAL_TRANSITION c_d3Final[];
        static const SmFx::INTERNAL_TRANSITION c_dx[];
        static const SmFx::INTERNAL_TRANSITION c_started[];
        static const SmFx::INTERNAL_TRANSITION c_stopped[];
        static const SmFx::INTERNAL_TRANSITION c_surpriseRemovedInD0[];
        static const SmFx::INTERNAL_TRANSITION c_surpriseRemovedInDx[];
        static const SmFx::INTERNAL_TRANSITION c_surpriseRemovedNotStarted[];
        static const SmFx::INTERNAL_TRANSITION c_surpriseRemovedStopped[];
    };

    // Declarations of external transitions for each state that has any.
    struct ExternalTransitions
    {
        static const SmFx::EXTERNAL_TRANSITION c_created[];
        static const SmFx::EXTERNAL_TRANSITION c_d0[];
        static const SmFx::EXTERNAL_TRANSITION c_d3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_deviceAddFailed[];
        static const SmFx::EXTERNAL_TRANSITION c_dx[];
        static const SmFx::EXTERNAL_TRANSITION c_poweringDownD1[];
        static const SmFx::EXTERNAL_TRANSITION c_poweringDownD2[];
        static const SmFx::EXTERNAL_TRANSITION c_poweringDownD3[];
        static const SmFx::EXTERNAL_TRANSITION c_poweringDownD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_poweringUp[];
        static const SmFx::EXTERNAL_TRANSITION c_restarting[];
        static const SmFx::EXTERNAL_TRANSITION c_signalD0[];
        static const SmFx::EXTERNAL_TRANSITION c_signalD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_signalDx[];
        static const SmFx::EXTERNAL_TRANSITION c_signalStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_signalStoppingD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_started[];
        static const SmFx::EXTERNAL_TRANSITION c_stopped[];
        static const SmFx::EXTERNAL_TRANSITION c_stoppingD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedInD0[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedInDx[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedNotStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedSignalD0[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedSignalD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedSignalDx[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedSignalStopDx[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedStopped[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovedStopping[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovingInD0[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovingInDx[];
        static const SmFx::EXTERNAL_TRANSITION c_surpriseRemovingNotStarted[];
    };

    // Declarations of deferred events for each state that has any.
    struct DeferredEvents
    {
    };

    // Declarations of purged events for each state that has any.
    struct PurgeEvents
    {
    };

    // Declarations of stop-timer-on-exit details for each state that has it.
    struct StopTimerOnExitDetails
    {
    };

    // Declarations of slot arrays for states that have a non-empty slot array.
    struct SlotArrays
    {
        static const SmFx::StateSlot c_created[];
        static const SmFx::StateSlot c_d0[];
        static const SmFx::StateSlot c_d3Final[];
        static const SmFx::StateSlot c_deviceAddFailed[];
        static const SmFx::StateSlot c_dx[];
        static const SmFx::StateSlot c_poweringDownD1[];
        static const SmFx::StateSlot c_poweringDownD2[];
        static const SmFx::StateSlot c_poweringDownD3[];
        static const SmFx::StateSlot c_poweringDownD3Final[];
        static const SmFx::StateSlot c_poweringUp[];
        static const SmFx::StateSlot c_removed[];
        static const SmFx::StateSlot c_restarting[];
        static const SmFx::StateSlot c_signalD0[];
        static const SmFx::StateSlot c_signalD3Final[];
        static const SmFx::StateSlot c_signalDx[];
        static const SmFx::StateSlot c_signalStarted[];
        static const SmFx::StateSlot c_signalStoppingD3Final[];
        static const SmFx::StateSlot c_started[];
        static const SmFx::StateSlot c_stopped[];
        static const SmFx::StateSlot c_stoppingD3Final[];
        static const SmFx::StateSlot c_surpriseRemovedInD0[];
        static const SmFx::StateSlot c_surpriseRemovedInDx[];
        static const SmFx::StateSlot c_surpriseRemovedNotStarted[];
        static const SmFx::StateSlot c_surpriseRemovedSignalD0[];
        static const SmFx::StateSlot c_surpriseRemovedSignalD3Final[];
        static const SmFx::StateSlot c_surpriseRemovedSignalDx[];
        static const SmFx::StateSlot c_surpriseRemovedSignalStopDx[];
        static const SmFx::StateSlot c_surpriseRemovedStopped[];
        static const SmFx::StateSlot c_surpriseRemovedStopping[];
        static const SmFx::StateSlot c_surpriseRemovingInD0[];
        static const SmFx::StateSlot c_surpriseRemovingInDx[];
        static const SmFx::StateSlot c_surpriseRemovingNotStarted[];
    };

    // Declaration of the complete state machine specification.
    static const SmFx::STATE_MACHINE_SPECIFICATION c_specification;

    // The state machine engine.
    SmFx::StateMachineEngine m_engine;
};

// Engine method implementations.

template<typename T>
NTSTATUS
DevicePnpPowerStateMachine<T>::Initialize(
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
    engineConfig.isWorkerRequired = false;
#else // #ifdef _KERNEL_MODE
    engineConfig.isWorkerRequired = false;
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
DevicePnpPowerStateMachine<T>::IsInitialized()
{
    return m_engine.IsInitialized();
}

template<typename T>
DevicePnpPowerStateMachine<T>::~DevicePnpPowerStateMachine()
{
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::EnqueueEvent(
    typename DevicePnpPowerStateMachine<T>::Event event)
{
    m_engine.EnqueueEvent(static_cast<SmFx::EventIndex>(event));
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::EvtLogMachineExceptionThunk(
    void* context,
    SmFx::MachineException exception,
    SmFx::EventId relevantEvent,
    SmFx::StateId relevantState)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->EvtLogMachineException(
        exception,
        static_cast<EventId>(relevantEvent),
        static_cast<StateId>(relevantState));
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::EvtLogEventEnqueueThunk(
    void* context,
    SmFx::EventId relevantEvent)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->EvtLogEventEnqueue(static_cast<EventId>(relevantEvent));
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::EvtLogTransitionThunk(
    void* context,
    SmFx::TransitionType transitionType,
    SmFx::StateId sourceState,
    SmFx::EventId processedEvent,
    SmFx::StateId targetState)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->EvtLogTransition(
        transitionType,
        static_cast<StateId>(sourceState),
        static_cast<EventId>(processedEvent),
        static_cast<StateId>(targetState));
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::EvtMachineDestroyedThunk(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->EvtMachineDestroyed();
}

// Definitions for each of the state entry functions.

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::DeviceAddFailedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->Stopping();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringDownD1Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->PoweringDownD1();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringDownD2Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->PoweringDownD2();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringDownD3Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->PoweringDownD3();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringDownD3FinalEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->PoweringDownD3Final();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringUpEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->PoweringUp();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::RemovedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->Removed();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::RestartingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->Restarting();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SignalD0Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SignalD3FinalEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SignalDxEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SignalStartedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SignalStoppingD3FinalEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::StoppingD3FinalEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->Stopping();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedSignalD0Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedSignalD3FinalEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedSignalDxEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedSignalStopDxEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->WdfEventProcessed();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedStoppingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->Stopping();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovingInD0Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->SurpriseRemoving();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovingInDxEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->SurpriseRemoving();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovingNotStartedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->Stopping();

    return static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState);
}

// Definitions for each of the internal transition actions.

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::CreatedActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedNoEventsToPlay();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::D0ActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedPlayPowerUp();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::D3FinalActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedNoEventsToPlay();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::DxActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedNoEventsToPlay();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::StartedActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedNoEventsToPlay();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::StoppedActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedNoEventsToPlay();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::SurpriseRemovedInD0ActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedPlaySurpriseRemove();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::SurpriseRemovedInDxActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedPlaySurpriseRemove();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::SurpriseRemovedNotStartedActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedPlaySurpriseRemove();
}

template<typename T>
void
DevicePnpPowerStateMachine<T>::Actions::SurpriseRemovedStoppedActionOnAdapterCreated(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<DevicePnpPowerStateMachine<T>*>(context));

    derived->AdapterCreatedPlaySurpriseRemove();
}

// Pop transitions.

template <typename T>
const SmFx::POP_TRANSITION DevicePnpPowerStateMachine<T>::PopTransitions::c_removed[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Destroy),
        // Return event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Destroy)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

// Internal transitions.

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_created[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::CreatedActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_d0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::D0ActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_d3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::D3FinalActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_dx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::DxActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_started[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::StartedActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_stopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::StoppedActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_surpriseRemovedInD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::SurpriseRemovedInD0ActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_surpriseRemovedInDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::SurpriseRemovedInDxActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_surpriseRemovedNotStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::SurpriseRemovedNotStartedActionOnAdapterCreated
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::InternalTransitions::c_surpriseRemovedStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::AdapterCreated),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &DevicePnpPowerStateMachine<T>::Actions::SurpriseRemovedStoppedActionOnAdapterCreated
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
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_created[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Cleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::DeviceAddFailed)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Start),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SignalStarted)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovingNotStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_d0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerDownD1),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::PoweringDownD1)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerDownD2),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::PoweringDownD2)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerDownD3),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::PoweringDownD3)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerDownD3Final),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::PoweringDownD3Final)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovingInD0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_d3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Stop),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::StoppingD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_deviceAddFailed[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Removed)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_dx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerUp),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::PoweringUp)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovingInDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringDownD1[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SignalDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringDownD2[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SignalDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringDownD3[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SignalDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringDownD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SignalD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringUp[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SignalD0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_restarting[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SignalStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::D0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::D3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Dx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Started)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalStoppingD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Stopped)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_started[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerUp),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::PoweringUp)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Stop),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::StoppingD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_stopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Cleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Removed)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Start),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Restarting)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_stoppingD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SignalStoppingD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedInD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerDownD1),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedSignalDx)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerDownD2),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedSignalDx)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerDownD3),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedSignalDx)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerDownD3Final),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedSignalD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedInDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::PowerUp),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedSignalD0)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Stop),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedStopping)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedNotStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Cleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Removed)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedSignalD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedInD0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedSignalD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedInDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedSignalDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedInDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedSignalStopDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedStopped)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::Cleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Removed)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedStopping[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedSignalStopDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovingInD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedInD0)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovingInDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedInDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovingNotStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::SurpriseRemovedNotStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

// Deferred events.

// Purged events.

// Stop-timer-on-exit details.

// Event table.
template<typename T>
const SmFx::EVENT_SPECIFICATION DevicePnpPowerStateMachine<T>::c_eventTable[] =
{
    {
        // Unused.
        static_cast<SmFx::EventId>(0),
        SmFx::EventQueueingDisposition::Invalid
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::AdapterCreated),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::Cleanup),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::Destroy),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::GoToNextState),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::PowerDownD1),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::PowerDownD2),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::PowerDownD3),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::PowerDownD3Final),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::PowerUp),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::Start),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::Stop),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(DevicePnpPowerStateMachine<T>::EventId::SurpriseRemove),
        SmFx::EventQueueingDisposition::Unlimited
    },
};

// Submachine table.
template<typename T>
const SmFx::SUBMACHINE_SPECIFICATION DevicePnpPowerStateMachine<T>::c_machineTable[] =
{
    {},
    // Submachine: DevicePnpPower
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(DevicePnpPowerStateMachine<T>::StateIndex::Created)
    },
};

// Slot arrays.

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_created[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_created,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_created,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_d0[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_d0,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_d0,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_d3Final[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_d3Final,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_d3Final,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_deviceAddFailed[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::DeviceAddFailedEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_deviceAddFailed,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_dx[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_dx,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_dx,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringDownD1[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringDownD1Entry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringDownD1,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringDownD2[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringDownD2Entry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringDownD2,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringDownD3[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringDownD3Entry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringDownD3,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringDownD3Final[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringDownD3FinalEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringDownD3Final,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringUp[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::PoweringUpEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_poweringUp,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_removed[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::RemovedEntry),
    DevicePnpPowerStateMachine<T>::PopTransitions::c_removed,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_restarting[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::RestartingEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_restarting,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_signalD0[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SignalD0Entry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalD0,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_signalD3Final[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SignalD3FinalEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalD3Final,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_signalDx[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SignalDxEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalDx,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_signalStarted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SignalStartedEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalStarted,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_signalStoppingD3Final[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SignalStoppingD3FinalEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_signalStoppingD3Final,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_started[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_started,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_started,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_stopped[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_stopped,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_stopped,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_stoppingD3Final[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::StoppingD3FinalEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_stoppingD3Final,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedInD0[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedInD0,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_surpriseRemovedInD0,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedInDx[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedInDx,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_surpriseRemovedInDx,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedNotStarted[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedNotStarted,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_surpriseRemovedNotStarted,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedSignalD0[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedSignalD0Entry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedSignalD0,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedSignalD3Final[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedSignalD3FinalEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedSignalD3Final,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedSignalDx[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedSignalDxEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedSignalDx,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedSignalStopDx[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedSignalStopDxEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedSignalStopDx,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedStopped[] =
{
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedStopped,
    DevicePnpPowerStateMachine<T>::InternalTransitions::c_surpriseRemovedStopped,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedStopping[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovedStoppingEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovedStopping,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovingInD0[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovingInD0Entry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovingInD0,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovingInDx[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovingInDxEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovingInDx,
};

template <typename T>
const SmFx::StateSlot DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovingNotStarted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&DevicePnpPowerStateMachine<T>::EntryFuncs::SurpriseRemovingNotStartedEntry),
    DevicePnpPowerStateMachine<T>::ExternalTransitions::c_surpriseRemovingNotStarted,
};

// State table.
template<typename T>
const SmFx::STATE_SPECIFICATION DevicePnpPowerStateMachine<T>::c_stateTable[] =
{
    {},
    // State: Created
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::Created),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_created,
    },
    // State: D0
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::D0),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_d0,
    },
    // State: D3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::D3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_d3Final,
    },
    // State: DeviceAddFailed
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::DeviceAddFailed),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_deviceAddFailed,
    },
    // State: Dx
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::Dx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_dx,
    },
    // State: PoweringDownD1
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::PoweringDownD1),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringDownD1,
    },
    // State: PoweringDownD2
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::PoweringDownD2),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringDownD2,
    },
    // State: PoweringDownD3
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::PoweringDownD3),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringDownD3,
    },
    // State: PoweringDownD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::PoweringDownD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringDownD3Final,
    },
    // State: PoweringUp
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::PoweringUp),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_poweringUp,
    },
    // State: Removed
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::Removed),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_removed,
    },
    // State: Restarting
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::Restarting),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_restarting,
    },
    // State: SignalD0
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SignalD0),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_signalD0,
    },
    // State: SignalD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SignalD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_signalD3Final,
    },
    // State: SignalDx
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SignalDx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_signalDx,
    },
    // State: SignalStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SignalStarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_signalStarted,
    },
    // State: SignalStoppingD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SignalStoppingD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_signalStoppingD3Final,
    },
    // State: Started
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::Started),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_started,
    },
    // State: Stopped
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::Stopped),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_stopped,
    },
    // State: StoppingD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::StoppingD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_stoppingD3Final,
    },
    // State: SurpriseRemovedInD0
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedInD0),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedInD0,
    },
    // State: SurpriseRemovedInDx
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedInDx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedInDx,
    },
    // State: SurpriseRemovedNotStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedNotStarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedNotStarted,
    },
    // State: SurpriseRemovedSignalD0
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedSignalD0),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedSignalD0,
    },
    // State: SurpriseRemovedSignalD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedSignalD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedSignalD3Final,
    },
    // State: SurpriseRemovedSignalDx
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedSignalDx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedSignalDx,
    },
    // State: SurpriseRemovedSignalStopDx
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedSignalStopDx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedSignalStopDx,
    },
    // State: SurpriseRemovedStopped
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedStopped),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedStopped,
    },
    // State: SurpriseRemovedStopping
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovedStopping),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovedStopping,
    },
    // State: SurpriseRemovingInD0
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovingInD0),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovingInD0,
    },
    // State: SurpriseRemovingInDx
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovingInDx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovingInDx,
    },
    // State: SurpriseRemovingNotStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(DevicePnpPowerStateMachine<T>::StateId::SurpriseRemovingNotStarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        DevicePnpPowerStateMachine<T>::SlotArrays::c_surpriseRemovingNotStarted,
    },
};

// Machine specification.
template<typename T>
const SmFx::STATE_MACHINE_SPECIFICATION DevicePnpPowerStateMachine<T>::c_specification =
{
    static_cast<SmFx::SubmachineIndex>(DevicePnpPowerStateMachine<T>::SubmachineName::DevicePnpPower),
    static_cast<SmFx::EventIndex>(DevicePnpPowerStateMachine<T>::Event::GoToNextState),
    DevicePnpPowerStateMachine<T>::c_machineTable,
    DevicePnpPowerStateMachine<T>::c_eventTable,
    DevicePnpPowerStateMachine<T>::c_stateTable
};
