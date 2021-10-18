/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    AdapterPnpPowerStateMachine.h

Abstract:

    The AdapterPnpPower state machine template base class.

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
class AdapterPnpPowerStateMachine
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
        Cleanup = 1,
        Destroy = 2,
        DeviceRestart = 3,
        Fail = 4,
        GoToNextState = 5,
        IoInterfaceArrival = 6,
        IoStart = 7,
        IoStopD1 = 8,
        IoStopD2 = 9,
        IoStopD3 = 10,
        IoStopD3Final = 11,
        NetAdapterStart = 12,
        NetAdapterStop = 13,
        No = 14,
        Success = 15,
        SurpriseRemove = 16,
        Yes = 17,
    };

    // Enumeration of state IDs.
    enum class StateId : SmFx::StateId
    {
        _nostate_ = 0,
        CompleteInvalidNetAdapterStart = 1,
        CompleteNetAdapterStart = 2,
        CompleteNetAdapterStartDefer = 3,
        CompleteNetAdapterStop = 4,
        CompleteNetAdapterStopNoOp = 5,
        DeferInterfaceStart = 6,
        DequeueInvalidNetAdapterStart = 7,
        DequeueNetAdapterStart = 8,
        DequeueNetAdapterStop = 9,
        DequeueNetAdapterStopNoOp = 10,
        DeviceSurpriseRemoved = 11,
        GoingToInterfaceStoppedImplicitIoStopped = 12,
        GoingToInterfaceStoppedIoStarted = 13,
        GoingToInterfaceStoppedIoStopped = 14,
        GoingToInterfaceStoppedIoStoppedD3Final = 15,
        InterfaceAlreadyStopped = 16,
        InterfaceStartedGoingToIoStarted = 17,
        InterfaceStartedGoingToIoStoppedD1 = 18,
        InterfaceStartedGoingToIoStoppedD2 = 19,
        InterfaceStartedGoingToIoStoppedD3 = 20,
        InterfaceStartedGoingToIoStoppedD3Final = 21,
        InterfaceStartedImplicitIoStarted = 22,
        InterfaceStartedImplicitIoStoppedGoingToSurpriseRemoved = 23,
        InterfaceStartedIoStarted = 24,
        InterfaceStartedIoStartingD0 = 25,
        InterfaceStartedIoStopped = 26,
        InterfaceStartedIoStoppedD3Final = 27,
        InterfaceStartedIoStoppedDx = 28,
        InterfaceStartedIoStoppedGoingToSurpriseRemoved = 29,
        InterfaceStop = 30,
        InterfaceStopped = 31,
        InterfaceStoppedIoStoppedD3Final = 32,
        InterfaceStoppedSignalIoStarted = 33,
        InterfaceStoppedSignalIoStopped = 34,
        InterfaceStoppedSignalIoStoppedD3Final = 35,
        InvalidNetAdapterStart = 36,
        InvalidObjectState = 37,
        IoEventNoOp = 38,
        IoStarted = 39,
        IoStopped = 40,
        ObjectDeleted = 41,
        ShouldStartInterface = 42,
        SignalIoEvent = 43,
        StartingInterface = 44,
        StopNetworkInterface = 45,
        StopNetworkInterfaceNoOp = 46,
    };

    // Enumeration of event IDs.
    enum class EventId : SmFx::EventId
    {
        _noevent_ = 0,
        Cleanup = 1,
        Destroy = 2,
        DeviceRestart = 3,
        Fail = 4,
        GoToNextState = 5,
        IoInterfaceArrival = 6,
        IoStart = 7,
        IoStopD1 = 8,
        IoStopD2 = 9,
        IoStopD3 = 10,
        IoStopD3Final = 11,
        NetAdapterStart = 12,
        NetAdapterStop = 13,
        No = 14,
        Success = 15,
        SurpriseRemove = 16,
        Yes = 17,
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
    AdapterPnpPowerStateMachine() = default;

    // This class is expected to be a base class, but polymorphic access through this base class is
    // not allowed. Hence, the destructor is protected.
    ~AdapterPnpPowerStateMachine();

    // No copy semantices, until we can decide what exactly it means to "copy" a state machine.

    AdapterPnpPowerStateMachine(
        const AdapterPnpPowerStateMachine&) = delete;

    AdapterPnpPowerStateMachine&
    operator=(
        const AdapterPnpPowerStateMachine&) = delete;

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
        static SmFx::StateEntryFunction CompleteInvalidNetAdapterStartEntry;
        static SmFx::StateEntryFunction CompleteNetAdapterStartEntry;
        static SmFx::StateEntryFunction CompleteNetAdapterStartDeferEntry;
        static SmFx::StateEntryFunction CompleteNetAdapterStopEntry;
        static SmFx::StateEntryFunction CompleteNetAdapterStopNoOpEntry;
        static SmFx::StateEntryFunction DeferInterfaceStartEntry;
        static SmFx::StateEntryFunction DequeueInvalidNetAdapterStartEntry;
        static SmFx::StateEntryFunction DequeueNetAdapterStartEntry;
        static SmFx::StateEntryFunction DequeueNetAdapterStopEntry;
        static SmFx::StateEntryFunction DequeueNetAdapterStopNoOpEntry;
        static SmFx::StateEntryFunction GoingToInterfaceStoppedImplicitIoStoppedEntry;
        static SmFx::StateEntryFunction InterfaceStartedGoingToIoStartedEntry;
        static SmFx::StateEntryFunction InterfaceStartedGoingToIoStoppedD1Entry;
        static SmFx::StateEntryFunction InterfaceStartedGoingToIoStoppedD2Entry;
        static SmFx::StateEntryFunction InterfaceStartedGoingToIoStoppedD3Entry;
        static SmFx::StateEntryFunction InterfaceStartedGoingToIoStoppedD3FinalEntry;
        static SmFx::StateEntryFunction InterfaceStartedImplicitIoStartedEntry;
        static SmFx::StateEntryFunction InterfaceStartedImplicitIoStoppedGoingToSurpriseRemovedEntry;
        static SmFx::StateEntryFunction InterfaceStartedIoStartingD0Entry;
        static SmFx::StateEntryFunction InterfaceStartedIoStoppedGoingToSurpriseRemovedEntry;
        static SmFx::StateEntryFunction InterfaceStoppedIoStoppedD3FinalEntry;
        static SmFx::StateEntryFunction InvalidNetAdapterStartEntry;
        static SmFx::StateEntryFunction ObjectDeletedEntry;
        static SmFx::StateEntryFunction ShouldStartInterfaceEntry;
        static SmFx::StateEntryFunction SignalIoEventEntry;
        static SmFx::StateEntryFunction StartingInterfaceEntry;
        static SmFx::StateEntryFunction StopNetworkInterfaceEntry;
        static SmFx::StateEntryFunction StopNetworkInterfaceNoOpEntry;
    };

    // Internal transition actions.
    struct Actions
    {
        static SmFx::InternalTransitionAction InterfaceStartedIoStartedActionOnIoInterfaceArrival;
    };

    // Enumeration of state indices.
    enum class StateIndex : SmFx::StateIndex
    {
        _nostate_ = 0,
        CompleteInvalidNetAdapterStart = 1,
        CompleteNetAdapterStart = 2,
        CompleteNetAdapterStartDefer = 3,
        CompleteNetAdapterStop = 4,
        CompleteNetAdapterStopNoOp = 5,
        DeferInterfaceStart = 6,
        DequeueInvalidNetAdapterStart = 7,
        DequeueNetAdapterStart = 8,
        DequeueNetAdapterStop = 9,
        DequeueNetAdapterStopNoOp = 10,
        DeviceSurpriseRemoved = 11,
        GoingToInterfaceStoppedImplicitIoStopped = 12,
        GoingToInterfaceStoppedIoStarted = 13,
        GoingToInterfaceStoppedIoStopped = 14,
        GoingToInterfaceStoppedIoStoppedD3Final = 15,
        InterfaceAlreadyStopped = 16,
        InterfaceStartedGoingToIoStarted = 17,
        InterfaceStartedGoingToIoStoppedD1 = 18,
        InterfaceStartedGoingToIoStoppedD2 = 19,
        InterfaceStartedGoingToIoStoppedD3 = 20,
        InterfaceStartedGoingToIoStoppedD3Final = 21,
        InterfaceStartedImplicitIoStarted = 22,
        InterfaceStartedImplicitIoStoppedGoingToSurpriseRemoved = 23,
        InterfaceStartedIoStarted = 24,
        InterfaceStartedIoStartingD0 = 25,
        InterfaceStartedIoStopped = 26,
        InterfaceStartedIoStoppedD3Final = 27,
        InterfaceStartedIoStoppedDx = 28,
        InterfaceStartedIoStoppedGoingToSurpriseRemoved = 29,
        InterfaceStop = 30,
        InterfaceStopped = 31,
        InterfaceStoppedIoStoppedD3Final = 32,
        InterfaceStoppedSignalIoStarted = 33,
        InterfaceStoppedSignalIoStopped = 34,
        InterfaceStoppedSignalIoStoppedD3Final = 35,
        InvalidNetAdapterStart = 36,
        InvalidObjectState = 37,
        IoEventNoOp = 38,
        IoStarted = 39,
        IoStopped = 40,
        ObjectDeleted = 41,
        ShouldStartInterface = 42,
        SignalIoEvent = 43,
        StartingInterface = 44,
        StopNetworkInterface = 45,
        StopNetworkInterfaceNoOp = 46,
    };

    // Enumeration of submachine names.
    enum class SubmachineName : SmFx::SubmachineIndex
    {
        _nosubmachine_ = 0,
        AdapterPnpPower = 1,
        InterfaceStopped = 2,
        InvalidObjectState = 3,
        NetworkInterfaceStop = 4,
        NetworkInterfaceStopNoOp = 5,
        SignalIoEvent = 6,
    };

    // Table of submachine specifications.
    static const SmFx::SUBMACHINE_SPECIFICATION c_machineTable[];

    // Declarations of pop transitions for each state that has any.
    struct PopTransitions
    {
        static const SmFx::POP_TRANSITION c_completeNetAdapterStart[];
        static const SmFx::POP_TRANSITION c_completeNetAdapterStop[];
        static const SmFx::POP_TRANSITION c_completeNetAdapterStopNoOp[];
        static const SmFx::POP_TRANSITION c_objectDeleted[];
        static const SmFx::POP_TRANSITION c_signalIoEvent[];
    };

    // Declarations of internal transitions for each state that has any.
    struct InternalTransitions
    {
        static const SmFx::INTERNAL_TRANSITION c_interfaceStartedIoStarted[];
        static const SmFx::INTERNAL_TRANSITION c_interfaceStartedIoStoppedD3Final[];
        static const SmFx::INTERNAL_TRANSITION c_interfaceStoppedIoStoppedD3Final[];
        static const SmFx::INTERNAL_TRANSITION c_invalidObjectState[];
        static const SmFx::INTERNAL_TRANSITION c_objectDeleted[];
    };

    // Declarations of external transitions for each state that has any.
    struct ExternalTransitions
    {
        static const SmFx::EXTERNAL_TRANSITION c_completeInvalidNetAdapterStart[];
        static const SmFx::EXTERNAL_TRANSITION c_completeNetAdapterStartDefer[];
        static const SmFx::EXTERNAL_TRANSITION c_deferInterfaceStart[];
        static const SmFx::EXTERNAL_TRANSITION c_dequeueInvalidNetAdapterStart[];
        static const SmFx::EXTERNAL_TRANSITION c_dequeueNetAdapterStart[];
        static const SmFx::EXTERNAL_TRANSITION c_dequeueNetAdapterStop[];
        static const SmFx::EXTERNAL_TRANSITION c_dequeueNetAdapterStopNoOp[];
        static const SmFx::EXTERNAL_TRANSITION c_deviceSurpriseRemoved[];
        static const SmFx::EXTERNAL_TRANSITION c_goingToInterfaceStoppedImplicitIoStopped[];
        static const SmFx::EXTERNAL_TRANSITION c_goingToInterfaceStoppedIoStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_goingToInterfaceStoppedIoStopped[];
        static const SmFx::EXTERNAL_TRANSITION c_goingToInterfaceStoppedIoStoppedD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceAlreadyStopped[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedGoingToIoStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedGoingToIoStoppedD1[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedGoingToIoStoppedD2[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedGoingToIoStoppedD3[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedGoingToIoStoppedD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedImplicitIoStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedImplicitIoStoppedGoingToSurpriseRemoved[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedIoStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedIoStartingD0[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedIoStopped[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedIoStoppedD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedIoStoppedDx[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStartedIoStoppedGoingToSurpriseRemoved[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStop[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStopped[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStoppedIoStoppedD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStoppedSignalIoStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStoppedSignalIoStopped[];
        static const SmFx::EXTERNAL_TRANSITION c_interfaceStoppedSignalIoStoppedD3Final[];
        static const SmFx::EXTERNAL_TRANSITION c_invalidNetAdapterStart[];
        static const SmFx::EXTERNAL_TRANSITION c_invalidObjectState[];
        static const SmFx::EXTERNAL_TRANSITION c_ioEventNoOp[];
        static const SmFx::EXTERNAL_TRANSITION c_ioStarted[];
        static const SmFx::EXTERNAL_TRANSITION c_ioStopped[];
        static const SmFx::EXTERNAL_TRANSITION c_shouldStartInterface[];
        static const SmFx::EXTERNAL_TRANSITION c_startingInterface[];
        static const SmFx::EXTERNAL_TRANSITION c_stopNetworkInterface[];
        static const SmFx::EXTERNAL_TRANSITION c_stopNetworkInterfaceNoOp[];
    };

    // Declarations of deferred events for each state that has any.
    struct DeferredEvents
    {
        static const SmFx::EventIndex c_interfaceStartedIoStarted[];
        static const SmFx::EventIndex c_interfaceStartedIoStopped[];
        static const SmFx::EventIndex c_interfaceStartedIoStoppedD3Final[];
        static const SmFx::EventIndex c_interfaceStartedIoStoppedDx[];
    };

    // Declarations of purged events for each state that has any.
    struct PurgeEvents
    {
        static const SmFx::EventIndex c_ioStarted[];
        static const SmFx::EventIndex c_ioStopped[];
    };

    // Declarations of stop-timer-on-exit details for each state that has it.
    struct StopTimerOnExitDetails
    {
    };

    // Declarations of slot arrays for states that have a non-empty slot array.
    struct SlotArrays
    {
        static const SmFx::StateSlot c_completeInvalidNetAdapterStart[];
        static const SmFx::StateSlot c_completeNetAdapterStart[];
        static const SmFx::StateSlot c_completeNetAdapterStartDefer[];
        static const SmFx::StateSlot c_completeNetAdapterStop[];
        static const SmFx::StateSlot c_completeNetAdapterStopNoOp[];
        static const SmFx::StateSlot c_deferInterfaceStart[];
        static const SmFx::StateSlot c_dequeueInvalidNetAdapterStart[];
        static const SmFx::StateSlot c_dequeueNetAdapterStart[];
        static const SmFx::StateSlot c_dequeueNetAdapterStop[];
        static const SmFx::StateSlot c_dequeueNetAdapterStopNoOp[];
        static const SmFx::StateSlot c_deviceSurpriseRemoved[];
        static const SmFx::StateSlot c_goingToInterfaceStoppedImplicitIoStopped[];
        static const SmFx::StateSlot c_goingToInterfaceStoppedIoStarted[];
        static const SmFx::StateSlot c_goingToInterfaceStoppedIoStopped[];
        static const SmFx::StateSlot c_goingToInterfaceStoppedIoStoppedD3Final[];
        static const SmFx::StateSlot c_interfaceAlreadyStopped[];
        static const SmFx::StateSlot c_interfaceStartedGoingToIoStarted[];
        static const SmFx::StateSlot c_interfaceStartedGoingToIoStoppedD1[];
        static const SmFx::StateSlot c_interfaceStartedGoingToIoStoppedD2[];
        static const SmFx::StateSlot c_interfaceStartedGoingToIoStoppedD3[];
        static const SmFx::StateSlot c_interfaceStartedGoingToIoStoppedD3Final[];
        static const SmFx::StateSlot c_interfaceStartedImplicitIoStarted[];
        static const SmFx::StateSlot c_interfaceStartedImplicitIoStoppedGoingToSurpriseRemoved[];
        static const SmFx::StateSlot c_interfaceStartedIoStarted[];
        static const SmFx::StateSlot c_interfaceStartedIoStartingD0[];
        static const SmFx::StateSlot c_interfaceStartedIoStopped[];
        static const SmFx::StateSlot c_interfaceStartedIoStoppedD3Final[];
        static const SmFx::StateSlot c_interfaceStartedIoStoppedDx[];
        static const SmFx::StateSlot c_interfaceStartedIoStoppedGoingToSurpriseRemoved[];
        static const SmFx::StateSlot c_interfaceStop[];
        static const SmFx::StateSlot c_interfaceStopped[];
        static const SmFx::StateSlot c_interfaceStoppedIoStoppedD3Final[];
        static const SmFx::StateSlot c_interfaceStoppedSignalIoStarted[];
        static const SmFx::StateSlot c_interfaceStoppedSignalIoStopped[];
        static const SmFx::StateSlot c_interfaceStoppedSignalIoStoppedD3Final[];
        static const SmFx::StateSlot c_invalidNetAdapterStart[];
        static const SmFx::StateSlot c_invalidObjectState[];
        static const SmFx::StateSlot c_ioEventNoOp[];
        static const SmFx::StateSlot c_ioStarted[];
        static const SmFx::StateSlot c_ioStopped[];
        static const SmFx::StateSlot c_objectDeleted[];
        static const SmFx::StateSlot c_shouldStartInterface[];
        static const SmFx::StateSlot c_signalIoEvent[];
        static const SmFx::StateSlot c_startingInterface[];
        static const SmFx::StateSlot c_stopNetworkInterface[];
        static const SmFx::StateSlot c_stopNetworkInterfaceNoOp[];
    };

    // Declaration of the complete state machine specification.
    static const SmFx::STATE_MACHINE_SPECIFICATION c_specification;

    // The state machine engine.
    SmFx::StateMachineEngine m_engine;
};

// Engine method implementations.

template<typename T>
NTSTATUS
AdapterPnpPowerStateMachine<T>::Initialize(
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
    engineConfig.stackSize = 3;

    SetLogMachineExceptionCallback<T>(&engineConfig, true);
    SetLogEventEnqueueCallback<T>(&engineConfig, true);
    SetLogTransitionCallback<T>(&engineConfig, true);
    SetMachineDestroyedCallback<T>(&engineConfig, true);

    return m_engine.Initialize(engineConfig);
}

template<typename T>
bool
AdapterPnpPowerStateMachine<T>::IsInitialized()
{
    return m_engine.IsInitialized();
}

template<typename T>
AdapterPnpPowerStateMachine<T>::~AdapterPnpPowerStateMachine()
{
}

template<typename T>
void
AdapterPnpPowerStateMachine<T>::EnqueueEvent(
    typename AdapterPnpPowerStateMachine<T>::Event event)
{
    m_engine.EnqueueEvent(static_cast<SmFx::EventIndex>(event));
}

template<typename T>
void
AdapterPnpPowerStateMachine<T>::EvtLogMachineExceptionThunk(
    void* context,
    SmFx::MachineException exception,
    SmFx::EventId relevantEvent,
    SmFx::StateId relevantState)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->EvtLogMachineException(
        exception,
        static_cast<EventId>(relevantEvent),
        static_cast<StateId>(relevantState));
}

template<typename T>
void
AdapterPnpPowerStateMachine<T>::EvtLogEventEnqueueThunk(
    void* context,
    SmFx::EventId relevantEvent)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->EvtLogEventEnqueue(static_cast<EventId>(relevantEvent));
}

template<typename T>
void
AdapterPnpPowerStateMachine<T>::EvtLogTransitionThunk(
    void* context,
    SmFx::TransitionType transitionType,
    SmFx::StateId sourceState,
    SmFx::EventId processedEvent,
    SmFx::StateId targetState)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->EvtLogTransition(
        transitionType,
        static_cast<StateId>(sourceState),
        static_cast<EventId>(processedEvent),
        static_cast<StateId>(targetState));
}

template<typename T>
void
AdapterPnpPowerStateMachine<T>::EvtMachineDestroyedThunk(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->EvtMachineDestroyed();
}

// Definitions for each of the state entry functions.

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteInvalidNetAdapterStartEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->CompleteNetAdapterStart();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteNetAdapterStartEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->CompleteNetAdapterStart();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteNetAdapterStartDeferEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->CompleteNetAdapterStart();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteNetAdapterStopEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->CompleteNetAdapterStop();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteNetAdapterStopNoOpEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->CompleteNetAdapterStop();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::DeferInterfaceStartEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->DeferStartNetworkInterface();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::DequeueInvalidNetAdapterStartEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->DequeueNetAdapterStart();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::DequeueNetAdapterStartEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->DequeueNetAdapterStart();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::DequeueNetAdapterStopEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->DequeueNetAdapterStop();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::DequeueNetAdapterStopNoOpEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->DequeueNetAdapterStop();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::GoingToInterfaceStoppedImplicitIoStoppedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StopIoD3Final();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStartedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StartIo();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStoppedD1Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->StopIoD1();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStoppedD2Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StopIoD2();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStoppedD3Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StopIoD3();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStoppedD3FinalEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StopIoD3Final();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedImplicitIoStartedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StartIo();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedImplicitIoStoppedGoingToSurpriseRemovedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StopIoD3Final();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedIoStartingD0Entry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StartIoD0();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedIoStoppedGoingToSurpriseRemovedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->ReportSurpriseRemoval();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStoppedIoStoppedD3FinalEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->DeviceReleased();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::InvalidNetAdapterStartEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->StartNetworkInterfaceInvalidDeviceState();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::ObjectDeletedEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->Deleted();

    return static_cast<SmFx::EventIndex>(0);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::ShouldStartInterfaceEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    AdapterPnpPowerStateMachine<T>::Event returnEvent = derived->ShouldStartNetworkInterface();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::SignalIoEventEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->IoEventProcessed();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::StartingInterfaceEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    AdapterPnpPowerStateMachine<T>::Event returnEvent = derived->StartNetworkInterface();

    return static_cast<SmFx::EventIndex>(returnEvent);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::StopNetworkInterfaceEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

#ifdef _KERNEL_MODE
    _Analysis_assume_(KeGetCurrentIrql() == PASSIVE_LEVEL);
#endif // #ifdef _KERNEL_MODE

    derived->StopNetworkInterface();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

template<typename T>
SmFx::EventIndex
AdapterPnpPowerStateMachine<T>::EntryFuncs::StopNetworkInterfaceNoOpEntry(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->StopNetworkInterfaceNoOp();

    return static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState);
}

// Definitions for each of the internal transition actions.

template<typename T>
void
AdapterPnpPowerStateMachine<T>::Actions::InterfaceStartedIoStartedActionOnIoInterfaceArrival(
    void* context)
{
    auto derived = static_cast<T*>(reinterpret_cast<AdapterPnpPowerStateMachine<T>*>(context));

    derived->AllowBindings();
}

// Pop transitions.

template <typename T>
const SmFx::POP_TRANSITION AdapterPnpPowerStateMachine<T>::PopTransitions::c_completeNetAdapterStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Return event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStart)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

template <typename T>
const SmFx::POP_TRANSITION AdapterPnpPowerStateMachine<T>::PopTransitions::c_completeNetAdapterStop[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Return event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

template <typename T>
const SmFx::POP_TRANSITION AdapterPnpPowerStateMachine<T>::PopTransitions::c_completeNetAdapterStopNoOp[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Return event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

template <typename T>
const SmFx::POP_TRANSITION AdapterPnpPowerStateMachine<T>::PopTransitions::c_objectDeleted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Destroy),
        // Return event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Destroy)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

template <typename T>
const SmFx::POP_TRANSITION AdapterPnpPowerStateMachine<T>::PopTransitions::c_signalIoEvent[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Return event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::EventIndex>(0)
    }
};

// Internal transitions.

template <typename T>
const SmFx::INTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::InternalTransitions::c_interfaceStartedIoStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoInterfaceArrival),
        // Flags.
        SmFx::InternalTransitionFlags::None,
        // Action.
        &AdapterPnpPowerStateMachine<T>::Actions::InterfaceStartedIoStartedActionOnIoInterfaceArrival
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        SmFx::InternalTransitionFlags::None,
        nullptr
    }
};

template <typename T>
const SmFx::INTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::InternalTransitions::c_interfaceStartedIoStoppedD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoInterfaceArrival),
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
const SmFx::INTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::InternalTransitions::c_interfaceStoppedIoStoppedD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::SurpriseRemove),
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
const SmFx::INTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::InternalTransitions::c_invalidObjectState[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoInterfaceArrival),
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
const SmFx::INTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::InternalTransitions::c_objectDeleted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::DeviceRestart),
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
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_completeInvalidNetAdapterStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InvalidObjectState)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_completeNetAdapterStartDefer[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStopped)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_deferInterfaceStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::CompleteNetAdapterStartDefer)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_dequeueInvalidNetAdapterStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InvalidNetAdapterStart)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_dequeueNetAdapterStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::ShouldStartInterface)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_dequeueNetAdapterStop[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::StopNetworkInterface)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_dequeueNetAdapterStopNoOp[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::StopNetworkInterfaceNoOp)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_deviceSurpriseRemoved[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Cleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::ObjectDeleted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_goingToInterfaceStoppedImplicitIoStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::GoingToInterfaceStoppedIoStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_goingToInterfaceStoppedIoStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_goingToInterfaceStoppedIoStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoStopped)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_goingToInterfaceStoppedIoStoppedD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStoppedIoStoppedD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceAlreadyStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStopped)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStoppedD1[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStoppedDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStoppedD2[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStoppedDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStoppedD3[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStoppedDx)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStoppedD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStoppedD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedImplicitIoStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedImplicitIoStoppedGoingToSurpriseRemoved[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStoppedGoingToSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD1),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedGoingToIoStoppedD1)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD2),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedGoingToIoStoppedD2)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD3),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedGoingToIoStoppedD3)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD3Final),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedGoingToIoStoppedD3Final)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStop),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::GoingToInterfaceStoppedImplicitIoStopped)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedImplicitIoStoppedGoingToSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStartingD0[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedGoingToIoStarted)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStop),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::GoingToInterfaceStoppedIoStopped)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStoppedGoingToSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStoppedD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStop),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::GoingToInterfaceStoppedIoStoppedD3Final)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStoppedGoingToSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStoppedDx[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStartingD0)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStop),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::GoingToInterfaceStoppedIoStopped)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStoppedGoingToSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStoppedGoingToSurpriseRemoved[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::DeviceSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStop[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InvalidObjectState)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::DequeueNetAdapterStart)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStop),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceAlreadyStopped)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStoppedIoStoppedD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Cleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::ObjectDeleted)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::DeviceRestart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoStopped)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStoppedSignalIoStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoStarted)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStoppedSignalIoStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoStopped)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStoppedSignalIoStoppedD3Final[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStoppedIoStoppedD3Final)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_invalidNetAdapterStart[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::CompleteInvalidNetAdapterStart)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_invalidObjectState[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoEventNoOp)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD1),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoEventNoOp)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD2),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoEventNoOp)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD3),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoEventNoOp)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD3Final),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoEventNoOp)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::DequeueInvalidNetAdapterStart)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStop),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStop)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_ioEventNoOp[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InvalidObjectState)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_ioStarted[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Cleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::ObjectDeleted)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD1),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStoppedSignalIoStopped)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD2),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStoppedSignalIoStopped)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD3),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStoppedSignalIoStopped)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStopD3Final),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStoppedSignalIoStoppedD3Final)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedImplicitIoStarted)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::DeviceSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_ioStopped[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Cleanup),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::ObjectDeleted)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoStart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStoppedSignalIoStarted)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::NetAdapterStart),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStartedIoStopped)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::SurpriseRemove),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::DeviceSurpriseRemoved)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_shouldStartInterface[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::No),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::DeferInterfaceStart)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Yes),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::StartingInterface)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_startingInterface[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Fail),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::CompleteNetAdapterStartDefer)
    },
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Success),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::CompleteNetAdapterStart)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_stopNetworkInterface[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::CompleteNetAdapterStop)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

