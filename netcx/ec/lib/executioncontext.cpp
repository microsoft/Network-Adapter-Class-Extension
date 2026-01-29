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
#include "DispatcherSubsystem.hpp"

GUID g_staticEcClientIdentier = { 0 };

DECLARE_CONST_UNICODE_STRING(
    DefaultFriendlyName,
    L"Execution Context");

PassiveLevelKnobs passivelLevelKnobs;

bool
operator==(
    PROCESSOR_NUMBER const & P1,
    PROCESSOR_NUMBER const & P2
)
{
    return P1.Group == P2.Group && P1.Number == P2.Number;
}

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
    _In_ GUID const* ClientIdentifier,
    _In_ wistd::unique_ptr<Dispatcher> dispatcher,
    _In_ KPoolPtr<UNICODE_STRING> clientFriendlyName
) noexcept :
    m_dispatcher(wistd::move(dispatcher)),
    m_clientFriendlyName(wistd::move(clientFriendlyName)),
    m_clientIdentifier(*ClientIdentifier),
    m_activityTracker(*ClientIdentifier),
    m_notifyDpc(*this),
    m_taskWorker{*m_dispatcher, DispatchHighPriority, TaskWorkerEntry},
    m_pollWorker{*m_dispatcher, DispatchNormalPriority, PollWorkerEntry, PollWorkerStart, PollWorkerStop},
    m_setAffinityWorker{*m_dispatcher, DispatchHighPriority, SetAffinityWorker}
{
    PAGED_CODE();
    InitializeGlobalEcStatistic();

    InitializeListHead(&m_taskQueue);
    InitializeListHead(&m_pollList);
    InitializeListHead(&m_notificationList);

    m_notifyDpc.SetImportance(DpcImportance::MediumHigh);
    m_taskWorker.Flags.NeedsPassiveLevel = 1;
    m_taskWorker.DebugData = reinterpret_cast<ULONG_PTR>(this);
    m_pollWorker.DebugData = reinterpret_cast<ULONG_PTR>(this);
    InitializeListHead(&m_taskWorker.Entry);
    InitializeListHead(&m_pollWorker.Entry);

    m_activityTracker.Initialize(m_clientFriendlyName.get());
    m_taskWorker.SetTargetDispatcher(*m_dispatcher);
    m_pollWorker.SetTargetDispatcher(*m_dispatcher);
    m_setAffinityWorker.SetTargetDispatcher(*m_dispatcher);
}

_Use_decl_annotations_
PAGEDX
ExecutionContext::~ExecutionContext(
    void
)
{
    PAGED_CODE();

    Destroy();

    NT_FRE_ASSERT(IsListEmpty(&m_taskQueue));
}

