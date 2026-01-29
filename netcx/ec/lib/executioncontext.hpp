// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <wil/resource.h>
#include <knew.h>
#include <KMacros.h>
#include <KSpinLock.h>
#include <Histogram.h>
#include <kwaitevent.h>
#include <karray.h>
#include <limits.h>
#include <IExecutionContext.hpp>

#include "ActivityTracker.hpp"
#include "Thread.hpp"
#include <ExecutionContextPoll.h>
#include <ExecutionContextNotification.h>
#include <ExecutionContextTask.h>
#include <KWaitTask.hpp>
#include <ExecutionContextDispatch.h>
#include "Dpc.h"
#include "TimeBudget.h"
#include "TimeBudgetWarningLogger.h"
#include "Dispatcher.hpp"
#include "DispatcherWorker.hpp"
#include "PassiveLevelKnobs.hpp"

extern GUID g_staticEcClientIdentier;

DispatcherWorkerStart
    PollWorkerStart;

DispatcherWorkerStop
    PollWorkerStop;

DispatcherWorkerEntry
    PollWorkerEntry;

DispatcherWorkerEntry
    TaskWorkerEntry;

DispatcherWorkerEntry
    SetAffinityWorker;

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
    WaitingDispatcher,
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
    NotifyContextStealing,
    PollWorkerEntry,
    PollWorkerExit,
};

class ExecutionContext
    : public NONPAGED_OBJECT<'xtCE'>
    , public IExecutionContext
{
    friend class
        ExecutionContextNotification;

#ifdef TEST_HARNESS_CLASS_NAME
    friend class
        TEST_HARNESS_CLASS_NAME;
#endif

    friend class
        Dispatcher;

    friend DispatcherWorkerStart
        PollWorkerStart;

    friend DispatcherWorkerStop
        PollWorkerStop;

    friend DispatcherWorkerEntry
        PollWorkerEntry;

    friend DispatcherWorkerEntry
        TaskWorkerEntry;

    friend DispatcherWorkerEntry
        SetAffinityWorker;

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    static
    void
    InitializeSubsystem(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
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
        ArbitraryContextAtDispatch,
        ArbitraryContextAtApc,
        ArbitraryContextAtPassive,
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
        _In_ GUID const* ClientIdentifier,
        _In_ wistd::unique_ptr<Dispatcher> dispatcher,
        _In_ KPoolPtr<UNICODE_STRING> clientFriendlyName
    ) noexcept;

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    virtual
    ~ExecutionContext(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    static
    NTSTATUS
    Create(
        _In_opt_ UNICODE_STRING const* ClientFriendlyName,
        _In_opt_ GUID const* ClientIdentifier,
        _Out_ wistd::unique_ptr<ExecutionContext>& executionContext
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

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SetAffinity(
        _In_ const PROCESSOR_NUMBER & Processor
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    QueueTask(
        _In_ EXECUTION_CONTEXT_TASK * Task
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    QueueTask(
        _In_ EXECUTION_CONTEXT_TASK * Task,
        _In_ ExecutionContextEvent Event
    );

    _IRQL_requires_max_(HIGH_LEVEL)
    void
    Notify(
        void
    );

    _IRQL_requires_max_(HIGH_LEVEL)
    void
    NotifyEx(
        _In_ EXECUTION_CONTEXT_NOTIFY_PARAMETERS * Parameters
    );

private:

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
        void
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

    _IRQL_requires_(DISPATCH_LEVEL)
    void
    NotifyDpc(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _Requires_lock_not_held_(m_lock)
    void
    NotifyCommon(
        _In_ NotifyContext Context,
        _In_ EXECUTION_CONTEXT_NOTIFY_PARAMETERS * Parameters
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    WorkerExitStatus
    TaskWorker(KIRQL Irql);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    PollStart();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    PollStop();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    WorkerExitStatus
    PollWorker(KIRQL Irql);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SetAffinityWorkerInner(
        void
    );

private:

    const GUID
        m_clientIdentifier;

    KPoolPtr<UNICODE_STRING>
        m_clientFriendlyName;

    _Guarded_by_(m_lock)
    ExecutionContextState
        m_state = ExecutionContextState::Stopped;

    _Guarded_by_(m_lock)
    ActivityTracker
        m_activityTracker;

    KSpinLock
        m_lock;

    _Guarded_by_(m_lock)
    LIST_ENTRY
        m_taskQueue;

    LIST_ENTRY
        m_pollList;

    LIST_ENTRY
        m_notificationList;

    bool
        m_armed{ false };

    // Use MAXIMUM_PROC_PER_GROUP to differentiate between when no
    // affinity was set and when the affinity was set to processor 0
    static_assert(MAXIMUM_PROC_PER_GROUP <= UCHAR_MAX);

    PROCESSOR_NUMBER
        m_affinity = { 0, MAXIMUM_PROC_PER_GROUP, 0 };

    Dpc<ExecutionContext, &ExecutionContext::NotifyDpc>
        m_notifyDpc;

    wistd::unique_ptr<Dispatcher>
        m_dispatcher;

    DispatcherWorker
        m_taskWorker;

    DispatcherWorker
        m_pollWorker;

    DispatcherWorker
        m_setAffinityWorker;
};