template <typename T>
const SmFx::EXTERNAL_TRANSITION AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_stopNetworkInterfaceNoOp[] =
{
    {
        // Triggering event.
        static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
        // Target state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::CompleteNetAdapterStopNoOp)
    },
    {
        // Sentinel.
        static_cast<SmFx::EventIndex>(0),
        static_cast<SmFx::StateIndex>(0)
    }
};

// Deferred events.

template <typename T>
const SmFx::EventIndex AdapterPnpPowerStateMachine<T>::DeferredEvents::c_interfaceStartedIoStarted[] =
{
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Cleanup),
    static_cast<SmFx::EventIndex>(0)
};

template <typename T>
const SmFx::EventIndex AdapterPnpPowerStateMachine<T>::DeferredEvents::c_interfaceStartedIoStopped[] =
{
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Cleanup),
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoInterfaceArrival),
    static_cast<SmFx::EventIndex>(0)
};

template <typename T>
const SmFx::EventIndex AdapterPnpPowerStateMachine<T>::DeferredEvents::c_interfaceStartedIoStoppedD3Final[] =
{
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Cleanup),
    static_cast<SmFx::EventIndex>(0)
};

template <typename T>
const SmFx::EventIndex AdapterPnpPowerStateMachine<T>::DeferredEvents::c_interfaceStartedIoStoppedDx[] =
{
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::Cleanup),
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoInterfaceArrival),
    static_cast<SmFx::EventIndex>(0)
};

