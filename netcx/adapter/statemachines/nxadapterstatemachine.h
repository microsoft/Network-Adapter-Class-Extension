/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    NxAdapterStateMachine.h

Abstract:

    The NxAdapter state machine template base class.

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
class NxAdapterStateMachine
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
        DatapathCreate = 1,
        DatapathDestroy = 2,
        EvtCleanup = 3,
        NdisMiniportHalt = 4,
        NdisMiniportInitializeEx = 5,
        NdisMiniportPause = 6,
        NdisMiniportRestart = 7,
        SyncSuccess = 8,
    };

    // Enumeration of state IDs.
    enum class StateId : SmFx::StateId
    {
        _nostate_ = 0,
        AdapterPauseComplete = 1,
        AdapterPaused = 2,
        AdapterRestartComplete = 3,
        AdapterRestarted = 4,
        DatapathPaused = 5,
        DatapathPausedCreating = 6,
        DatapathPausedDestroying = 7,
        DatapathPausedStarting = 8,
        DatapathPauseStopping = 9,
        DatapathRestartDestroying = 10,
        DatapathRestartedCreating = 11,
        DatapathRestartedStarting = 12,
        DatapathRestartStopping = 13,
        DatapathRunning = 14,
        Halted = 15,
        Halting = 16,
    };

    // Enumeration of event IDs.
    enum class EventId : SmFx::EventId
    {
        _noevent_ = 0,
        DatapathCreate = 1,
        DatapathDestroy = 2,
        EvtCleanup = 3,
        NdisMiniportHalt = 4,
        NdisMiniportInitializeEx = 5,
        NdisMiniportPause = 6,
        NdisMiniportRestart = 7,
        SyncSuccess = 8,
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
    NxAdapterStateMachine() = default;

    // This class is expected to be a base class, but polymorphic access through this base class is
    // not allowed. Hence, the destructor is protected.
    ~NxAdapterStateMachine();

    // No copy semantices, until we can decide what exactly it means to "copy" a state machine.

    NxAdapterStateMachine(
        const NxAdapterStateMachine&) = delete;

    NxAdapterStateMachine&
    operator=(
        const NxAdapterStateMachine&) = delete;

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
        static SmFx::StateEntryFunction AdapterPauseCompleteEntry;
        static SmFx::StateEntryFunction AdapterPausedEntry;
        static SmFx::StateEntryFunction AdapterRestartCompleteEntry;
        static SmFx::StateEntryFunction AdapterRestartedEntry;
        static SmFx::StateEntryFunction DatapathPausedEntry;
        static SmFx::StateEntryFunction DatapathPausedCreatingEntry;
        static SmFx::StateEntryFunction DatapathPausedDestroyingEntry;
        static SmFx::StateEntryFunction DatapathPausedStartingEntry;
        static SmFx::StateEntryFunction DatapathPauseStoppingEntry;
        static SmFx::StateEntryFunction DatapathRestartDestroyingEntry;
        static SmFx::StateEntryFunction DatapathRestartedCreatingEntry;
        static SmFx::StateEntryFunction DatapathRestartedStartingEntry;
        static SmFx::StateEntryFunction DatapathRestartStoppingEntry;
        static SmFx::StateEntryFunction DatapathRunningEntry;
        static SmFx::StateEntryFunction HaltedEntry;
        static SmFx::StateEntryFunction HaltingEntry;
    };

    // Internal transition actions.
    struct Actions
    {
    };

    // Enumeration of state indices.
    enum class StateIndex : SmFx::StateIndex
    {
        _nostate_ = 0,
        AdapterPauseComplete = 1,
        AdapterPaused = 2,
        AdapterRestartComplete = 3,
        AdapterRestarted = 4,
        DatapathPaused = 5,
        DatapathPausedCreating = 6,
        DatapathPausedDestroying = 7,
        DatapathPausedStarting = 8,
        DatapathPauseStopping = 9,
        DatapathRestartDestroying = 10,
        DatapathRestartedCreating = 11,
        DatapathRestartedStarting = 12,
        DatapathRestartStopping = 13,
        DatapathRunning = 14,
        Halted = 15,
        Halting = 16,
    };

    // Enumeration of submachine names.
    enum class SubmachineName : SmFx::SubmachineIndex
    {
        _nosubmachine_ = 0,
        NxAdapter = 1,
    };

    // Table of submachine specifications.
    static const SmFx::SUBMACHINE_SPECIFICATION c_machineTable[];

    // Declarations of pop transitions for each state that has any.
    struct PopTransitions
    {
        static const SmFx::POP_TRANSITION c_halted[];
    };

    // Declarations of internal transitions for each state that has any.
    struct InternalTransitions
    {
    };

    // Declarations of external transitions for each state that has any.
    struct ExternalTransitions
    {
        static const SmFx::EXTERNAL_TRANSITION c_adapterPauseComplete[];
        static const SmFx::EXTERNAL_TRANSITION c_adapterPaused[];
        static const SmFx::EXTERNAL_TRANSITION c_adapterRestartComplete[];
        static const SmFx::EXTERNAL_TRANSITION c_adapterRestarted[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathPaused[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathPausedCreating[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathPausedDestroying[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathPausedStarting[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathPauseStopping[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathRestartDestroying[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathRestartedCreating[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathRestartedStarting[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathRestartStopping[];
        static const SmFx::EXTERNAL_TRANSITION c_datapathRunning[];
        static const SmFx::EXTERNAL_TRANSITION c_halted[];
        static const SmFx::EXTERNAL_TRANSITION c_halting[];
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
        static const SmFx::StateSlot c_adapterPauseComplete[];
        static const SmFx::StateSlot c_adapterPaused[];
        static const SmFx::StateSlot c_adapterRestartComplete[];
        static const SmFx::StateSlot c_adapterRestarted[];
        static const SmFx::StateSlot c_datapathPaused[];
        static const SmFx::StateSlot c_datapathPausedCreating[];
        static const SmFx::StateSlot c_datapathPausedDestroying[];
        static const SmFx::StateSlot c_datapathPausedStarting[];
        static const SmFx::StateSlot c_datapathPauseStopping[];
        static const SmFx::StateSlot c_datapathRestartDestroying[];
        static const SmFx::StateSlot c_datapathRestartedCreating[];
        static const SmFx::StateSlot c_datapathRestartedStarting[];
        static const SmFx::StateSlot c_datapathRestartStopping[];
        static const SmFx::StateSlot c_datapathRunning[];
        static const SmFx::StateSlot c_halted[];
        static const SmFx::StateSlot c_halting[];
    };

    // Declaration of the complete state machine specification.
    static const SmFx::STATE_MACHINE_SPECIFICATION c_specification;

    // The state machine engine.
    SmFx::StateMachineEngine m_engine;
};

// Engine method implementations.

template<typename T>
NTSTATUS
NxAdapterStateMachine<T>::Initialize(
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
NxAdapterStateMachine<T>::IsInitialized()
{
    return m_engine.IsInitialized();
}

template<typename T>
NxAdapterStateMachine<T>::~NxAdapterStateMachine()
{
}

template<typename T>
void
NxAdapterStateMachine<T>::EnqueueEvent(
    typename NxAdapterStateMachine<T>::Event event)
{
    m_engine.EnqueueEvent(static_cast<SmFx::EventIndex>(event));
}

template<typename T>
void
NxAdapterStateMachine<T>::EvtLogMachineExceptionThunk(
    void* context,
    SmFx::MachineException exception,
    SmFx::EventId relevantEvent,
    SmFx::StateId relevantState)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->EvtLogMachineException(
        exception,
        static_cast<EventId>(relevantEvent),
        static_cast<StateId>(relevantState));
}

template<typename T>
void
NxAdapterStateMachine<T>::EvtLogEventEnqueueThunk(
    void* context,
    SmFx::EventId relevantEvent)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->EvtLogEventEnqueue(static_cast<EventId>(relevantEvent));
}

template<typename T>
void
NxAdapterStateMachine<T>::EvtLogTransitionThunk(
    void* context,
    SmFx::TransitionType transitionType,
    SmFx::StateId sourceState,
    SmFx::EventId processedEvent,
    SmFx::StateId targetState)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->EvtLogTransition(
        transitionType,
        static_cast<StateId>(sourceState),
        static_cast<EventId>(processedEvent),
        static_cast<StateId>(targetState));
}

template<typename T>
void
NxAdapterStateMachine<T>::EvtMachineDestroyedThunk(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->EvtMachineDestroyed();
}

// Definitions for each of the state entry functions.

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::AdapterPauseCompleteEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    NxAdapterStateMachine<T>::Event returnEvent = derived->DatapathPauseComplete();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::AdapterPausedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->AsyncTransitionComplete();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::AdapterRestartCompleteEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    NxAdapterStateMachine<T>::Event returnEvent = derived->DatapathRestartComplete();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::AdapterRestartedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->AsyncTransitionComplete();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathPausedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->AsyncTransitionComplete();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathPausedCreatingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    NxAdapterStateMachine<T>::Event returnEvent = derived->DatapathCreate();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathPausedDestroyingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->DatapathDestroy();

    return static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathPausedStartingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    NxAdapterStateMachine<T>::Event returnEvent = derived->DatapathStartRestartComplete();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathPauseStoppingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    NxAdapterStateMachine<T>::Event returnEvent = derived->DatapathStopPauseComplete();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathRestartDestroyingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->DatapathDestroy();

    return static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathRestartedCreatingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    NxAdapterStateMachine<T>::Event returnEvent = derived->DatapathCreate();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathRestartedStartingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    NxAdapterStateMachine<T>::Event returnEvent = derived->DatapathStart();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathRestartStoppingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->DatapathStop();

    return static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::DatapathRunningEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->AsyncTransitionComplete();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::HaltedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->AsyncTransitionComplete();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
NxAdapterStateMachine<T>::EntryFuncs::HaltingEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxAdapterStateMachine<T>*>(context));

    derived->NdisHalt();

    return static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess);
}

// Definitions for each of the internal transition actions.

// Pop transitions.

template <typename T>
const SmFx::POP_TRANSITION NxAdapterStateMachine<T>::PopTransitions::c_halted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::EvtCleanup),
        // Return event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::EvtCleanup)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

// Internal transitions.

// External transitions.

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_adapterPauseComplete[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::AdapterPaused)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_adapterPaused[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::DatapathCreate),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathPausedCreating)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::NdisMiniportHalt),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::Halting)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::NdisMiniportRestart),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::AdapterRestartComplete)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_adapterRestartComplete[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::AdapterRestarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_adapterRestarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::DatapathCreate),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathRestartedCreating)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::NdisMiniportPause),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::AdapterPauseComplete)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPaused[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::DatapathDestroy),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathPausedDestroying)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::NdisMiniportRestart),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathPausedStarting)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPausedCreating[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathPaused)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPausedDestroying[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::AdapterPaused)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPausedStarting[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathRunning)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPauseStopping[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathPaused)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRestartDestroying[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::AdapterRestarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRestartedCreating[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathRestartedStarting)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRestartedStarting[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathRunning)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRestartStopping[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathRestartDestroying)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRunning[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::DatapathDestroy),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathRestartStopping)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::NdisMiniportPause),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::DatapathPauseStopping)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_halted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::NdisMiniportInitializeEx),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::AdapterPaused)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxAdapterStateMachine<T>::ExternalTransitions::c_halting[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::Halted)
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
const SmFx::EVENT_SPECIFICATION NxAdapterStateMachine<T>::c_eventTable[] =
{
    {
        // Unused.
        static_cast<SmFx::EventId>(0),
        SmFx::EventQueueingDisposition::Invalid
    },
    {
        static_cast<SmFx::EventId>(NxAdapterStateMachine<T>::EventId::DatapathCreate),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxAdapterStateMachine<T>::EventId::DatapathDestroy),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxAdapterStateMachine<T>::EventId::EvtCleanup),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxAdapterStateMachine<T>::EventId::NdisMiniportHalt),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxAdapterStateMachine<T>::EventId::NdisMiniportInitializeEx),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxAdapterStateMachine<T>::EventId::NdisMiniportPause),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxAdapterStateMachine<T>::EventId::NdisMiniportRestart),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxAdapterStateMachine<T>::EventId::SyncSuccess),
        SmFx::EventQueueingDisposition::Unlimited
    },
};

