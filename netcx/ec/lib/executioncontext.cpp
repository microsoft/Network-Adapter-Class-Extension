// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include <KString.h>
#include <ntstrsafe.h>

#include "ExecutionContext.hpp"
#include "ExecutionContextStatistics.hpp"
#include "HistogramTimer.h"
#include "HistogramHub.h"
#include "IrqlController.h"
#include "NotificationReserved.hpp"
#include "PollReserved.hpp"
#include "KMemoryAccess.h"
#include "EcEvents.h"

GUID g_staticEcClientIdentier = { 0 };

EXECUTION_CONTEXT_RUNTIME_KNOBS g_staticEcKnobs = {
    /*.Size = */                    sizeof(g_staticEcKnobs),
    /*.Flags = */                   ExecutionContextFlagRunDpcForFirstLoop,
    /*.MaxTimeAtDispatch = */       0,
    /*.DispatchTimeWarning = */     0,
    /*.DpcWatchdogTimerThreshold =*/80,
    /*.WorkerThreadPriority = */    8,
    /*.MaxPacketsSend = */          64,
    /*.MaxPacketsSendComplete = */  64,
    /*.MaxPacketsReceive = */       64,
    /*.MaxPacketsReceiveComplete =*/64
};

DECLARE_CONST_UNICODE_STRING(
    DefaultFriendlyName,
    L"Execution Context");

_Use_decl_annotations_
void
ExecutionContext::InitializeSubsystem(
    void
)
{
    EventRegisterMicrosoft_Windows_Network_ExecutionContext();
}

_Use_decl_annotations_
void
ExecutionContext::UninitializeSubsystem(
    void
)
{
    EventUnregisterMicrosoft_Windows_Network_ExecutionContext();
}

static
void
UnregisterPollTaskCallback(
    _In_ void * TaskContext
);

static
void
UnregisterNotificationTaskCallback(
    _In_ void * TaskContext
);