// Purged events.

template <typename T>
const SmFx::EventIndex AdapterPnpPowerStateMachine<T>::PurgeEvents::c_ioStarted[] =
{
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoInterfaceArrival),
    static_cast<SmFx::EventIndex>(0)
};

template <typename T>
const SmFx::EventIndex AdapterPnpPowerStateMachine<T>::PurgeEvents::c_ioStopped[] =
{
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::IoInterfaceArrival),
    static_cast<SmFx::EventIndex>(0)
};

// Stop-timer-on-exit details.

// Event table.
template<typename T>
const SmFx::EVENT_SPECIFICATION AdapterPnpPowerStateMachine<T>::c_eventTable[] =
{
    {
        // Unused.
        static_cast<SmFx::EventId>(0),
        SmFx::EventQueueingDisposition::Invalid
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::Cleanup),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::Destroy),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::DeviceRestart),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::Fail),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::GoToNextState),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::IoInterfaceArrival),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::IoStart),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::IoStopD1),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::IoStopD2),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::IoStopD3),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::IoStopD3Final),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::NetAdapterStart),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::NetAdapterStop),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::No),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::Success),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::SurpriseRemove),
        SmFx::EventQueueingDisposition::Unlimited
    },
    {
        static_cast<SmFx::EventId>(AdapterPnpPowerStateMachine<T>::EventId::Yes),
        SmFx::EventQueueingDisposition::Unlimited
    },
};

