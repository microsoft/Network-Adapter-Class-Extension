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
        D0EntryPostHardwareEnabled = 1,
        D0ExitPreHardwareDisabledD3Final = 2,
        D0ExitPreHardwareDisabledDx = 3,
        No = 4,
        RequestPlatformLevelDeviceReset = 5,
        SyncFail = 6,
        SyncSuccess = 7,
        WdfDeviceObjectCleanup = 8,
        Yes = 9,
    };

    // Enumeration of state IDs.
    enum class StateId : SmFx::StateId
    {
        _nostate_ = 0,
        CollectingDiagnostics = 1,
        D0 = 2,
        D3Final = 3,
        Dx = 4,
        Initialized = 5,
        PnPStarted = 6,
        TriggeringPlatformLevelDeviceReset = 7,
    };

    // Enumeration of event IDs.
    enum class EventId : SmFx::EventId
    {
        _noevent_ = 0,
        D0EntryPostHardwareEnabled = 1,
        D0ExitPreHardwareDisabledD3Final = 2,
        D0ExitPreHardwareDisabledDx = 3,
        No = 4,
        RequestPlatformLevelDeviceReset = 5,
        SyncFail = 6,
        SyncSuccess = 7,
        WdfDeviceObjectCleanup = 8,
        Yes = 9,
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
        static SmFx::StateEntryFunction CollectingDiagnosticsEntry;
        static SmFx::StateEntryFunction TriggeringPlatformLevelDeviceResetEntry;
    };

    // Internal transition actions.
    struct Actions
    {
    };

    // Enumeration of state indices.
    enum class StateIndex : SmFx::StateIndex
    {
        _nostate_ = 0,
        CollectingDiagnostics = 1,
        D0 = 2,
        D3Final = 3,
        Dx = 4,
        Initialized = 5,
        PnPStarted = 6,
        TriggeringPlatformLevelDeviceReset = 7,
    };

    // Enumeration of submachine names.
    enum class SubmachineName : SmFx::SubmachineIndex
    {
        _nosubmachine_ = 0,
        PowerStateMachine = 1,
        StateMachine = 2,
    };

    // Table of submachine specifications.
    static const SmFx::SUBMACHINE_SPECIFICATION c_machineTable[];

    // Declarations of pop transitions for each state that has any.
    struct PopTransitions
    {
        static const SmFx::POP_TRANSITION c_d0[];
        static const SmFx::POP_TRANSITION c_d3Final[];
        static const SmFx::POP_TRANSITION c_initialized[];
        static const SmFx::POP_TRANSITION c_pnPStarted[];
        static const SmFx::POP_TRANSITION c_triggeringPlatformLevelDeviceReset[];
    };

    // Declarations of internal transitions for each state that has any.
    struct InternalTransitions
    {
        static const SmFx::INTERNAL_TRANSITION c_d3Final[];
        static const SmFx::INTERNAL_TRANSITION c_triggeringPlatformLevelDeviceReset[];
    };

    // Declarations of external transitions for each state that has any.
    struct ExternalTransitions
    {
        static const SmFx::EXTERNAL_TRANSITION c_collectingDiagnostics[];
        static const SmFx::EXTERNAL_TRANSITION c_d0[];
        static const SmFx::EXTERNAL_TRANSITION c_d3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_dx[];
        static const SmFx::EXTERNAL_TRANSITION c_initialized[];
        static const SmFx::EXTERNAL_TRANSITION c_pnPStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_triggeringPlatformLevelDeviceReset[];
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
        static const SmFx::StateSlot c_collectingDiagnostics[];
        static const SmFx::StateSlot c_d0[];
        static const SmFx::StateSlot c_d3Final[];
        static const SmFx::StateSlot c_dx[];
        static const SmFx::StateSlot c_initialized[];
        static const SmFx::StateSlot c_pnPStarted[];
        static const SmFx::StateSlot c_triggeringPlatformLevelDeviceReset[];
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
    engineConfig.stackSize = 2;

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
NxDeviceStateMachine<T>::EntryFuncs::CollectingDiagnosticsEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    NxDeviceStateMachine<T>::Event returnEvent = derived->CollectDiagnostics();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
NxDeviceStateMachine<T>::EntryFuncs::TriggeringPlatformLevelDeviceResetEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<NxDeviceStateMachine<T>*>(context));

    derived->TriggerPlatformLevelDeviceReset();

    return static_cast<SmFx::EventIndex>(0);
}

// Definitions for each of the internal transition actions.

// Pop transitions.

template <typename T>
const SmFx::POP_TRANSITION NxDeviceStateMachine<T>::PopTransitions::c_d0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0ExitPreHardwareDisabledD3Final),
        // Return event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0ExitPreHardwareDisabledD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

template <typename T>
const SmFx::POP_TRANSITION NxDeviceStateMachine<T>::PopTransitions::c_d3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup),
        // Return event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

