// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <wil/resource.h>
#include <knew.h>
#include <KMacros.h>
#include <KSpinLock.h>
#include <Histogram.h>
#include <kwaitevent.h>
#include <limits.h>

#include "ActivityTracker.hpp"
#include "Thread.hpp"
#include <ExecutionContextPoll.h>
#include <ExecutionContextNotification.h>
#include <ExecutionContextTask.h>
#include "ExecutionContextTask.hpp"
#include <ExecutionContextDispatch.h>
#include "Dpc.h"
#include "TimeBudget.h"
#include "TimeBudgetWarningLogger.h"

extern GUID g_staticEcClientIdentier;
extern EXECUTION_CONTEXT_RUNTIME_KNOBS g_staticEcKnobs;

_IRQL_requires_max_(PASSIVE_LEVEL)
NET_EXECUTION_CONTEXT_DISPATCH const *
GetExecutionContextDispatch(
    void
);

enum class ExecutionContextEvent
{
    Notify = 1,
    QueueTask,
    QueueRegisterPollTask,
    QueueUnregisterPollTask,
    QueueChangePollFunctionTask,
    QueueRegisterNotificationTask,
    QueueUnregisterNotificationTask,
    QueueSetWorkerThreadAffinityTask,
    QueueTerminationTask,
    QueueUnregisterEverything,
};

// Keep these values in sync with ETW manifest: <valueMap name="ExecutionContextState">
enum class ExecutionContextState
{
    Stopped = 1,
    WaitingWorkerThread,
    WaitingWorkerDpc,
    Running,
};

// Keep these values in sync with ETW manifest: <valueMap name="ExecutionContextStateChangeReason">
enum class ExecutionContextStateChangeReason
{
    NotifyDpcScheduledWhileStopped = 1,
    NotifyNeedsDpc,
    NotifyNeedsWorkerThread,
    NotifyWhileExitingWorker,
    TaskQueuedWhileStopped,
    TaskQueuedWhileRunningFromDpc,
    TaskQueuedWhileExitingWorker,
    TaskPreemptedPolling,
    WorkerDpcScheduled,
    WorkerThreadScheduled,
    NoMoreWork,
    OutOfDpcBudget,
    TerminateWorkerThread,
};

class ExecutionContext : public NONPAGED_OBJECT<'xtCE'>
{
    friend class
        ExecutionContextNotification;
#ifdef TEST_HARNESS_CLASS_NAME
    friend class
        TEST_HARNESS_CLASS_NAME;
#endif

    static
    EVT_EXECUTION_CONTEXT_TASK
        EvtSetWorkerThreadAffinity;

    static
    EVT_EXECUTION_CONTEXT_TASK
        EvtTerminateWorkerThread;

public:

    static
    void
    InitializeSubsystem(
        void
    );

    static
    void
    UninitializeSubsystem(
        void
    );

public:

    enum class NotifyContext
    {
        NotifyDpc = 1,
        ArbitraryContext,
    };

    enum class RunContext
    {
        NotifyDpc = 1,
        WorkerDpc,
        WorkerThread,
    };

    enum class PollExitReason
    {
        NoMoreWork = 1,
        OutOfDpcBudget,
        TaskPending,
    };

public:

    static
    PAGEDX
    EC_START_ROUTINE
        Thread;