// Submachine table.
template<typename T>
const SmFx::SUBMACHINE_SPECIFICATION AdapterPnpPowerStateMachine<T>::c_machineTable[] =
{
    {},
    // Submachine: AdapterPnpPower
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::IoStopped)
    },
    // Submachine: InterfaceStopped
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InterfaceStopped)
    },
    // Submachine: InvalidObjectState
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::InvalidObjectState)
    },
    // Submachine: NetworkInterfaceStop
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::DequeueNetAdapterStop)
    },
    // Submachine: NetworkInterfaceStopNoOp
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::DequeueNetAdapterStopNoOp)
    },
    // Submachine: SignalIoEvent
    {
        // Initial state.
        static_cast<SmFx::StateIndex>(AdapterPnpPowerStateMachine<T>::StateIndex::SignalIoEvent)
    },
};

// Slot arrays.

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeInvalidNetAdapterStart[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteInvalidNetAdapterStartEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_completeInvalidNetAdapterStart,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeNetAdapterStart[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteNetAdapterStartEntry),
    AdapterPnpPowerStateMachine<T>::PopTransitions::c_completeNetAdapterStart,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeNetAdapterStartDefer[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteNetAdapterStartDeferEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_completeNetAdapterStartDefer,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeNetAdapterStop[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteNetAdapterStopEntry),
    AdapterPnpPowerStateMachine<T>::PopTransitions::c_completeNetAdapterStop,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeNetAdapterStopNoOp[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::CompleteNetAdapterStopNoOpEntry),
    AdapterPnpPowerStateMachine<T>::PopTransitions::c_completeNetAdapterStopNoOp,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_deferInterfaceStart[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::DeferInterfaceStartEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_deferInterfaceStart,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_dequeueInvalidNetAdapterStart[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::DequeueInvalidNetAdapterStartEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_dequeueInvalidNetAdapterStart,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_dequeueNetAdapterStart[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::DequeueNetAdapterStartEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_dequeueNetAdapterStart,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_dequeueNetAdapterStop[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::DequeueNetAdapterStopEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_dequeueNetAdapterStop,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_dequeueNetAdapterStopNoOp[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::DequeueNetAdapterStopNoOpEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_dequeueNetAdapterStopNoOp,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_deviceSurpriseRemoved[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_deviceSurpriseRemoved,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_goingToInterfaceStoppedImplicitIoStopped[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::GoingToInterfaceStoppedImplicitIoStoppedEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_goingToInterfaceStoppedImplicitIoStopped,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_goingToInterfaceStoppedIoStarted[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_goingToInterfaceStoppedIoStarted,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_goingToInterfaceStoppedIoStopped[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_goingToInterfaceStoppedIoStopped,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_goingToInterfaceStoppedIoStoppedD3Final[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_goingToInterfaceStoppedIoStoppedD3Final,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceAlreadyStopped[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceAlreadyStopped,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStarted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStartedEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStarted,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStoppedD1[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStoppedD1Entry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStoppedD1,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStoppedD2[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStoppedD2Entry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStoppedD2,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStoppedD3[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStoppedD3Entry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStoppedD3,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStoppedD3Final[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedGoingToIoStoppedD3FinalEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedGoingToIoStoppedD3Final,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedImplicitIoStarted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedImplicitIoStartedEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedImplicitIoStarted,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedImplicitIoStoppedGoingToSurpriseRemoved[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedImplicitIoStoppedGoingToSurpriseRemovedEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedImplicitIoStoppedGoingToSurpriseRemoved,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStarted[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStarted,
    AdapterPnpPowerStateMachine<T>::InternalTransitions::c_interfaceStartedIoStarted,
    AdapterPnpPowerStateMachine<T>::DeferredEvents::c_interfaceStartedIoStarted,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStartingD0[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedIoStartingD0Entry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStartingD0,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStopped[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStopped,
    AdapterPnpPowerStateMachine<T>::DeferredEvents::c_interfaceStartedIoStopped,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStoppedD3Final[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStoppedD3Final,
    AdapterPnpPowerStateMachine<T>::InternalTransitions::c_interfaceStartedIoStoppedD3Final,
    AdapterPnpPowerStateMachine<T>::DeferredEvents::c_interfaceStartedIoStoppedD3Final,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStoppedDx[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStoppedDx,
    AdapterPnpPowerStateMachine<T>::DeferredEvents::c_interfaceStartedIoStoppedDx,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStoppedGoingToSurpriseRemoved[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStartedIoStoppedGoingToSurpriseRemovedEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStartedIoStoppedGoingToSurpriseRemoved,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStop[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStop,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStopped[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStopped,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStoppedIoStoppedD3Final[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InterfaceStoppedIoStoppedD3FinalEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStoppedIoStoppedD3Final,
    AdapterPnpPowerStateMachine<T>::InternalTransitions::c_interfaceStoppedIoStoppedD3Final,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStoppedSignalIoStarted[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStoppedSignalIoStarted,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStoppedSignalIoStopped[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStoppedSignalIoStopped,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStoppedSignalIoStoppedD3Final[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_interfaceStoppedSignalIoStoppedD3Final,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_invalidNetAdapterStart[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::InvalidNetAdapterStartEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_invalidNetAdapterStart,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_invalidObjectState[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_invalidObjectState,
    AdapterPnpPowerStateMachine<T>::InternalTransitions::c_invalidObjectState,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_ioEventNoOp[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_ioEventNoOp,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_ioStarted[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_ioStarted,
    AdapterPnpPowerStateMachine<T>::PurgeEvents::c_ioStarted,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_ioStopped[] =
{
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_ioStopped,
    AdapterPnpPowerStateMachine<T>::PurgeEvents::c_ioStopped,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_objectDeleted[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::ObjectDeletedEntry),
    AdapterPnpPowerStateMachine<T>::InternalTransitions::c_objectDeleted,
    AdapterPnpPowerStateMachine<T>::PopTransitions::c_objectDeleted,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_shouldStartInterface[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::ShouldStartInterfaceEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_shouldStartInterface,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_signalIoEvent[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::SignalIoEventEntry),
    AdapterPnpPowerStateMachine<T>::PopTransitions::c_signalIoEvent,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_startingInterface[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::StartingInterfaceEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_startingInterface,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_stopNetworkInterface[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::StopNetworkInterfaceEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_stopNetworkInterface,
};

template <typename T>
const SmFx::StateSlot AdapterPnpPowerStateMachine<T>::SlotArrays::c_stopNetworkInterfaceNoOp[] =
{
    reinterpret_cast<SmFx::StateSlot>(&AdapterPnpPowerStateMachine<T>::EntryFuncs::StopNetworkInterfaceNoOpEntry),
    AdapterPnpPowerStateMachine<T>::ExternalTransitions::c_stopNetworkInterfaceNoOp,
};

// State table.
template<typename T>
const SmFx::STATE_SPECIFICATION AdapterPnpPowerStateMachine<T>::c_stateTable[] =
{
    {},
    // State: CompleteInvalidNetAdapterStart
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::CompleteInvalidNetAdapterStart),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeInvalidNetAdapterStart,
    },
    // State: CompleteNetAdapterStart
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::CompleteNetAdapterStart),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeNetAdapterStart,
    },
    // State: CompleteNetAdapterStartDefer
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::CompleteNetAdapterStartDefer),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeNetAdapterStartDefer,
    },
    // State: CompleteNetAdapterStop
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::CompleteNetAdapterStop),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeNetAdapterStop,
    },
    // State: CompleteNetAdapterStopNoOp
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::CompleteNetAdapterStopNoOp),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_completeNetAdapterStopNoOp,
    },
    // State: DeferInterfaceStart
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::DeferInterfaceStart),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_deferInterfaceStart,
    },
    // State: DequeueInvalidNetAdapterStart
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::DequeueInvalidNetAdapterStart),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_dequeueInvalidNetAdapterStart,
    },
    // State: DequeueNetAdapterStart
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::DequeueNetAdapterStart),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_dequeueNetAdapterStart,
    },
    // State: DequeueNetAdapterStop
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::DequeueNetAdapterStop),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_dequeueNetAdapterStop,
    },
    // State: DequeueNetAdapterStopNoOp
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::DequeueNetAdapterStopNoOp),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_dequeueNetAdapterStopNoOp,
    },
    // State: DeviceSurpriseRemoved
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::DeviceSurpriseRemoved),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::InvalidObjectState),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_deviceSurpriseRemoved,
    },
    // State: GoingToInterfaceStoppedImplicitIoStopped
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::GoingToInterfaceStoppedImplicitIoStopped),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_goingToInterfaceStoppedImplicitIoStopped,
    },
    // State: GoingToInterfaceStoppedIoStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::GoingToInterfaceStoppedIoStarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::NetworkInterfaceStop),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_goingToInterfaceStoppedIoStarted,
    },
    // State: GoingToInterfaceStoppedIoStopped
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::GoingToInterfaceStoppedIoStopped),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::NetworkInterfaceStop),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_goingToInterfaceStoppedIoStopped,
    },
    // State: GoingToInterfaceStoppedIoStoppedD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::GoingToInterfaceStoppedIoStoppedD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::NetworkInterfaceStop),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_goingToInterfaceStoppedIoStoppedD3Final,
    },
    // State: InterfaceAlreadyStopped
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceAlreadyStopped),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::NetworkInterfaceStopNoOp),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceAlreadyStopped,
    },
    // State: InterfaceStartedGoingToIoStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedGoingToIoStarted),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStarted,
    },
    // State: InterfaceStartedGoingToIoStoppedD1
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedGoingToIoStoppedD1),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStoppedD1,
    },
    // State: InterfaceStartedGoingToIoStoppedD2
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedGoingToIoStoppedD2),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStoppedD2,
    },
    // State: InterfaceStartedGoingToIoStoppedD3
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedGoingToIoStoppedD3),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStoppedD3,
    },
    // State: InterfaceStartedGoingToIoStoppedD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedGoingToIoStoppedD3Final),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedGoingToIoStoppedD3Final,
    },
    // State: InterfaceStartedImplicitIoStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedImplicitIoStarted),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedImplicitIoStarted,
    },
    // State: InterfaceStartedImplicitIoStoppedGoingToSurpriseRemoved
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedImplicitIoStoppedGoingToSurpriseRemoved),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedImplicitIoStoppedGoingToSurpriseRemoved,
    },
    // State: InterfaceStartedIoStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedIoStarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::DeferredEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStarted,
    },
    // State: InterfaceStartedIoStartingD0
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedIoStartingD0),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStartingD0,
    },
    // State: InterfaceStartedIoStopped
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedIoStopped),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::DeferredEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStopped,
    },
    // State: InterfaceStartedIoStoppedD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedIoStoppedD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::DeferredEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStoppedD3Final,
    },
    // State: InterfaceStartedIoStoppedDx
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedIoStoppedDx),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::DeferredEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStoppedDx,
    },
    // State: InterfaceStartedIoStoppedGoingToSurpriseRemoved
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStartedIoStoppedGoingToSurpriseRemoved),
        // State flags.
        SmFx::StateFlags::RequiresDedicatedThread,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStartedIoStoppedGoingToSurpriseRemoved,
    },
    // State: InterfaceStop
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStop),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::NetworkInterfaceStopNoOp),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStop,
    },
    // State: InterfaceStopped
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStopped),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStopped,
    },
    // State: InterfaceStoppedIoStoppedD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStoppedIoStoppedD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::InvalidObjectState),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStoppedIoStoppedD3Final,
    },
    // State: InterfaceStoppedSignalIoStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStoppedSignalIoStarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStoppedSignalIoStarted,
    },
    // State: InterfaceStoppedSignalIoStopped
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStoppedSignalIoStopped),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStoppedSignalIoStopped,
    },
    // State: InterfaceStoppedSignalIoStoppedD3Final
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InterfaceStoppedSignalIoStoppedD3Final),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_interfaceStoppedSignalIoStoppedD3Final,
    },
    // State: InvalidNetAdapterStart
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InvalidNetAdapterStart),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_invalidNetAdapterStart,
    },
    // State: InvalidObjectState
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::InvalidObjectState),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Async,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_invalidObjectState,
    },
    // State: IoEventNoOp
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::IoEventNoOp),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        SmFx::StateSlotType::ExternalTransitions,
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::SignalIoEvent),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_ioEventNoOp,
    },
    // State: IoStarted
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::IoStarted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PurgeEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::InterfaceStopped),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_ioStarted,
    },
    // State: IoStopped
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::IoStopped),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PurgeEvents)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::InterfaceStopped),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_ioStopped,
    },
    // State: ObjectDeleted
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::ObjectDeleted),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Call,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::InternalTransitions) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::InvalidObjectState),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_objectDeleted,
    },
    // State: ShouldStartInterface
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::ShouldStartInterface),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_shouldStartInterface,
    },
    // State: SignalIoEvent
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::SignalIoEvent),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::PopTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_signalIoEvent,
    },
    // State: StartingInterface
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::StartingInterface),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_startingInterface,
    },
    // State: StopNetworkInterface
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::StopNetworkInterface),
        // State flags.
        SmFx::StateFlags::RequiresPassiveLevel,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_stopNetworkInterface,
    },
    // State: StopNetworkInterfaceNoOp
    {
        // State ID.
        static_cast<SmFx::StateId>(AdapterPnpPowerStateMachine<T>::StateId::StopNetworkInterfaceNoOp),
        // State flags.
        SmFx::StateFlags::None,
        // State type.
        SmFx::StateType::Sync,
        // State slots.
        static_cast<SmFx::StateSlotType>(static_cast<uint16_t>(SmFx::StateSlotType::EntryFunction) | static_cast<uint16_t>(SmFx::StateSlotType::ExternalTransitions)),
        // Submachine.
        static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::_nosubmachine_),
        // Slot array.
        AdapterPnpPowerStateMachine<T>::SlotArrays::c_stopNetworkInterfaceNoOp,
    },
};

// Machine specification.
template<typename T>
const SmFx::STATE_MACHINE_SPECIFICATION AdapterPnpPowerStateMachine<T>::c_specification =
{
    static_cast<SmFx::SubmachineIndex>(AdapterPnpPowerStateMachine<T>::SubmachineName::AdapterPnpPower),
    static_cast<SmFx::EventIndex>(AdapterPnpPowerStateMachine<T>::Event::GoToNextState),
    AdapterPnpPowerStateMachine<T>::c_machineTable,
    AdapterPnpPowerStateMachine<T>::c_eventTable,
    AdapterPnpPowerStateMachine<T>::c_stateTable
};