_Use_decl_annotations_
PAGEDX
NTSTATUS
ExecutionContext::Create(
    _In_opt_ UNICODE_STRING const* ClientFriendlyName,
    _In_opt_ GUID const* ClientIdentifier,
    _Out_ wistd::unique_ptr<ExecutionContext>& ExecutionContext)
{
    auto dispatcher = wil::make_unique_nothrow<Dispatcher>(passivelLevelKnobs);

    if (!dispatcher)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    auto ntStatus = dispatcher->Initialize();

    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    auto const sourceString = ClientFriendlyName != nullptr && ClientFriendlyName->Length > 0
        ? *ClientFriendlyName
        : DefaultFriendlyName;

    KPoolPtr<UNICODE_STRING> clientFriendlyName =
        Rtl::DuplicateUnicodeString(
            sourceString,
            'aNCE'
        );

    if (!clientFriendlyName)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    auto executionContext = wil::make_unique_nothrow<::ExecutionContext>(
        ClientIdentifier != nullptr ? ClientIdentifier : &g_staticEcClientIdentier,
        wistd::move(dispatcher),
        wistd::move(clientFriendlyName)
    );

    if (!executionContext)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ExecutionContext = wistd::move(executionContext);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
ExecutionContext::Destroy(
)
{
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

    KWaitTask unregisterEverythingTask{&localContext, unregisterEverything};
    QueueTask(unregisterEverythingTask.GetTask(), ExecutionContextEvent::QueueUnregisterEverything);

    unregisterEverythingTask.WaitForCompletion();
    m_pollWorker.Rundown();
    m_taskWorker.Rundown();
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

    KWaitTask task{&addContext, taskFn};

    QueueTask(task.GetTask(), ExecutionContextEvent::QueueRegisterPollTask);
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

    KWaitTask task{&Poll->Link, UnregisterPollTaskCallback};
    QueueTask(task.GetTask(), ExecutionContextEvent::QueueUnregisterPollTask);
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

    KWaitTask changePollTask{&context, changePollFn};
    QueueTask(changePollTask.GetTask(), ExecutionContextEvent::QueueChangePollFunctionTask);
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

    KWaitTask task{&addContext, taskFn};
    QueueTask(task.GetTask(), ExecutionContextEvent::QueueRegisterNotificationTask);
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

    KWaitTask task{&Notification->Link, UnregisterNotificationTaskCallback};
    QueueTask(task.GetTask(), ExecutionContextEvent::QueueUnregisterNotificationTask);
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
                TargetState == ExecutionContextState::WaitingDispatcher ||
                TargetState == ExecutionContextState::Running);

            break;

        case ExecutionContextState::WaitingWorkerThread:

            NT_FRE_ASSERT(TargetState == ExecutionContextState::Running);

            break;

        case ExecutionContextState::WaitingWorkerDpc:

            NT_FRE_ASSERT(TargetState == ExecutionContextState::Running);

            break;

        case ExecutionContextState::WaitingDispatcher:

            NT_FRE_ASSERT(TargetState == ExecutionContextState::Running);

            break;

        case ExecutionContextState::Running:

            NT_FRE_ASSERT(
                TargetState == ExecutionContextState::Stopped ||
                TargetState == ExecutionContextState::WaitingWorkerThread ||
                TargetState == ExecutionContextState::WaitingWorkerDpc ||
                TargetState == ExecutionContextState::WaitingDispatcher);

            break;
    }

    auto const previousState = m_state;
    m_state = TargetState;

    m_activityTracker.LogStateChange(Reason, previousState, TargetState, m_pollWorker.CurrentDispatcher);
}

_Use_decl_annotations_
void
ExecutionContext::Notify(
    void
)
{
    EXECUTION_CONTEXT_NOTIFY_PARAMETERS parameters;
    INITIALIZE_EXECUTION_CONTEXT_NOTIFY_PARAMETERS(&parameters);

    NotifyEx(&parameters);
}

_Use_decl_annotations_
void
ExecutionContext::NotifyEx(
    EXECUTION_CONTEXT_NOTIFY_PARAMETERS * Parameters
)
{
    auto const isIrqlAboveDispatch = Parameters->Irql > DISPATCH_LEVEL;

    if (isIrqlAboveDispatch)
    {
        // We can't acquire a spin lock here, so we need to queue a DPC to evaluate the EC state
        m_notifyDpc.InsertQueueDpc();

        return;
    }

    NotifyCommon(NotifyContext::ArbitraryContext, Parameters);
}

_Use_decl_annotations_
void
ExecutionContext::NotifyDpc(
    void
)
{
    EXECUTION_CONTEXT_NOTIFY_PARAMETERS parameters;
    INITIALIZE_EXECUTION_CONTEXT_NOTIFY_PARAMETERS_WITH_IRQL(&parameters, DISPATCH_LEVEL);

    NotifyCommon(NotifyContext::NotifyDpc, &parameters);
}

_Use_decl_annotations_
void
ExecutionContext::NotifyCommon(
    NotifyContext Context,
    EXECUTION_CONTEXT_NOTIFY_PARAMETERS * Parameters
)
{
    UNREFERENCED_PARAMETER((Context, Parameters));

    KAcquireSpinLock lock{ m_lock };
    m_activityTracker.LogEvent(ExecutionContextEvent::Notify);
    lock.Release();

    m_pollWorker.Insert();
}

_Use_decl_annotations_
void
ExecutionContext::QueueTask(
    EXECUTION_CONTEXT_TASK * Task
)
{
    QueueTask(Task, ExecutionContextEvent::QueueTask);
}

_Use_decl_annotations_
void
ExecutionContext::QueueTask(
    EXECUTION_CONTEXT_TASK * Task,
    ExecutionContextEvent Event
)
{
    KAcquireSpinLock lock{ m_lock };

    m_activityTracker.LogEvent(Event);

    InsertTailList(&m_taskQueue, &Task->Linkage);
    lock.Release();

    m_taskWorker.Insert();
}

