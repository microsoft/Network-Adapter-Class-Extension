// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>
#include <kspinlock.h>
#include <kwaitevent.h>
#include <kptr.h>
#include <Limits.h>
#include "ActiveList.hpp"
#include "DispatcherWorker.hpp"
#include "Dpc.h"
#include <ExecutionContext.h>
#include "TimeBudgetWarningLogger.h"
#include "Thread.hpp"
#include "IKnobs.hpp"

enum class DispatcherContext
{
    Dpc = 1,
    ThreadAtPassive,
    ThreadAtDispatch,
    ArbitraryAtDispatch,
};

enum class DispatcherState
{
    Stopped = 1,
    WaitingDpc,
    WaitingThread,
    WaitingContextSteal,
    Running,
};

enum class DispatcherStateChangeReason : UINT32
{
    InsertWorker = 0,
    DpcScheduled,
    ThreadScheduled,
    ContextSteal,
    WorkerNeedsPassiveLevel,
    DpcBudgetExceeded,
    ArbitraryDpcBudgetExceeded,
    Idle,
    KnobsReload,
    Max
};

#include "DispatcherCounters.hpp"

class Dispatcher : public NONPAGED_OBJECT<'rcpD'>
{
    friend static EC_START_ROUTINE
        DispatcherThread;

#ifdef TEST_HARNESS_CLASS_NAME
    friend class
        TEST_HARNESS_CLASS_NAME;
#endif

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    Dispatcher(
        IKnobs & EcKnobs
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    Dispatcher(
        PROCESSOR_NUMBER const & Processor,
        IKnobs & EcKnobs
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize();

    _IRQL_requires_(PASSIVE_LEVEL)
    ~Dispatcher();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    InsertWorker(
        _In_ DispatcherWorker * Worker
    );

    _IRQL_requires_(DISPATCH_LEVEL)
    _Requires_lock_held_(Worker->Lock)
    void
    RundownActiveWorker(
        _In_ DispatcherWorker * Worker
    );

private:

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    InitializeName(
        void
    );

    _IRQL_requires_(DISPATCH_LEVEL)
    void
    DpcRoutine();

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    ThreadRoutine();

    bool
    IsAffinitized() const;

    _Requires_lock_held_(m_lock)
    void
    ChangeStateLockHeld(
        DispatcherState TargetState,
        DispatcherStateChangeReason Reason
    );

    _Requires_lock_not_held_(m_lock)
    void
    ChangeStateLockNotHeld(
        DispatcherState TargetState,
        DispatcherStateChangeReason Reason
    );

    template <DispatcherContext Context>
    _Requires_lock_not_held_(m_lock)
    void
    EnterDispatcherLockNotHeld(
        void
    );

    _Requires_lock_held_(m_lock)
    void
    AddToReadyList(
        _In_ DispatcherWorker * Worker
    );

    _Requires_lock_not_held_(m_lock)
    void
    PopAllFromReadyList(
        _Out_ LIST_ENTRY *List
    );

    _Requires_lock_not_held_(m_lock)
    void
    DrainReadyList(
        void
    );

    _Requires_lock_not_held_(m_lock)
    _Requires_lock_not_held_(Worker->Lock)
    bool
    TryMoveWorkerToActiveList(
        _Inout_ DispatcherWorker * Worker
    );

    _Requires_lock_not_held_(m_lock)
    void
    RundownHelper(
        void
    );

    void
    LogReadyListInsertion(
        _In_ DispatcherWorker const * Worker
    );

    void
    LogReadyListRemoval(
        _In_ DispatcherWorker const * Worker
    );

private:

    // Use MAXIMUM_PROC_PER_GROUP to differentiate between when no
    // affinity was set and when the affinity was set to processor 0
    static_assert(MAXIMUM_PROC_PER_GROUP <= UCHAR_MAX);

    PROCESSOR_NUMBER
        m_processor{ 0, MAXIMUM_PROC_PER_GROUP, 0 };

    KPoolPtrNP<UNICODE_STRING>
        m_name;

    DispatcherCounters
        m_counters{};

    KSpinLock
        m_lock;

    _Guarded_by_(m_lock)
    DispatcherState
        m_state{ DispatcherState::Stopped };

    // DPC used to run dispatcher logic at DISPATCH_LEVEL
    Dpc<Dispatcher, &Dispatcher::DpcRoutine>
        m_dpc;

    // Current worker thread priority. This might difer from the value in the knobs field until we
    // have a chance to adjust the worker thread priority.
    UINT32
        m_currentThreadPriority{ 0 };

    // Thread used to run dispatcher logic at PASSIVE_LEVEL
    unique_thread
        m_thread;

    // Used only during initialization. This is signaled when our worker thread is done initializing
    KAutoEvent
        m_threadReady;

    // Used to signal our worker thread for execution
    KAutoEvent
        m_threadRequested;

    // Used during destruction to tell our worker thread it should exit
    bool
        m_exitThread{ false };

    // Number of dispatch objects currently going through a rundown. This helps us speed
    // up those rundowns by tracking down their dispatch contexts and removing them from
    // the active list
    _Interlocked_
    volatile LONG
        m_rundownCounter{ 0 };

    // This holds the list of workers we need to start calling. It can be modified outside of
    // the dispatcher context, so we must always acquire the dispatcher lock
    _Guarded_by_(m_lock)
    LIST_ENTRY
        m_readyList;

    volatile LONG
        m_readyListDirty{ 0 };

    // This holds the list of workers we are currently polling. It can only be modified from
    // inside the dispatcher context, so we do not need to acquire any locks
    ActiveList
        m_activeList{ this, m_counters };

    IKnobs &
        m_knobs;
};
