#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "Queue.tmh"
#include "Queue.hpp"

_Use_decl_annotations_
void
Queue::EcTaskCancel(
    void * Context
)
{
    auto queue = reinterpret_cast<Queue *>(Context);
    queue->m_queueDispatch->Cancel(queue->m_queue);
}

_Use_decl_annotations_
void
Queue::EvtSetNblNotification(
    void * Context,
    BOOLEAN NotificationEnabled
)
{
    auto queue = reinterpret_cast<Queue *>(Context);
    InterlockedExchange(&queue->m_nblNotificationEnabled, NotificationEnabled);
}

_Use_decl_annotations_
Queue::Queue(
    NxTranslationApp & App,
    size_t QueueId,
    PFN_EXECUTION_CONTEXT_POLL EvtPollQueueStarted,
    PFN_EXECUTION_CONTEXT_POLL EvtPollQueueStopping
) noexcept
    : m_app(App)
    , m_queueId(QueueId)
    , m_pollQueueStarted(EvtPollQueueStarted)
    , m_pollQueueStopping(EvtPollQueueStopping)

{
    INITIALIZE_EXECUTION_CONTEXT_NOTIFICATION(
        &m_nblNotification,
        this,
        EvtSetNblNotification);
}

size_t
Queue::GetQueueId(
    void
) const
{
    return m_queueId;
}

_Use_decl_annotations_
void
Queue::Start(
    void
)
{
    NT_FRE_ASSERT(! m_started);
    NT_FRE_ASSERT(! m_stopping);

    // Invoke NetCx to start the IHV driver queue
    m_queueDispatch->Start(m_queue);

    // Start producing new packets
    INITIALIZE_EXECUTION_CONTEXT_POLL(
        &m_executionContextPoll,
        ExecutionContextPollTypeAdvance,
        this,
        m_pollQueueStarted);

    m_executionContextDispatch->RegisterPoll(m_executionContext, &m_executionContextPoll);

    m_executionContextDispatch->Notify(m_executionContext);

    m_started = true;
}

_Use_decl_annotations_
bool
Queue::IsStarted(
    void
) const
{
    return m_started;
}

_Use_decl_annotations_
void
Queue::Cancel(
    void
)
{
    NT_FRE_ASSERT(m_started);
    NT_FRE_ASSERT(! m_stopping);

    m_stopping = true;

    // Stop giving new resources to the IHV driver
    m_executionContextDispatch->ChangePollFunction(
        m_executionContext,
        &m_executionContextPoll,
        m_pollQueueStopping);

    // Invoke IHV driver's cancel callback
    m_executionContextDispatch->RunSynchronousTask(
        m_executionContext,
        this,
        EcTaskCancel);

    // Enforce executing the queue teardown logic
    m_executionContextDispatch->Notify(m_executionContext);
}

_Use_decl_annotations_
bool
Queue::IsStopping(
    void
) const
{
    return m_stopping;
}

_Use_decl_annotations_
void
Queue::Stop(
    void
)
{
    NT_FRE_ASSERT(m_started);
    NT_FRE_ASSERT(m_stopping);

    // Wait until queue is ready to be stopped
    m_readyToStop.Wait();

    // Make sure we stop calling advance
    m_executionContextDispatch->UnregisterPoll(m_executionContext, &m_executionContextPoll);

    // Invoke NetCx to stop the IHV driver queue
    m_queueDispatch->Stop(m_queue);

    m_readyToStop.Clear();

    m_started = false;
    m_stopping = false;
}