// Submachine table.
template<typename T>
const SmFx::SUBMACHINE_SPECIFICATION NxAdapterStateMachine<T>::c_machineTable[] =
{
    {},
    // Submachine: NxAdapter
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(NxAdapterStateMachine<T>::StateIndex::Halted)
    },
};

// Slot arrays.

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_adapterPauseComplete[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::AdapterPauseCompleteEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_adapterPauseComplete,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_adapterPaused[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::AdapterPausedEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_adapterPaused,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_adapterRestartComplete[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::AdapterRestartCompleteEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_adapterRestartComplete,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_adapterRestarted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::AdapterRestartedEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_adapterRestarted,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathPaused[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathPausedEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPaused,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathPausedCreating[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathPausedCreatingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPausedCreating,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathPausedDestroying[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathPausedDestroyingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPausedDestroying,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathPausedStarting[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathPausedStartingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPausedStarting,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathPauseStopping[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathPauseStoppingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathPauseStopping,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathRestartDestroying[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathRestartDestroyingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRestartDestroying,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathRestartedCreating[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathRestartedCreatingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRestartedCreating,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathRestartedStarting[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathRestartedStartingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRestartedStarting,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathRestartStopping[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathRestartStoppingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRestartStopping,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_datapathRunning[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::DatapathRunningEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_datapathRunning,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_halted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::HaltedEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_halted,
    NxAdapterStateMachine<T>::PopTransitions::c_halted,
};

template <typename T>
const SmFx::StateSlot NxAdapterStateMachine<T>::SlotArrays::c_halting[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxAdapterStateMachine<T>::EntryFuncs::HaltingEntry),
    NxAdapterStateMachine<T>::ExternalTransitions::c_halting,
};

// State table.
template<typename T>
const SmFx::STATE_SPECIFICATION NxAdapterStateMachine<T>::c_stateTable[] =
{
    {},
    // State: AdapterPauseComplete
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::AdapterPauseComplete),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_adapterPauseComplete,
    },
    // State: AdapterPaused
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::AdapterPaused),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_adapterPaused,
    },
    // State: AdapterRestartComplete
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::AdapterRestartComplete),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_adapterRestartComplete,
    },
    // State: AdapterRestarted
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::AdapterRestarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_adapterRestarted,
    },
    // State: DatapathPaused
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathPaused),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathPaused,
    },
    // State: DatapathPausedCreating
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathPausedCreating),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathPausedCreating,
    },
    // State: DatapathPausedDestroying
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathPausedDestroying),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathPausedDestroying,
    },
    // State: DatapathPausedStarting
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathPausedStarting),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathPausedStarting,
    },
    // State: DatapathPauseStopping
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathPauseStopping),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathPauseStopping,
    },
    // State: DatapathRestartDestroying
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathRestartDestroying),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathRestartDestroying,
    },
    // State: DatapathRestartedCreating
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathRestartedCreating),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathRestartedCreating,
    },
    // State: DatapathRestartedStarting
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathRestartedStarting),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathRestartedStarting,
    },
    // State: DatapathRestartStopping
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathRestartStopping),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathRestartStopping,
    },
    // State: DatapathRunning
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::DatapathRunning),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_datapathRunning,
    },
    // State: Halted
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::Halted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_halted,
    },
    // State: Halting
    {
        // State ID.
        static_cast<SmFx::StateId>(NxAdapterStateMachine<T>::StateId::Halting),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxAdapterStateMachine<T>::SlotArrays::c_halting,
    },
};

// Machine specification.
template<typename T>
const SmFx::STATE_MACHINE_SPECIFICATION NxAdapterStateMachine<T>::c_specification =
{
    static_cast<SmFx::SubmachineIndex>(NxAdapterStateMachine<T>::SubmachineName::NxAdapter),
    static_cast<SmFx::EventIndex>(NxAdapterStateMachine<T>::Event::SyncSuccess),
    NxAdapterStateMachine<T>::c_machineTable,
    NxAdapterStateMachine<T>::c_eventTable,
    NxAdapterStateMachine<T>::c_stateTable
};