_Use_decl_annotations_
void
ExecutionContext::ProcessTaskQueue(
    KAcquireSpinLock & Lock
)
{
    while (!IsListEmpty(&m_taskQueue))
    {
        m_activityTracker.LogTask();

        auto task = CONTAINING_RECORD(
            RemoveHeadList(&m_taskQueue),
            EXECUTION_CONTEXT_TASK,
            Linkage);

        InitializeListHead(&task->Linkage);

        Lock.Release();

        task->TaskFn(task->Context);

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

    m_armed = true;
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

    m_armed = false;
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
void
ExecutionContext::SetAffinity(
    _In_ const PROCESSOR_NUMBER & Processor
)
{
    static_assert(sizeof(PROCESSOR_NUMBER) == sizeof(ULONG));
    WriteULongRelease(reinterpret_cast<ULONG *>(&m_affinity), reinterpret_cast<const ULONG &>(Processor));
    m_setAffinityWorker.Insert();
}

WorkerExitStatus
TaskWorkerEntry(
    DispatcherWorker * Worker,
    KIRQL Irql
)
{
    auto executionContext = CONTAINING_RECORD(Worker, ExecutionContext, m_taskWorker);
    return executionContext->TaskWorker(Irql);
}

WorkerExitStatus
ExecutionContext::TaskWorker(KIRQL Irql)
{
    NT_FRE_ASSERT(Irql == PASSIVE_LEVEL);

    KAcquireSpinLock lock{ m_lock };
    ProcessTaskQueue(lock);

    return WorkerExitStatus::Remove;
}

void
PollWorkerStart(
    DispatcherWorker * Worker
)
{
    auto executionContext = CONTAINING_RECORD(Worker, ExecutionContext, m_pollWorker);
    executionContext->PollStart();
}

void
ExecutionContext::PollStart(
    void
)
{
    ChangeStateLockNotHeld(ExecutionContextStateChangeReason::PollWorkerEntry, ExecutionContextState::Running);
    DisableNotifications();
}

void
PollWorkerStop(
    DispatcherWorker * Worker
)
{
    auto executionContext = CONTAINING_RECORD(Worker, ExecutionContext, m_pollWorker);
    executionContext->PollStop();
}

void
ExecutionContext::PollStop(
    void
)
{
    ChangeStateLockNotHeld(ExecutionContextStateChangeReason::PollWorkerExit, ExecutionContextState::Stopped);
}

WorkerExitStatus
PollWorkerEntry(
    DispatcherWorker * Worker,
    KIRQL Irql
)
{
    auto executionContext = CONTAINING_RECORD(Worker, ExecutionContext, m_pollWorker);
    return executionContext->PollWorker(Irql);
}

WorkerExitStatus
ExecutionContext::PollWorker(KIRQL Irql)
{
    auto const workCounter = PollClients(Irql);
    auto const workDone = workCounter > 0;

    m_activityTracker.LogIteration(workCounter);

    if (!workDone && !m_armed)
    {
        // No work was done and notifications are currently not armed, arm them
        EnableNotifications();
    }
    else if (workDone && m_armed)
    {
        // We had armed notifications but had activity before exiting the polling loop, disarm notifications
        DisableNotifications();
    }
    else if (!workDone && m_armed)
    {
        return WorkerExitStatus::Remove;
    }

    return WorkerExitStatus::Yield;
}

WorkerExitStatus
SetAffinityWorker(
    DispatcherWorker * Worker,
    KIRQL Irql
)
{
    UNREFERENCED_PARAMETER(Irql);

    auto executionContext = CONTAINING_RECORD(
        Worker,
        ExecutionContext,
        m_setAffinityWorker);

    executionContext->SetAffinityWorkerInner();

    return WorkerExitStatus::Remove;
}

_Use_decl_annotations_
void
ExecutionContext::SetAffinityWorkerInner(
    void
)
{
    auto processorNumberAsUlong = ReadULongAcquire(reinterpret_cast<ULONG *>(&m_affinity));
    auto processorNumber = reinterpret_cast<PROCESSOR_NUMBER &>(processorNumberAsUlong);
    auto & newDispatcher = GetProcessorDispatcher(processorNumber);

    m_taskWorker.SetTargetDispatcher(newDispatcher);
    m_pollWorker.SetTargetDispatcher(newDispatcher);
    m_setAffinityWorker.SetTargetDispatcher(newDispatcher);
}