template <typename T>
const SmFx::POP_TRANSITION NxDeviceStateMachine<T>::PopTransitions::c_initialized[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup),
        // Return event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

template <typename T>
const SmFx::POP_TRANSITION NxDeviceStateMachine<T>::PopTransitions::c_pnPStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup),
        // Return event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

template <typename T>
const SmFx::POP_TRANSITION NxDeviceStateMachine<T>::PopTransitions::c_triggeringPlatformLevelDeviceReset[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup),
        // Return event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::WdfDeviceObjectCleanup)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

// Internal transitions.

template <typename T>
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_d3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RequestPlatformLevelDeviceReset),
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
const SmFx::INTERNAL_TRANSITION NxDeviceStateMachine<T>::InternalTransitions::c_triggeringPlatformLevelDeviceReset[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0EntryPostHardwareEnabled),
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
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_collectingDiagnostics[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::SyncSuccess),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::TriggeringPlatformLevelDeviceReset)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_d0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0ExitPreHardwareDisabledDx),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::Dx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_d3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0EntryPostHardwareEnabled),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::PnPStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_dx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0EntryPostHardwareEnabled),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::D0)
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
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0EntryPostHardwareEnabled),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::PnPStarted)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RequestPlatformLevelDeviceReset),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::CollectingDiagnostics)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_pnPStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0ExitPreHardwareDisabledD3Final),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::D3Final)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::RequestPlatformLevelDeviceReset),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::CollectingDiagnostics)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION NxDeviceStateMachine<T>::ExternalTransitions::c_triggeringPlatformLevelDeviceReset[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(NxDeviceStateMachine<T>::Event::D0ExitPreHardwareDisabledD3Final),
        // Target state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::D3Final)
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
const SmFx::EVENT_SPECIFICATION NxDeviceStateMachine<T>::c_eventTable[] =
{
    {
        // Unused.
        static_cast<SmFx::EventId>(0),
        SmFx::EventQueueingDisposition::Invalid
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::D0EntryPostHardwareEnabled),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::D0ExitPreHardwareDisabledD3Final),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::D0ExitPreHardwareDisabledDx),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::No),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(NxDeviceStateMachine<T>::EventId::RequestPlatformLevelDeviceReset),
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
    // Submachine: PowerStateMachine
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::D0)
    },
    // Submachine: StateMachine
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(NxDeviceStateMachine<T>::StateIndex::Initialized)
    },
};

// Slot arrays.

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_collectingDiagnostics[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::CollectingDiagnosticsEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_collectingDiagnostics,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_d0[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_d0,
    NxDeviceStateMachine<T>::PopTransitions::c_d0,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_d3Final[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_d3Final,
    NxDeviceStateMachine<T>::InternalTransitions::c_d3Final,
    NxDeviceStateMachine<T>::PopTransitions::c_d3Final,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_dx[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_dx,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_initialized[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_initialized,
    NxDeviceStateMachine<T>::PopTransitions::c_initialized,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_pnPStarted[] =
{
    NxDeviceStateMachine<T>::ExternalTransitions::c_pnPStarted,
    NxDeviceStateMachine<T>::PopTransitions::c_pnPStarted,
};

template <typename T>
const SmFx::StateSlot NxDeviceStateMachine<T>::SlotArrays::c_triggeringPlatformLevelDeviceReset[] =
{
    reinterpret_cast<SmFx::StateSlot>(&NxDeviceStateMachine<T>::EntryFuncs::TriggeringPlatformLevelDeviceResetEntry),
    NxDeviceStateMachine<T>::ExternalTransitions::c_triggeringPlatformLevelDeviceReset,
    NxDeviceStateMachine<T>::InternalTransitions::c_triggeringPlatformLevelDeviceReset,
    NxDeviceStateMachine<T>::PopTransitions::c_triggeringPlatformLevelDeviceReset,
};

// State table.
template<typename T>
const SmFx::STATE_SPECIFICATION NxDeviceStateMachine<T>::c_stateTable[] =
{
    {},
    // State: CollectingDiagnostics
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::CollectingDiagnostics),
        // State flags.
        SmFx::StateFlags::RequiresDedicatedThread,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_collectingDiagnostics,
    },
    // State: D0
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::D0),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_d0,
    },
    // State: D3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::D3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_d3Final,
    },
    // State: Dx
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::Dx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_dx,
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
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_initialized,
    },
    // State: PnPStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::PnPStarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::PowerStateMachine),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_pnPStarted,
    },
    // State: TriggeringPlatformLevelDeviceReset
    {
        // State ID.
        static_cast<SmFx::StateId>(NxDeviceStateMachine<T>::StateId::TriggeringPlatformLevelDeviceReset),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(NxDeviceStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        NxDeviceStateMachine<T>::SlotArrays::c_triggeringPlatformLevelDeviceReset,
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