_Use_decl_annotations_
PAGEDX
ExecutionContext::ExecutionContext(
    _In_ GUID const * ClientIdentifier,
    _In_ EXECUTION_CONTEXT_RUNTIME_KNOBS const * RuntimeMutableKnobs
)
    : m_clientIdentifier(*ClientIdentifier)
    , m_activityTracker(*ClientIdentifier)
    , m_notifyDpc(NotifyDpcRoutine, this)
    , m_pollDpc(PollDpcRoutine, this)
    , m_runtimeKnobs(RuntimeMutableKnobs)
    , m_warningLogger(*ClientIdentifier, RuntimeMutableKnobs->DispatchTimeWarningInterval)
    , m_setWorkerThreadAffinityTask(this, EvtSetWorkerThreadAffinity, false)
    , m_terminateWorkerThreadTask(this, EvtTerminateWorkerThread, false)
{
    PAGED_CODE();
    InitializeGlobalEcStatistic();
    GetGlobalEcStatistics().AddHistogramReference(m_dpcHistogram);
    DECLARE_CONST_UNICODE_STRING(dpcHistogramName, L"DpcDuration");
    m_dpcHistogram.SetName(dpcHistogramName);

    auto const dispatchTimeWarning = mem::ReadNoFence(&m_runtimeKnobs->DispatchTimeWarning);
    m_warningLogger.SetWatermark(dispatchTimeWarning);

    NT_FRE_ASSERT(m_runtimeKnobs);

    InitializeListHead(&m_taskQueue);
    InitializeListHead(&m_pollList);
    InitializeListHead(&m_notificationList);
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEDX
ExecutionContext::~ExecutionContext(
    void
)
{
    PAGED_CODE();
    GetGlobalEcStatistics().RemoveHistogramReference(m_dpcHistogram);

    Destroy();

    NT_FRE_ASSERT(!m_workerThreadRequested.Test());
    NT_FRE_ASSERT(IsListEmpty(&m_taskQueue));
}

_Use_decl_annotations_
PAGEDX
NTSTATUS
ExecutionContext::InitializeFriendlyName(
    UNICODE_STRING const * FriendlyName
)
{
    PAGED_CODE();

    auto const & sourceString = FriendlyName != nullptr && FriendlyName->Length > 0
        ? *FriendlyName
        : DefaultFriendlyName;

    m_clientFriendlyName = Rtl::DuplicateUnicodeString(
        sourceString,
        'aNCE');

    if (!m_clientFriendlyName)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGEDX
NTSTATUS
ExecutionContext::Initialize(
    UNICODE_STRING const * clientFriendlyName
)
{
    PAGED_CODE();

    auto ntStatus = InitializeFriendlyName(clientFriendlyName);
    m_warningLogger.SetFriendlyName(m_clientFriendlyName.get());
    m_activityTracker.Initialize(m_clientFriendlyName.get());

    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    ntStatus = EcThreadCreate(
        ExecutionContext::Thread,
        this,
        m_workerThreadObject);

    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    EcThreadSetPriority(m_workerThreadObject, m_runtimeKnobs->WorkerThreadPriority);
    SetWorkerThreadDescription(*m_clientFriendlyName);

    m_workerThreadReady.Wait();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
ExecutionContext::Destroy(
)
{
    // This is a scenario when EC has failed to initialize
    if (!m_workerThreadObject)
    {
        return;
    }

    {
        KAcquireSpinLock lock{ m_lock };
        if (m_affinityChanged)
        {
            m_workerThreadAffinity = m_originalWorkerThreadAffinity;
            lock.Release();
            QueueTask(m_setWorkerThreadAffinityTask, ExecutionContextEvent::QueueSetWorkerThreadAffinityTask);
        }
    }

    struct
    {
        LIST_ENTRY *notificationsList;
        LIST_ENTRY *pollList;
    } localContext;

    auto unregisterEverything = [](void * Context)
    {
        auto context = static_cast<decltype(localContext) *>(Context);

        auto pollList = static_cast<LIST_ENTRY *>(context->pollList);
        auto nextLink = pollList->Flink;
        while (nextLink != pollList)
        {
            auto currentLink = nextLink;
            nextLink = nextLink->Flink;
            UnregisterPollTaskCallback(currentLink);
        }

        auto notificationsList = static_cast<LIST_ENTRY *>(context->notificationsList);
        nextLink = notificationsList->Flink;
        while (nextLink != notificationsList)
        {
            auto currentLink = nextLink;
            nextLink = nextLink->Flink;
            UnregisterNotificationTaskCallback(currentLink);
        }
    };

    localContext.notificationsList = &m_notificationList;
    localContext.pollList = &m_pollList;

    ExecutionContextTask unregisterEverythingTask { &localContext, unregisterEverything };
    QueueTask(unregisterEverythingTask, ExecutionContextEvent::QueueUnregisterEverything);
    QueueTask(m_terminateWorkerThreadTask, ExecutionContextEvent::QueueTerminationTask);

    // we don't have to wait for other tasks explicitly, because EC gurantees first come - first serve serialization
    EcThreadWaitForTermination(m_workerThreadObject);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
static SetNameForHistogram(
    _In_ Statistics::Histogram * histogram,
    _In_ const GUID & ecIdentifer,
    _In_ UNICODE_STRING const * ecFriendlyName,
    _In_ ULONG pollTag
)
{
    UNICODE_STRING guidStr = {0};
    DECLARE_UNICODE_STRING_SIZE(histogramName, Statistics::HIST_NAME_SIZE);

    auto status = RtlStringFromGUID(ecIdentifer, &guidStr);

    if (NT_SUCCESS(status))
    {
        status = RtlUnicodeStringPrintf(&histogramName, L"%.4S:%wZ-%wZ", &pollTag, &guidStr, ecFriendlyName);
        NT_FRE_ASSERT(status != STATUS_INVALID_PARAMETER);
        histogram->SetName(histogramName);
    }
    else
    {
        DECLARE_CONST_UNICODE_STRING(errorName, L"NameGenerationError");
        histogram->SetName(errorName);
    }

    RtlFreeUnicodeString(&guidStr);
}

_Use_decl_annotations_
PAGEDX
void
ExecutionContext::RegisterPoll(
    EXECUTION_CONTEXT_POLL * Poll
)
{
    PAGED_CODE();

    NT_FRE_ASSERT(IsListEmpty(&Poll->Link));

    // Make sure adding the new poll to the list is synchronized by the EC, to do this we use a task
    struct
    {
        ExecutionContext* Ec;
        EXECUTION_CONTEXT_POLL* EcPoll;
    } addContext;

    auto taskFn = [](_In_ void * Context)
    {
        auto context = static_cast<decltype(addContext) *>(Context);
        auto entry = context->Ec->m_pollList.Flink;

        // Insert the new poll sorted by type
        while (entry != &context->Ec->m_pollList)
        {
            auto const type = CONTAINING_RECORD(entry, EXECUTION_CONTEXT_POLL, Link)->Type;

            if (context->EcPoll->Type < type)
            {
                break;
            }

            entry = entry->Flink;
        }

        auto const pollToAdd = CONTAINING_RECORD(&context->EcPoll->Link, EXECUTION_CONTEXT_POLL, Link);
        auto &pollContext = ContextFromPoll(pollToAdd);
        NT_FRE_ASSERT(pollContext.histogram.get() == nullptr);

        pollContext.histogram.reset(new(std::nothrow)Statistics::Histogram());
        if (pollContext.histogram)
        {
            SetNameForHistogram(
                pollContext.histogram.get(),
                context->Ec->m_clientIdentifier,
                context->Ec->m_clientFriendlyName.get(),
                pollToAdd->Tag);
            GetGlobalEcStatistics().AddHistogramReference(*pollContext.histogram);
        }

        InsertHeadList(entry->Blink, &context->EcPoll->Link);
    };

    addContext.Ec = this;
    addContext.EcPoll = Poll;

    ExecutionContextTask task{ &addContext, taskFn };

    QueueTask(task, ExecutionContextEvent::QueueRegisterPollTask);
    task.WaitForCompletion();
}

static
void
UnregisterPollTaskCallback(
    _In_ void * TaskContext
)
{
    auto pollLink = static_cast<LIST_ENTRY *>(TaskContext);
    RemoveEntryList(pollLink);
    InitializeListHead(pollLink);

    auto const entry = CONTAINING_RECORD(pollLink, EXECUTION_CONTEXT_POLL, Link);
    auto &pollContext = ContextFromPoll(entry);
    if (pollContext.histogram)
    {
        GetGlobalEcStatistics().RemoveHistogramReference(*pollContext.histogram);
        pollContext.histogram = nullptr;
    }
}

_Use_decl_annotations_
PAGEDX
void
ExecutionContext::UnregisterPoll(
    EXECUTION_CONTEXT_POLL * Poll
)
{
    PAGED_CODE();

    ExecutionContextTask task{ &Poll->Link, UnregisterPollTaskCallback };
    QueueTask(task, ExecutionContextEvent::QueueUnregisterPollTask);
    task.WaitForCompletion();
}

_Use_decl_annotations_
PAGEDX
void
ExecutionContext::ChangePollFunction(
    EXECUTION_CONTEXT_POLL * Poll,
    PFN_EXECUTION_CONTEXT_POLL PollFn
)
{
    PAGED_CODE();

    struct
    {
        LIST_ENTRY * PollListHead;
        LIST_ENTRY * PollLink;
        PFN_EXECUTION_CONTEXT_POLL * TargetPointer;
        PFN_EXECUTION_CONTEXT_POLL NewPollFn;
    } context;

    context.PollListHead = &m_pollList;
    context.PollLink = &Poll->Link;
    context.TargetPointer = &Poll->PollFn;
    context.NewPollFn = PollFn;

    auto changePollFn = [](void * Context)
    {
        auto changeContext = static_cast<decltype(context) *>(Context);

        // Ensure the poll is currently registered with this EC
        bool found = false;
        for (auto link = changeContext->PollListHead->Flink; link != changeContext->PollListHead; link = link->Flink)
        {
            if (link == changeContext->PollLink)
            {
                found = true;
                break;
            }
        }

        NT_FRE_ASSERTMSG(
            "An attempt was made to change the poll function from the wrong execution context",
            found);

        // Change poll function
        *changeContext->TargetPointer = changeContext->NewPollFn;
    };

    ExecutionContextTask changePollTask { &context, changePollFn };

    QueueTask(changePollTask, ExecutionContextEvent::QueueChangePollFunctionTask);
    changePollTask.WaitForCompletion();
}

_Use_decl_annotations_
PAGEDX
void
ExecutionContext::RegisterNotification(
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    PAGED_CODE();

    // Can't use the same object to register twice
    NT_FRE_ASSERT(IsListEmpty(&Notification->Link));

    // Make sure adding the new notification to the list is synchronized by the EC, to do this we use a task
    struct
    {
        LIST_ENTRY * ListHead;
        LIST_ENTRY * Entry;
    } addContext;

    auto taskFn = [](_In_ void * Context)
    {
        auto context = static_cast<decltype(addContext) *>(Context);
        InsertTailList(context->ListHead, context->Entry);
    };

    addContext.ListHead = &m_notificationList;
    addContext.Entry = &Notification->Link;

    ExecutionContextTask task{ &addContext, taskFn };

    QueueTask(task, ExecutionContextEvent::QueueRegisterNotificationTask);
    task.WaitForCompletion();
}

static
void
UnregisterNotificationTaskCallback(
    _In_ void * TaskContext
)
{
    auto link = static_cast<LIST_ENTRY *>(TaskContext);
    RemoveEntryList(link);
    InitializeListHead(link);

    // Ensure the notification final state is disabled
    auto notificationReserved = NotificationReserved::FromNotificationEntry(link);
    notificationReserved->Disable();
}

_Use_decl_annotations_
PAGEDX
void
ExecutionContext::UnregisterNotification(
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    PAGED_CODE();

    ExecutionContextTask task{ &Notification->Link, UnregisterNotificationTaskCallback };
    QueueTask(task, ExecutionContextEvent::QueueUnregisterNotificationTask);
    task.WaitForCompletion();
}

_Use_decl_annotations_
void
ExecutionContext::ChangeStateLockNotHeld(
    ExecutionContextStateChangeReason Reason,
    ExecutionContextState TargetState
)
{
    KAcquireSpinLock lock{ m_lock };
    ChangeStateLockHeld(Reason, TargetState);
}

_Use_decl_annotations_
void
ExecutionContext::ChangeStateLockHeld(
    ExecutionContextStateChangeReason Reason,
    ExecutionContextState TargetState
)
{
    switch (m_state)
    {
        case ExecutionContextState::Stopped:

            // EC is about to start running, reset activity counters
            m_activityTracker.ResetCounters();

            NT_FRE_ASSERT(
                TargetState == ExecutionContextState::WaitingWorkerThread ||
                TargetState == ExecutionContextState::WaitingWorkerDpc ||
                TargetState == ExecutionContextState::Running);

            break;

        case ExecutionContextState::WaitingWorkerThread:

            NT_FRE_ASSERT(TargetState == ExecutionContextState::Running);

            break;

        case ExecutionContextState::WaitingWorkerDpc:

            NT_FRE_ASSERT(TargetState == ExecutionContextState::Running);

            break;

        case ExecutionContextState::Running:

            NT_FRE_ASSERT(
                TargetState == ExecutionContextState::Stopped ||
                TargetState == ExecutionContextState::WaitingWorkerThread ||
                TargetState == ExecutionContextState::WaitingWorkerDpc);

            break;
    }

    auto const previousState = m_state;
    m_state = TargetState;

    m_activityTracker.LogStateChange(Reason, previousState, TargetState);
}

_Use_decl_annotations_
void
ExecutionContext::Notify(
    void
)
{
    auto const isIrqlAboveDispatch = KeGetCurrentIrql() > DISPATCH_LEVEL;

    if (isIrqlAboveDispatch)
    {
        // We can't acquire a spin lock here, so we need to queue a DPC to evaluate the EC state
        m_notifyDpc.InsertQueueDpc();

        return;
    }

    NotifyCommon(NotifyContext::ArbitraryContext);
}

_Use_decl_annotations_
void
ExecutionContext::NotifyDpc(
    void
)
{
    NotifyCommon(NotifyContext::NotifyDpc);
}

_Use_decl_annotations_
void
ExecutionContext::NotifyCommon(
    NotifyContext Context
)
{
    KAcquireSpinLock lock{ m_lock };

    m_activityTracker.LogEvent(ExecutionContextEvent::Notify);

    m_hasPollWork = true;

    if (m_state != ExecutionContextState::Stopped)
    {
        // EC is already running in a different context, the poll work will be picked up by it
        return;
    }

    if (ShouldRunInDpc())
    {
        if (Context == NotifyContext::NotifyDpc)
        {
            // The EC is configured to run in DPC and we are already using one of our own, we can just start polling
            RunLockHeld(lock, RunContext::NotifyDpc);
        }
        else
        {
            // This Notify call is coming from an arbitrary context, switch to our own DPC
            QueueWorkerDpc(ExecutionContextStateChangeReason::NotifyNeedsDpc);
        }
    }
    else
    {
        // We cannot run in DPC, switch to the worker thread
        QueueWorkerThread(ExecutionContextStateChangeReason::NotifyNeedsWorkerThread);
    }
}

_Use_decl_annotations_
void
ExecutionContext::QueueTask(
    ExecutionContextTask & Task
)
{
    QueueTask(Task, ExecutionContextEvent::QueueTask);
}

_Use_decl_annotations_
void
ExecutionContext::QueueTask(
    ExecutionContextTask & Task,
    ExecutionContextEvent Event
)
{
    KAcquireSpinLock lock{ m_lock };

    m_activityTracker.LogEvent(Event);

    auto const queued = Task.AddToList(&m_taskQueue);

    if (queued && m_state == ExecutionContextState::Stopped)
    {
        QueueWorkerThread(ExecutionContextStateChangeReason::TaskQueuedWhileStopped);
    }
}

_Use_decl_annotations_
PAGEDX
EC_RETURN
ExecutionContext::Thread(
    void * Context
)
{
    PAGED_CODE();

    auto executionContext = static_cast<ExecutionContext *>(Context);
    executionContext->WorkerThreadRoutine();

    return EC_RETURN();
}

_Use_decl_annotations_
void
ExecutionContext::NotifyDpcRoutine(
    ExecutionContext * ExecutionContext
)
{
    ExecutionContext->NotifyDpc();
}

_Use_decl_annotations_
void
ExecutionContext::PollDpcRoutine(
    ExecutionContext * ExecutionContext
)
{
    ExecutionContext->PollDpc();
}

_Use_decl_annotations_
void
ExecutionContext::PollDpc(
    void
)
{
    RunLockNotHeld(RunContext::WorkerDpc);
}

_Use_decl_annotations_
void
ExecutionContext::WorkerThreadRoutine(
    void
)
{
    m_workerThreadReady.Set();

    while (true)
    {
        m_workerThreadRequested.Wait();

        RunLockNotHeld(RunContext::WorkerThread);

        if (m_terminateWorkerThread)
        {
            // If we are terminating the worker thread we must not have any registered poll callbacks nor notification callbacks
            NT_FRE_ASSERT(IsListEmpty(&m_pollList));
            NT_FRE_ASSERT(IsListEmpty(&m_notificationList));

            KAcquireSpinLock lock{ m_lock };
            NT_FRE_ASSERT(IsListEmpty(&m_taskQueue));

            return;
        }
    }
}

_Use_decl_annotations_
void
ExecutionContext::ProcessTaskQueue(
    KAcquireSpinLock & Lock
)
{
    NT_FRE_ASSERT(m_state == ExecutionContextState::Running);

    while (!IsListEmpty(&m_taskQueue))
    {
        m_activityTracker.LogTask();

        auto task = ExecutionContextTask::FromLink(
            RemoveHeadList(&m_taskQueue));

        InitializeListHead(&task->m_linkage);

        Lock.Release();

        auto const signalCompletion = task->m_signalCompletion;

        task->m_taskFn(task->m_context);

        // After this point it is not safe to touch the task memory, unless if the user
        // asked us to signal the internal event
        if (signalCompletion)
        {
            task->m_completed.Set();
        }

        Lock.Acquire();
    }
}

_Use_decl_annotations_
void
ExecutionContext::EnableNotifications(
    void
)
{
    for (auto link = m_notificationList.Flink; link != &m_notificationList; link = link->Flink)
    {
        auto notificationReserved = NotificationReserved::FromNotificationEntry(link);
        notificationReserved->Enable();
    }
}

_Use_decl_annotations_
void
ExecutionContext::DisableNotifications(
    void
)
{
    for (auto link = m_notificationList.Flink; link != &m_notificationList; link = link->Flink)
    {
        auto notificationReserved = NotificationReserved::FromNotificationEntry(link);
        notificationReserved->Disable();
    }
}

_Use_decl_annotations_
UINT64
ExecutionContext::PollClients(
    KIRQL Irql
)
{
    UINT64 workCounter = 0;

    for (auto link = m_pollList.Flink; link != &m_pollList; link = link->Flink)
    {
        EXECUTION_CONTEXT_POLL_PARAMETERS parameters{};
        parameters.Irql = Irql;

        auto poll = CONTAINING_RECORD(link, EXECUTION_CONTEXT_POLL, Link);
        auto &pollContext = ContextFromPoll(poll);
        Statistics::Timer timer { pollContext.histogram.get(), Irql == DISPATCH_LEVEL };

        poll->PollFn(poll->Context, &parameters);

        workCounter += parameters.WorkCounter;
    }

    return workCounter;
}

_Use_decl_annotations_
ExecutionContext::PollExitReason
ExecutionContext::Poll(
    KIRQL Irql
)
{
    const Statistics::Timer timer { &m_dpcHistogram, Irql == DISPATCH_LEVEL };
    m_pollingThread = EcGetCurrentThread();
    auto pollingScopeGuard = wil::scope_exit([this](){ m_pollingThread = EC_THREAD_INVALID; });

    DisableNotifications();
    bool armed = false;

    auto const maxTimeInDispatch = mem::ReadNoFence(&m_runtimeKnobs->MaxTimeAtDispatch);
    auto const flags = mem::ReadNoFence(&m_runtimeKnobs->Flags);
    auto const dpcWatchdogThreshold = mem::ReadNoFence(&m_runtimeKnobs->DpcWatchdogTimerThreshold);

    TimeBudget timeBudget {
        maxTimeInDispatch,
        WI_IsFlagSet(flags, ExecutionContextFlagTryExtendMaxTimeAtDispatch),
        dpcWatchdogThreshold,
        Irql
    };

    timeBudget.Reset();

    while (true)
    {
        // when clients did some work, we assume there might be more work to do
        auto const workCounter = PollClients(Irql);
        auto const workDone = workCounter > 0;

        m_activityTracker.LogIteration(workCounter);

        if (!workDone && !armed)
        {
            // No work was done and notifications are currently not armed, arm them
            EnableNotifications();
            armed = true;
        }
        else if (!workDone && armed)
        {
            // No work was done and notifications were already enabled, exit the polling loop
            break;
        }
        else if (workDone && armed)
        {
            // We had armed notifications but had activity before exiting the polling loop, disarm notifications
            DisableNotifications();
            armed = false;
        }

        {
            KAcquireSpinLock lock{ m_lock };

            if (!IsListEmpty(&m_taskQueue))
            {
                return PollExitReason::TaskPending;
            }
        }

        if (Irql == DISPATCH_LEVEL)
        {
            m_warningLogger.LogWarningIfOvertime(timeBudget);
        }

        // Always false at PASSIVE_LEVEL
        if (timeBudget.IsOutOfBudget() && !m_terminateWorkerThread)
        {
            return PollExitReason::OutOfDpcBudget;
        }
    }

    return PollExitReason::NoMoreWork;
}

_Use_decl_annotations_
bool
ExecutionContext::ShouldRunInDpc(
    void
) const
{
    auto const flags = mem::ReadNoFence(&m_runtimeKnobs->Flags);
    return WI_IsFlagSet(flags, ExecutionContextFlagRunDpcForFirstLoop);
}

_Use_decl_annotations_
EC_THREAD_PRIORITY
ExecutionContext::GetWorkerThreadPriority(
    void
)
{
    if (!m_workerThreadObject)
    {
        return EC_INVALID_THREAD_PRIORITY;
    }

    return EcThreadGetPriority(m_workerThreadObject);
}

_Use_decl_annotations_
void
ExecutionContext::SetWorkerThreadAffinity(
    const GROUP_AFFINITY & GroupAffinity
)
{
    {
        KAcquireSpinLock lock{ m_lock };
        m_workerThreadAffinity = GroupAffinity;
    }

    // If the task is already queued this is a no-op, the new value we set last will be used
    // If the task is currenlty running it might or might not pick up the new value right
    // away, but we are guaranteed to queue the task
    QueueTask(m_setWorkerThreadAffinityTask, ExecutionContextEvent::QueueSetWorkerThreadAffinityTask);
}

_Use_decl_annotations_
void
ExecutionContext::SetDpcTargetProcessor(
    _In_ const PROCESSOR_NUMBER & ProcessorNumber
)
{
    static_assert(sizeof(PROCESSOR_NUMBER) == sizeof(ULONG));
    WriteULongRelease(reinterpret_cast<ULONG *>(&m_dpcAffinity), reinterpret_cast<const ULONG &>(ProcessorNumber));
}

_Use_decl_annotations_
void
ExecutionContext::SetWorkerThreadDescription(
    const UNICODE_STRING & Description
)
{
    if (Description.Length == 0 || Description.Buffer == nullptr)
    {
        return;
    }

    EcThreadSetDescription(m_workerThreadObject, Description);
}

_Use_decl_annotations_
void
ExecutionContext::RunLockNotHeld(
    RunContext Context
)
{
    KAcquireSpinLock lock { m_lock };
    RunLockHeld(lock, Context);
}

_Use_decl_annotations_
void
ExecutionContext::RunLockHeld(
    KAcquireSpinLock & Lock,
    RunContext Context
)
{
    ExecutionContextStateChangeReason reason{};

    switch (Context)
    {
        case RunContext::NotifyDpc:

            reason = ExecutionContextStateChangeReason::NotifyDpcScheduledWhileStopped;
            break;

        case RunContext::WorkerDpc:

            reason = ExecutionContextStateChangeReason::WorkerDpcScheduled;
            break;

        case RunContext::WorkerThread:

            reason = ExecutionContextStateChangeReason::WorkerThreadScheduled;
            break;

        default:

            NT_FRE_ASSERTMSG(
                "Invalid RunContext passed to RunLockHeld",
                false);
            break;
    }

    ChangeStateLockHeld(reason, ExecutionContextState::Running);

    //
    // Tasks have priority over polling
    //

    if (!IsListEmpty(&m_taskQueue))
    {
        if (Context != RunContext::WorkerThread)
        {
            // If we're running at DPC we must switch to the worker thread
            QueueWorkerThread(ExecutionContextStateChangeReason::TaskQueuedWhileRunningFromDpc);
            return;
        }

        ProcessTaskQueue(Lock);
    }

    if (m_hasPollWork)
    {
        m_hasPollWork = false;
        Lock.Release();

        // Use a new scope for IrqlController, otherwise it might cause an IRQL mismatch next time we release the spin lock
        PollExitReason exitReason;
        {
            auto const flags = mem::ReadNoFence(&m_runtimeKnobs->Flags);
            auto const raiseToDispatch = Context == RunContext::WorkerThread && WI_IsFlagSet(flags, ExecutionContextFlagRunWorkerThreadAtDispatch);
            IrqlController irqlController { raiseToDispatch };

            KIRQL irql{};

            switch (Context)
            {
                case RunContext::NotifyDpc:

                    [[fallthrough]];

                case RunContext::WorkerDpc:

                    irql = DISPATCH_LEVEL;
                    break;

                case RunContext::WorkerThread:

                    irql = irqlController.WasRaised()
                        ? DISPATCH_LEVEL
                        : PASSIVE_LEVEL;

                    break;
            }

            exitReason = Poll(irql);
        }

        Lock.Acquire();

        if (exitReason != PollExitReason::NoMoreWork)
        {
            // ExecutionContext::Poll had to return before the poll clients went idle to let us schedule our worker
            // thread. Make sure we go back to polling when we get rescheduled
            m_hasPollWork = true;

            QueueWorkerThread(
                exitReason == PollExitReason::OutOfDpcBudget
                    ? ExecutionContextStateChangeReason::OutOfDpcBudget
                    : ExecutionContextStateChangeReason::TaskPreemptedPolling);

            return;
        }
    }

    // Before changing the state back to stopped we need to check if new work was scheduled
    if (!IsListEmpty(&m_taskQueue))
    {
        QueueWorkerThread(ExecutionContextStateChangeReason::TaskQueuedWhileExitingWorker);
    }
    else if (m_hasPollWork)
    {
        if (ShouldRunInDpc())
        {
            QueueWorkerDpc(ExecutionContextStateChangeReason::NotifyWhileExitingWorker);
        }
        else
        {
            QueueWorkerThread(ExecutionContextStateChangeReason::NotifyWhileExitingWorker);
        }
    }
    else
    {
        ChangeStateLockHeld(ExecutionContextStateChangeReason::NoMoreWork, ExecutionContextState::Stopped);
    }
}

_Use_decl_annotations_
void
ExecutionContext::QueueWorkerThread(
    ExecutionContextStateChangeReason Reason
)
{
    ChangeStateLockHeld(Reason, ExecutionContextState::WaitingWorkerThread);
    m_workerThreadRequested.Set();
}

_Use_decl_annotations_
void
ExecutionContext::QueueWorkerDpc(
    ExecutionContextStateChangeReason Reason
)
{
    ChangeStateLockHeld(Reason, ExecutionContextState::WaitingWorkerDpc);

    auto processorNumberAsUlong = ReadULongAcquire(reinterpret_cast<ULONG *>(&m_dpcAffinity));
    auto processorNumber = reinterpret_cast<PROCESSOR_NUMBER &>(processorNumberAsUlong);

    if (processorNumber.Number != MAXIMUM_PROC_PER_GROUP)
    {
        m_pollDpc.SetTargetProcessorDpcEx(&processorNumber);
    }

    m_pollDpc.InsertQueueDpc();
}

_Use_decl_annotations_
void
ExecutionContext::EvtSetWorkerThreadAffinity(
    void * Context
)
{
    auto executionContext = static_cast<ExecutionContext *>(Context);
    executionContext->SetWorkerThreadAffinityTask();
}

_Use_decl_annotations_
void
ExecutionContext::SetWorkerThreadAffinityTask(
    void
)
{
    KAcquireSpinLock lock{ m_lock };
    if (!m_affinityChanged)
    {
        EcThreadSetAffinity(&m_workerThreadAffinity, &m_originalWorkerThreadAffinity);
        m_affinityChanged = true;
    }
    else
    {
        EcThreadSetAffinity(&m_workerThreadAffinity, nullptr);
    }
}

_Use_decl_annotations_
void
ExecutionContext::EvtTerminateWorkerThread(
    void * Context
)
{
    auto executionContext = static_cast<ExecutionContext *>(Context);
    executionContext->TerminateWorkerThreadTask();
}

_Use_decl_annotations_
void
ExecutionContext::TerminateWorkerThreadTask(
    void
)
{
    //Worker thread termination should be requested only once
    NT_FRE_ASSERT(!m_terminateWorkerThread);

    m_terminateWorkerThread = true;
}
