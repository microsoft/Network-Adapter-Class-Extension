// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "Dispatcher.hpp"
#include "ExecutionContext.hpp"
#include "IrqlController.h"
#include "TimeBudget.h"
#include <EcEvents.h>
#include <kmemoryaccess.h>

bool operator!=(PROCESSOR_NUMBER const & P1, PROCESSOR_NUMBER const & P2)
{
    return P1.Group != P2.Group || P1.Number != P2.Number;
}

static
EC_START_ROUTINE
    DispatcherThread;

_Use_decl_annotations_
Dispatcher::Dispatcher(
    IKnobs & EcKnobs
)
    : m_dpc{ *this }
    , m_knobs(EcKnobs)
{
    InitializeListHead(&m_readyList);

    m_dpc.SetImportance(DpcImportance::MediumHigh);
}

_Use_decl_annotations_
Dispatcher::Dispatcher(
    PROCESSOR_NUMBER const & Processor,
    IKnobs & EcKnobs
    )
    : Dispatcher(EcKnobs)
{
    m_processor = Processor;
}

_Use_decl_annotations_
NTSTATUS
Dispatcher::Initialize()
{
    if (IsAffinitized())
    {
        m_dpc.SetTargetProcessorDpcEx(&m_processor);
    }

    auto ntStatus = InitializeName();

    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    ntStatus = EcThreadCreate(
        DispatcherThread,
        this,
        m_thread);

    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    m_threadReady.Wait();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
Dispatcher::InitializeName(
    void
)
{
    const size_t maxNameSize = IsAffinitized()
        ? sizeof(L"Core xxxx:xxxx Dispatcher")
        : sizeof(L"Unafinitized Dispatcher");

    const size_t allocationSize = sizeof(UNICODE_STRING) + maxNameSize;

    auto name = MakeSizedPoolPtrNP<UNICODE_STRING>('aNtD', allocationSize);

    if (!name)
    {
        return STATUS_INSUFF_SERVER_RESOURCES;
    }

    name->Buffer = reinterpret_cast<wchar_t *>(name.get() + 1);
    name->Length = 0;
    name->MaximumLength = static_cast<USHORT>(maxNameSize);

    NTSTATUS ntStatus;

    if (IsAffinitized())
    {
        ntStatus = RtlUnicodeStringPrintf(
            name.get(),
            L"Core %u:%u Dispatcher",
            m_processor.Group,
            m_processor.Number);
    }
    else
    {
        ntStatus = RtlUnicodeStringInit(
            name.get(),
            L"Unafinitized Dispatcher");
    }

    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    m_name = wistd::move(name);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
Dispatcher::~Dispatcher()
{
    if (m_thread)
    {
        m_exitThread = true;
        m_threadRequested.Set();

        EcThreadWaitForTermination(m_thread);
    }
}

bool
Dispatcher::IsAffinitized() const
{
    return m_processor.Number != MAXIMUM_PROC_PER_GROUP;
}

_Use_decl_annotations_
void
Dispatcher::DpcRoutine()
{
    NT_FRE_ASSERT(IsAffinitized());

    EnterDispatcherLockNotHeld<DispatcherContext::Dpc>();
}

_Use_decl_annotations_
EC_RETURN
DispatcherThread(
    void * Context
)
{
    auto dispatcher = static_cast<Dispatcher *>(Context);
    dispatcher->ThreadRoutine();

    return EC_RETURN();
}

_Use_decl_annotations_
void
Dispatcher::ThreadRoutine()
{
    if (IsAffinitized())
    {
        GROUP_AFFINITY affinity{};
        affinity.Group = m_processor.Group;
        affinity.Mask = static_cast<ULONG_PTR>(1ull << m_processor.Number);

        EcThreadSetAffinity(
            &affinity,
            nullptr);
    }

    m_threadReady.Set();

    while (true)
    {
        m_knobs.CheckKnobsUpdated();

        m_threadRequested.Wait();

        if (m_exitThread)
        {
            break;
        }

        // Check if the worker thread priority was changed
        auto const threadPriority = m_knobs.GetKnobInt32(KNOB::WorkerThreadPriority);

        if (m_currentThreadPriority != threadPriority)
        {
            EcThreadSetPriority(
                m_thread,
                threadPriority);

            m_currentThreadPriority = threadPriority;
        }

        // Raise the IRQL to dispatch based on policy
        auto const flags = static_cast<EXECUTION_CONTEXT_FLAGS>(m_knobs.GetKnobInt32(KNOB::Flags));
        auto const runWorkerThreadAtDispatch = WI_IsFlagSet(flags, ExecutionContextFlagRunWorkerThreadAtDispatch);

        IrqlController irqlController{ ExecutionContext::RunContext::WorkerThread, runWorkerThreadAtDispatch };

        auto const irql = irqlController.GetIrql();

        if (irql == PASSIVE_LEVEL)
        {
            EnterDispatcherLockNotHeld<DispatcherContext::ThreadAtPassive>();
        }
        else
        {
            EnterDispatcherLockNotHeld<DispatcherContext::ThreadAtDispatch>();
        }
    }
}

_Use_decl_annotations_
void
Dispatcher::ChangeStateLockNotHeld(
    DispatcherState TargetState,
    DispatcherStateChangeReason Reason
)
{
    KAcquireSpinLock dispatcherLock{ m_lock };
    ChangeStateLockHeld(TargetState, Reason);
}

_Use_decl_annotations_
void
Dispatcher::ChangeStateLockHeld(
    DispatcherState TargetState,
    DispatcherStateChangeReason Reason
)
{
    switch (m_state)
    {
        case DispatcherState::Stopped:

            NT_FRE_ASSERT(
                TargetState == DispatcherState::WaitingDpc ||
                TargetState == DispatcherState::WaitingThread ||
                TargetState == DispatcherState::WaitingContextSteal);

            break;

        case DispatcherState::WaitingDpc:

            NT_FRE_ASSERT(TargetState == DispatcherState::Running);
            break;

        case DispatcherState::WaitingThread:

            NT_FRE_ASSERT(TargetState == DispatcherState::Running);
            break;

        case DispatcherState::WaitingContextSteal:

            NT_FRE_ASSERT(TargetState == DispatcherState::Running);
            break;

        case DispatcherState::Running:

            NT_FRE_ASSERT(
                TargetState == DispatcherState::WaitingDpc ||
                TargetState == DispatcherState::WaitingThread ||
                TargetState == DispatcherState::Stopped);

            break;
    }

    EventWriteDispatcherStateChange(
        reinterpret_cast<UINT64>(this),
        static_cast<UINT32>(Reason),
        static_cast<UINT32>(m_state),
        static_cast<UINT32>(TargetState));

    const UINT32 counterIndex = static_cast<UINT32>(Reason);

    if (counterIndex < ARRAYSIZE(m_counters.StateChangeReason.AsArray))
    {
        m_counters.StateChangeReason.AsArray[counterIndex]++;
    }

    m_state = TargetState;
}

_Use_decl_annotations_
bool
Dispatcher::InsertWorker(
    DispatcherWorker * Worker
)
{
    const KIRQL originalIrql = KeGetCurrentIrql();
    DispatcherState state = static_cast<DispatcherState>(-1);

    {
        KAcquireSpinLock workerLock{ Worker->Lock };

        if (Worker->State.RunningDown == 1)
        {
            // If running down it should not execute anymore
            return false;
        }

        if (Worker->State.Ready)
        {
            // Already in the ready queue
            return false;
        }

        if (Worker->State.Active && !Worker->State.Ready)
        {
            // Already being processed by a dispatcher. When it exits the same dispatcher
            // will requeue this worker
            NT_FRE_ASSERT(Worker->CurrentDispatcher != nullptr);
            Worker->State.Ready = 1;

            return true;
        }

        NT_FRE_ASSERT(
            Worker->State.Ready == 0 &&
            Worker->State.Active == 0);

        Worker->State.Ready = 1;
        Worker->CurrentDispatcher = this;

        {
            KAcquireSpinLock dispatcherLock{ m_lock };

            AddToReadyList(Worker);

            if (m_state == DispatcherState::Stopped)
            {
                PROCESSOR_NUMBER currentProcessor;
                KeGetCurrentProcessorNumberEx(&currentProcessor);

                if (
                    !IsAffinitized() ||
                    !WI_IsFlagSet(m_knobs.GetKnobInt32(KNOB::Flags), ExecutionContextFlagRunDpcForFirstLoop)
                )
                {
                    state = DispatcherState::WaitingThread;
                }
                else if (currentProcessor != m_processor || originalIrql != DISPATCH_LEVEL)
                {
                    state = DispatcherState::WaitingDpc;
                }
                else
                {
                    state = DispatcherState::WaitingContextSteal;
                }

                ChangeStateLockHeld(state, DispatcherStateChangeReason::InsertWorker);
            }
        }
    }

    switch (state)
    {
        case DispatcherState::WaitingDpc:
            m_dpc.InsertQueueDpc();
            break;

        case DispatcherState::WaitingThread:
            m_threadRequested.Set();
            break;

        case DispatcherState::WaitingContextSteal:
            EnterDispatcherLockNotHeld<DispatcherContext::ArbitraryAtDispatch>();
            break;
    }

    return true;
}

template <DispatcherContext Context>
constexpr
KIRQL
GetIrql()
{
    if constexpr (Context == DispatcherContext::Dpc)
    {
        return DISPATCH_LEVEL;
    }
    else if constexpr (Context == DispatcherContext::ThreadAtDispatch)
    {
        return DISPATCH_LEVEL;
    }
    else if constexpr (Context == DispatcherContext::ArbitraryAtDispatch)
    {
        return DISPATCH_LEVEL;
    }
    else if constexpr (Context == DispatcherContext::ThreadAtPassive)
    {
        return PASSIVE_LEVEL;
    }
    else
    {
        static_assert(false, "Invalid DispatcherContext");
    }
}

template <DispatcherContext Context>
WorkerExitStatus
InvokeWorkerEntry(
    _In_ DispatcherWorker * Worker
)
{
    bool irqlLowered = false;
    WorkerExitStatus exitStatus;

    if constexpr (Context == DispatcherContext::ThreadAtDispatch)
    {
        if (Worker->Flags.NeedsPassiveLevel)
        {
            KeLowerIrql(PASSIVE_LEVEL);
            irqlLowered = true;
        }
    }

    auto const irql = irqlLowered
        ? static_cast<KIRQL>(PASSIVE_LEVEL)
        : GetIrql<Context>();

    // This is to help the compiler inline these callbacks, while maitaining the dispatcher
    // as a generic library. It can be removed or expanded, it has no functional effect.
    if (Worker->WorkerEntry == PollWorkerEntry)
    {
        exitStatus = PollWorkerEntry(Worker, irql);
    }
    else if (Worker->WorkerEntry == TaskWorkerEntry)
    {
        exitStatus = TaskWorkerEntry(Worker, irql);
    }
    else
    {
        exitStatus = Worker->WorkerEntry(Worker, irql);
    }

    if (irqlLowered)
    {
        NT_FRE_ASSERT(PASSIVE_LEVEL == KeRaiseIrqlToDpcLevel());
    }

    return exitStatus;
}

template <DispatcherContext Context>
DispatcherStateChangeReason
GetChangeReason()
{
    if constexpr (Context == DispatcherContext::Dpc)
    {
        return DispatcherStateChangeReason::DpcScheduled;
    }
    else if constexpr (Context == DispatcherContext::ThreadAtPassive)
    {
        return DispatcherStateChangeReason::ThreadScheduled;
    }
    else if constexpr (Context == DispatcherContext::ThreadAtDispatch)
    {
        return DispatcherStateChangeReason::ThreadScheduled;
    }
    else if constexpr (Context == DispatcherContext::ArbitraryAtDispatch)
    {
        return DispatcherStateChangeReason::ContextSteal;
    }
    else
    {
        static_assert(false, "Invalid DispatcherContext");
    }
}

template <DispatcherContext Context>
void
Dispatcher::EnterDispatcherLockNotHeld(
    void
)
{
    m_knobs.CheckKnobsUpdated();

    auto const maxTimeInDispatch = m_knobs.GetKnobInt32(KNOB::MaxTimeAtDispatch);
    auto const flags = m_knobs.GetKnobInt32(KNOB::Flags);
    auto const dpcWatchdogThreshold = m_knobs.GetKnobInt32(KNOB::DpcWatchdogTimerThreshold);

    TimeBudget timeBudget
    {
        maxTimeInDispatch,
        WI_IsFlagSet(flags, ExecutionContextFlagTryExtendMaxTimeAtDispatch),
        dpcWatchdogThreshold,
        GetIrql<Context>()
    };

    timeBudget.Reset();

    auto const dispatchTimeWarning = m_knobs.GetKnobInt32(KNOB::DispatchTimeWarning);
    auto const dispatchTimeWarningInterval = m_knobs.GetKnobInt32(KNOB::DispatchTimeWarningInterval);

    TimeBudgetWarningLogger warningLogger
    {
        GUID(),
        dispatchTimeWarningInterval,
        dispatchTimeWarning,
        m_name.get()
    };

    size_t iterationCounter = 0;

    KAcquireSpinLock dispatcherLock{ m_lock };

    ChangeStateLockHeld(DispatcherState::Running, GetChangeReason<Context>());

    do
    {
        dispatcherLock.Release();
        DrainReadyList();

        // Simple round robin time slicing for workers.
        DispatcherWorker *worker;

        while ((worker = m_activeList.MoveNext()) != nullptr)
        {
            iterationCounter++;

            if constexpr (Context == DispatcherContext::ArbitraryAtDispatch)
            {
                // If we are stealing someone's context make sure we don't run for too long
                if (iterationCounter > 5)
                {
                    ChangeStateLockNotHeld(DispatcherState::WaitingDpc, DispatcherStateChangeReason::ArbitraryDpcBudgetExceeded);
                    m_dpc.InsertQueueDpc();
                    return;
                }
            }

            if constexpr (Context == DispatcherContext::Dpc || Context == DispatcherContext::ArbitraryAtDispatch || Context == DispatcherContext::ThreadAtDispatch)
            {
                // When running at DISPATCH_LEVEL we need to make sure we are within our DPC budget policies
                if (timeBudget.IsOutOfBudget())
                {
                    ChangeStateLockNotHeld(DispatcherState::WaitingThread, DispatcherStateChangeReason::DpcBudgetExceeded);
                    m_threadRequested.Set();
                    return;
                }
            }

            if (worker->TargetDispatcher != this)
            {
                // The current worker target dispatcher has changed, we need to remove it from this dispatcher's active list
                // and transfer to the target dispatcher's ready list
                KAcquireSpinLock workerLock{ worker->Lock };

                NT_FRE_ASSERT(worker->State.Active == 1);
                worker->State.Active = 0;
                worker->CurrentDispatcher = nullptr;

                // Since we are removing from the active list we don't need to acquire
                // the dispatcher lock
                m_activeList.RemoveEntry(worker);

                // If this worker is running down avoid adding it again to the ready list.
                if (worker->State.RunningDown == 1)
                {
                    NT_FRE_ASSERT(!worker->RundownComlete.TestNP());
                    worker->RundownComlete.Set();
                    InterlockedDecrement(&m_rundownCounter);
                }
                else
                {
                    // Before adding the the target dispatcher's ready list we must ensure this worker state is correct
                    worker->State.Ready = 0;
                    workerLock.Release();
                    worker->TargetDispatcher->InsertWorker(worker);
                }

                // This worker was moved to a different dispatcher, continue execution from the next worker
                continue;
            }

            // If we are not running from the worker thread we must check if this worker needs to run at passive level
            if constexpr (Context == DispatcherContext::Dpc || Context == DispatcherContext::ArbitraryAtDispatch)
            {
                if (worker->Flags.NeedsPassiveLevel)
                {
                    ChangeStateLockNotHeld(DispatcherState::WaitingThread, DispatcherStateChangeReason::WorkerNeedsPassiveLevel);
                    m_threadRequested.Set();
                    return;
                }
            }

            const auto exitStatus = InvokeWorkerEntry<Context>(worker);

            switch (exitStatus)
            {
                case WorkerExitStatus::Remove:
                {
                    KAcquireSpinLock workerLock{ worker->Lock };

                    NT_FRE_ASSERT(worker->State.Active == 1);
                    worker->State.Active = 0;
                    worker->CurrentDispatcher = nullptr;

                    // Since we are removing from the active list we don't need to acquire
                    // the dispatcher lock
                    m_activeList.RemoveEntry(worker);

                    // If this worker is running down avoid adding it again to the ready list.
                    if (worker->State.RunningDown == 1)
                    {
                        NT_FRE_ASSERT(!worker->RundownComlete.TestNP());
                        worker->RundownComlete.Set();
                        InterlockedDecrement(&m_rundownCounter);

                        break;
                    }

                    // Is the worker ready again?
                    if (worker->State.Ready == 1)
                    {
                        worker->CurrentDispatcher = this;

                        dispatcherLock.Acquire();
                        AddToReadyList(worker);
                        dispatcherLock.Release();
                    }

                    break;
                }
                case WorkerExitStatus::Yield:
                {
                    // Nothing else to do
                    break;
                }
            }

            if constexpr (Context == DispatcherContext::Dpc || Context == DispatcherContext::ArbitraryAtDispatch || Context == DispatcherContext::ThreadAtDispatch)
            {
                warningLogger.LogWarningIfOvertime(timeBudget);
            }

            if (ReadNoFence(&m_rundownCounter) > 0)
            {
                RundownHelper();
            }

            if (m_knobs.GetKnobsUpdated())
            {
                if constexpr (
                    Context == DispatcherContext::Dpc ||
                    Context == DispatcherContext::ArbitraryAtDispatch
                )
                {
                    ChangeStateLockNotHeld(DispatcherState::WaitingDpc, DispatcherStateChangeReason::KnobsReload);
                    m_dpc.InsertQueueDpc();
                }
                else if constexpr (
                    Context == DispatcherContext::ThreadAtPassive ||
                    Context == DispatcherContext::ThreadAtDispatch
                )
                {
                    ChangeStateLockNotHeld(DispatcherState::WaitingThread, DispatcherStateChangeReason::KnobsReload);
                    m_threadRequested.Set();
                }
                else
                {
                    static_assert(false, "Invalid DispatcherContext");
                }
                return;
            }

            // Do a lock free check for new ready workers. We don't need to immediately observe new entries here.
            // If we do see entries we must acquire the dispatcher lock to process the list. For that we break
            // out of the inner loop.
            if (mem::ReadAcquire(&m_readyListDirty))
            {
                break;
            }
        }

        // To exit the outer loop we must ensure there are no new ready workers, for that we need to acquire the
        // dispatcher lock. If the ready list is empty we will stop on the last line of this function.
        dispatcherLock.Acquire();
    } while (!IsListEmpty(&m_readyList));

    ChangeStateLockHeld(DispatcherState::Stopped, DispatcherStateChangeReason::Idle);
}

_Use_decl_annotations_
void
Dispatcher::AddToReadyList(
    DispatcherWorker * Worker
)
{
    InsertTailList(&m_readyList, &Worker->Entry);
    mem::WriteRelease(&m_readyListDirty, 1L);

    LogReadyListInsertion(Worker);
}

_Use_decl_annotations_
void
Dispatcher::PopAllFromReadyList(
    LIST_ENTRY *List
)
{
    KAcquireSpinLock dispatcherLock{ m_lock };

    if (IsListEmpty(&m_readyList))
    {
        InitializeListHead(List);
    }
    else
    {
        *List = m_readyList;
        List->Flink->Blink = List;
        List->Blink->Flink = List;

        InitializeListHead(&m_readyList);
    }

    mem::WriteRelease(&m_readyListDirty, 0L);
}

_Use_decl_annotations_
void
Dispatcher::DrainReadyList(
    void
)
{
    LIST_ENTRY readyWorkers{};
    PopAllFromReadyList(&readyWorkers);

    LIST_ENTRY *entry = readyWorkers.Flink;

    while (entry != &readyWorkers)
    {
        // Cache the next entry before we move the worker to the active list, as that will
        // invalidate the list entry.
        LIST_ENTRY *nextEntry = entry->Flink;

        DispatcherWorker *worker = CONTAINING_RECORD(
            entry,
            DispatcherWorker,
            Entry);

        LogReadyListRemoval(worker);

        if (TryMoveWorkerToActiveList(worker))
        {
            // Only notify the worker it is about to run if the move to the active
            // list was successful.
            //
            // Note: It is very important we don't call the worker start callback
            // with any lock held, as that may cause deadlocks in case it decides
            // to call back into the dispatcher.
            worker->InvokeStart();
        }

        entry = nextEntry;
    }
}

_Use_decl_annotations_
bool
Dispatcher::TryMoveWorkerToActiveList(
    DispatcherWorker * Worker
)
{
    // This method always removes the worker from the ready list,
    // it then tries to add it to the active list, which may fail
    // if the worker is running down.
    // The return is true if the worker was added to the active list,
    // false otherwise.

    KAcquireSpinLock workerLock{ Worker->Lock };

    // Check if this worker is running down. If it is we won't add it to the active list.
    if (Worker->State.RunningDown)
    {
        NT_FRE_ASSERT(Worker->State.Ready);
        NT_FRE_ASSERT(!Worker->State.Active);

        Worker->State.Ready = 0;
        Worker->RundownComlete.Set();

        return false;
    }

    // Sanity check the worker state
    NT_FRE_ASSERT(Worker->CurrentDispatcher == this);

    NT_FRE_ASSERT(
        Worker->State.Ready == 1 &&
        Worker->State.Active == 0);

    Worker->State.Ready = 0;
    Worker->State.Active = 1;

    m_activeList.Insert(Worker);

    return true;
}

_Use_decl_annotations_
void
Dispatcher::RundownHelper(
    void
)
{
    m_activeList.Enumerate(
        [this](DispatcherWorker & Worker)
        {
             KAcquireSpinLock workerLock{ Worker.Lock };

            if (Worker.State.RunningDown == 1)
            {
                NT_FRE_ASSERT(Worker.State.Active == 1);
                Worker.State.Active = 0;
                Worker.CurrentDispatcher = nullptr;

                m_activeList.RemoveEntry(&Worker);

                NT_FRE_ASSERT(!Worker.RundownComlete.TestNP());
                Worker.RundownComlete.Set();
                NT_FRE_ASSERT(0 <= InterlockedDecrement(&m_rundownCounter));

                // We will process one rundown request at a time
                return false;
            }

            // Keep iterating
            return true;
        });
}

_Use_decl_annotations_
void
Dispatcher::RundownActiveWorker(
    DispatcherWorker * Worker
)
{
    // If the worker is **only** in the ready list we need to wait until the dispatcher removes it from its internal list
    if (Worker->State.Ready == 1 && Worker->State.Active == 0)
    {
        NT_FRE_ASSERT(Worker->CurrentDispatcher != nullptr);
        return;
    }

    NT_FRE_ASSERT(Worker->State.Active == 1);

    // If the object is ready and active we need to do two things:
    //
    //     1) Flip the ready bit to 0 so that the dispatcher does not requeue this object
    //     2) Wait until the dispatcher removes this object from its active list.

    if (Worker->State.Ready == 1)
    {
        Worker->State.Ready = 0;
        LogReadyListRemoval(Worker);
    }

    // This will give a hint to the dispatcher a rundown is happening, causing it to speed
    // up the removal of this object from the active list. The dispatcher will signal
    // RundownComplete when it removes the worker from the active list.
    InterlockedIncrement(&m_rundownCounter);
}

void
Dispatcher::LogReadyListInsertion(
    DispatcherWorker const * Worker
)
{
    if (Worker->Priority < ARRAYSIZE(m_counters.ReadyList.AsArray))
    {
        InterlockedIncrement64(
            reinterpret_cast<LONG64 volatile *>(&m_counters.ReadyList.AsArray[Worker->Priority]));
    }

    EventWriteDispatcherInsertReadyList(
        reinterpret_cast<UINT64>(this),
        reinterpret_cast<UINT64>(Worker),
        Worker->DebugData,
        Worker->Priority);
}

void
Dispatcher::LogReadyListRemoval(
    DispatcherWorker const * Worker
)
{
    if (Worker->Priority < ARRAYSIZE(m_counters.ReadyList.AsArray))
    {
        InterlockedDecrement64(
            reinterpret_cast<LONG64 volatile *>(&m_counters.ReadyList.AsArray[Worker->Priority]));
    }

    EventWriteDispatcherRemoveReadyList(
        reinterpret_cast<UINT64>(this),
        reinterpret_cast<UINT64>(Worker),
        Worker->DebugData,
        Worker->Priority);
}