    _IRQL_requires_(PASSIVE_LEVEL)
    _No_competing_thread_
    PAGEDX
    ExecutionContext(
        _In_ GUID const * ClientIdentifier,
        _In_ EXECUTION_CONTEXT_RUNTIME_KNOBS const * RuntimeMutableKnobs
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    virtual
    ~ExecutionContext(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    _No_competing_thread_
    PAGEDX
    NTSTATUS
    Initialize(
        _In_opt_ UNICODE_STRING const * clientFriendlyName = nullptr
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    RegisterPoll(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    UnregisterPoll(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    ChangePollFunction(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll,
        _In_ PFN_EXECUTION_CONTEXT_POLL PollFn
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    RegisterNotification(
        _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    UnregisterNotification(
        _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    EC_THREAD_PRIORITY
    GetWorkerThreadPriority(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetWorkerThreadAffinity(
        _In_ const GROUP_AFFINITY & GroupAffinity
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SetDpcTargetProcessor(
        _In_ const PROCESSOR_NUMBER & GroupAffinity
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    QueueTask(
        _In_ ExecutionContextTask & Task
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    QueueTask(
        _In_ ExecutionContextTask & Task,
        _In_ ExecutionContextEvent Event
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetWorkerThreadDescription(
        _In_ const UNICODE_STRING & Description
    );

    _IRQL_requires_max_(HIGH_LEVEL)
    void
    Notify(
        void
    );

private:

    _IRQL_requires_(PASSIVE_LEVEL)
    _No_competing_thread_
    PAGEDX
    NTSTATUS
    InitializeFriendlyName(
        _In_opt_ UNICODE_STRING const * FriendlyName
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_not_held_(m_lock)
    void
    ChangeStateLockNotHeld(
        _In_ ExecutionContextStateChangeReason Reason,
        _In_ ExecutionContextState TargetState
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_held_(m_lock)
    void
    ChangeStateLockHeld(
        _In_ ExecutionContextStateChangeReason Reason,
        _In_ ExecutionContextState TargetState
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Destroy(
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_held_(m_lock)
    void
    ProcessTaskQueue(
        _In_ KAcquireSpinLock & Lock
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    EnableNotifications(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    DisableNotifications(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT64
    PollClients(
        _In_ KIRQL Irql
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PollExitReason
    Poll(
        _In_ KIRQL Irql
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    WorkerThreadRoutine(
        void
    );

    _IRQL_requires_(DISPATCH_LEVEL)
    void
    PollDpc(
        void
    );

    _IRQL_requires_(DISPATCH_LEVEL)
    void
    NotifyDpc(
        void
    );

    _IRQL_requires_max_(HIGH_LEVEL)
    bool
    ShouldRunInDpc(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    static
    void
    NotifyDpcRoutine(
        _In_ ExecutionContext * ExecutionContext
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    static
    void
    PollDpcRoutine(
        _In_ ExecutionContext * ExecutionContext
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_not_held_(m_lock)
    void
    NotifyCommon(
        _In_ NotifyContext Context
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_not_held_(m_lock)
    void
    RunLockNotHeld(
        _In_ RunContext Context
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_held_(m_lock)
    void
    RunLockHeld(
        _In_ KAcquireSpinLock & Lock,
        _In_ RunContext Context
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_held_(m_lock)
    void
    QueueWorkerThread(
        _In_ ExecutionContextStateChangeReason Reason
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_held_(m_lock)
    void
    QueueWorkerDpc(
        _In_ ExecutionContextStateChangeReason Reason
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetWorkerThreadAffinityTask(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    TerminateWorkerThreadTask(
        void
    );

private:

    const GUID
        m_clientIdentifier;

    KPoolPtr<UNICODE_STRING>
        m_clientFriendlyName;

    unique_thread
        m_workerThreadObject;

    EC_THREAD
        m_pollingThread = EC_THREAD_INVALID;

    _Guarded_by_(m_lock)
    ExecutionContextState
        m_state = ExecutionContextState::Stopped;

    _Guarded_by_(m_lock)
    ActivityTracker
        m_activityTracker;

    _Guarded_by_(m_lock)
    bool
        m_hasPollWork = false;

    bool
        m_terminateWorkerThread = false;

    KAutoEvent
        m_workerThreadReady;

    KAutoEvent
        m_workerThreadRequested;

    KSpinLock
        m_lock;

    _Guarded_by_(m_lock)
    LIST_ENTRY
        m_taskQueue;

    LIST_ENTRY
        m_pollList;

    LIST_ENTRY
        m_notificationList;

    GROUP_AFFINITY
        m_workerThreadAffinity = {};

    GROUP_AFFINITY
        m_originalWorkerThreadAffinity = {};

    bool
        m_affinityChanged = false;

    // Use MAXIMUM_PROC_PER_GROUP to differentiate between when no
    // affinity was set and when the affinity was set to processor 0
    static_assert(MAXIMUM_PROC_PER_GROUP <= UCHAR_MAX);

    PROCESSOR_NUMBER
        m_dpcAffinity = { 0, MAXIMUM_PROC_PER_GROUP, 0 };

    EcDpc<ExecutionContext>
        m_notifyDpc;

    EcDpc<ExecutionContext>
        m_pollDpc;

    Statistics::Histogram
        m_dpcHistogram;

    EXECUTION_CONTEXT_RUNTIME_KNOBS const * const
        m_runtimeKnobs;

    TimeBudgetWarningLogger
        m_warningLogger;

    ExecutionContextTask
        m_setWorkerThreadAffinityTask;

    ExecutionContextTask
        m_terminateWorkerThreadTask;
};
