// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "QueueControl.tmh"
#include "QueueControl.hpp"

#include "Driver.hpp"
#include "RxScaling.hpp"

_Use_decl_annotations_
QueueControl::QueueControl(
    NxTranslationApp & App,
    NET_CLIENT_DISPATCH const * Dispatch,
    NET_CLIENT_ADAPTER Adapter,
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
    Rtl::KArray<wistd::unique_ptr<NxTxXlat>, NonPagedPoolNx> & TxQueues,
    Rtl::KArray<wistd::unique_ptr<NxRxXlat>, NonPagedPoolNx> & RxQueues
) noexcept
    : m_app(App)
    , m_dispatch(Dispatch)
    , m_adapter(Adapter)
    , m_adapterDispatch(AdapterDispatch)
    , m_work(this)
    , m_txQueues(TxQueues)
    , m_rxQueues(RxQueues)
{
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEDX
NTSTATUS
QueueControl::Initialize(
    void
) noexcept
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_work.Initialize(GetTranslatorDriverObject()),
        "Failed to initialize QueueControl work item");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
QueueControl::CreateQueues(
    size_t TxQueueCount,
    size_t RxQueueCount
)
{
    KLockThisExclusive lock(m_workSyncLock);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_txQueues.reserve(TxQueueCount));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_rxQueues.reserve(RxQueueCount));

    Rtl::KArray<wistd::unique_ptr<NxTxXlat>> txQueues;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! txQueues.resize(TxQueueCount));

    Rtl::KArray<wistd::unique_ptr<NxRxXlat>> rxQueues;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! rxQueues.resize(RxQueueCount));

    for (auto queueId = 0u; queueId < txQueues.count(); queueId++)
    {
        auto queue = wil::make_unique_nothrow<NxTxXlat>(
            m_app,
            false,
            queueId,
            *this,
            m_dispatch,
            m_adapter,
            m_adapterDispatch);

        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! queue);

        CX_RETURN_IF_NOT_NT_SUCCESS(
            queue->Initialize());

        CX_RETURN_IF_NOT_NT_SUCCESS(
            queue->Create());

        txQueues[queueId] = wistd::move(queue);
    }

    for (auto queueId = 0u; queueId < rxQueues.count(); queueId++)
    {
        auto queue = wil::make_unique_nothrow<NxRxXlat>(
            m_app,
            queueId,
            m_dispatch,
            m_adapter,
            m_adapterDispatch);

        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! queue);

        CX_RETURN_IF_NOT_NT_SUCCESS(
            queue->Initialize());

        rxQueues[queueId] = wistd::move(queue);
    }

    for (auto & queue : txQueues)
    {
        NT_FRE_ASSERT(m_txQueues.append(wistd::move(queue)));
    }

    for (auto & queue : rxQueues)
    {
        NT_FRE_ASSERT(m_rxQueues.append(wistd::move(queue)));
    }

    CX_RETURN_STATUS_SUCCESS_MSG("Created Default Queues"
        " Tx Count: %Iu/%Iu"
        " Rx Count: %Iu/%Iu",
        TxQueueCount, m_txQueues.count(),
        RxQueueCount, m_rxQueues.count());
}

//
// Creates receive scaling queues, configures and starts queues if
// the data path is available and receive scaling has been initialized.
//
// This method may be called during creation of the data path to
// create the queues needed for receive scaling or it may be called
// during receive scaling initialization.
//
// It is important to understand that receive scaling initialization
// can occur even when the data path is not available. The following
// two cases can occur.
//
// 1. The data path is not available we optimistically succeed and
//    expect this method will be called again during data path creation.
//    If during data path creation this method fails then the
//    translator revokes receive scaling at that time.
//
// 2. The data path is available we attempt to complete all necessary
//    operations and return the outcome.
//
_Use_decl_annotations_
PAGEDX
NTSTATUS
QueueControl::CreateRxScalingQueues(
    wistd::unique_ptr<RxScaling> & RxScaling
)
{
    KLockThisExclusive lock(m_workSyncLock);

    if (! RxScaling)
    {
        return STATUS_SUCCESS;
    }

    Rtl::KArray<wistd::unique_ptr<NxRxXlat>> queues;
    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INSUFFICIENT_RESOURCES,
        ! queues.resize(RxScaling->GetNumberOfQueues() - m_rxQueues.count()),
        "Failed to create array to store Rx queues for Rx scaling");

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_rxQueues.reserve(RxScaling->GetNumberOfQueues()),
        "Failed to reserve memory for Rx scaling queues");

    for (auto i = m_rxQueues.count(); i < RxScaling->GetNumberOfQueues(); i++)
    {
        auto rxQueue = wil::make_unique_nothrow<NxRxXlat>(
            m_app,
            i,
            m_dispatch,
            m_adapter,
            m_adapterDispatch);

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INSUFFICIENT_RESOURCES,
            ! rxQueue,
            "Failed to create queue for Rx scaling");

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            rxQueue->Initialize(),
            "Failed to initialize queue for Rx scaling");

        queues[i - m_rxQueues.count()] = wistd::move(rxQueue);
    }

    for (auto & queue : queues)
    {
        NT_FRE_ASSERT(m_rxQueues.append(wistd::move(queue)));
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        RxScaling->Configure(),
        "Failed to configure RxScaling");

    CX_RETURN_STATUS_SUCCESS_MSG("Created Rx Scaling Queues"
        " Rx Count: %Iu/%Iu",
        RxScaling->GetNumberOfQueues(), m_rxQueues.count());
}

_Use_decl_annotations_
NTSTATUS
QueueControl::ProvisionOnDemandQueues(
    size_t TxQueueCount
)
{
    KLockThisExclusive lock(m_workSyncLock);

    Rtl::KArray<wistd::unique_ptr<NxTxXlat>> queues;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! queues.resize(TxQueueCount - m_txQueues.count()));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_txQueues.reserve(TxQueueCount));

    for (auto queueId = m_txQueues.count(); queueId < TxQueueCount; queueId++)
    {
        auto queue = wil::make_unique_nothrow<NxTxXlat>(
            m_app,
            true,
            queueId,
            *this,
            m_dispatch,
            m_adapter,
            m_adapterDispatch);

        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! queue);

        CX_RETURN_IF_NOT_NT_SUCCESS(
            queue->Initialize());

        queues[queueId - m_txQueues.count()] = wistd::move(queue);
    }

    for (auto & queue : queues)
    {
        NT_FRE_ASSERT(m_txQueues.append(wistd::move(queue)));
    }

    CX_RETURN_STATUS_SUCCESS_MSG("Provisioned Tx Demand Queues"
        " Tx Count: %Iu/%Iu",
        TxQueueCount, m_txQueues.count());
}

_Use_decl_annotations_
void
QueueControl::DestroyOnDemandQueuesByPeerDemux(
    size_t PeerDemux
)
{
    KLockThisExclusive lock(m_workSyncLock);

    for (auto const & queue : m_txQueues)
    {
        auto const property = queue->GetTxDemuxProperty(TxDemuxTypePeerAddress);
        if (property && property->Value == PeerDemux)
        {
            DestroyOnDemandQueue(queue->GetQueueId());
        }
    }
}

_Use_decl_annotations_
void
QueueControl::DestroyOnDemandQueue(
    size_t Index
)
{
    auto & queue = m_txQueues[Index];

    if (queue->IsCreated())
    {
        if (queue->IsStarted())
        {
            if (!queue->IsStopping())
            {
                queue->Cancel();
            }

            queue->Stop();
        }
        else
        {
            queue->DropQueuedNetBufferLists();
        }

        queue->Destroy();

        CX_RETURN_MSG("Destroyed Demand Queue"
            " Index %Iu",
            Index);
    }
}

_Use_decl_annotations_
void
QueueControl::SignalQueueStateChange(
    void
)
{
    KAcquireSpinLock lock(m_workQueueLock);

    m_event.Clear();

    TryQueueWorkItem();
}

_Use_decl_annotations_
void
QueueControl::SetDatapathState(
    DatapathState State
)
{
    KAcquireSpinLock lock(m_workQueueLock);

    (void)InterlockedExchange(
        reinterpret_cast<volatile LONG *>(&m_state),
        static_cast<LONG>(State));
    m_event.Clear();
    
    TryQueueWorkItem();
}

_Use_decl_annotations_
PAGEDX
void
QueueControl::StartQueues(
    void
)
{
    SetDatapathState(DatapathState::Started);
    m_event.Wait();
}

_Use_decl_annotations_
PAGEDX
void
QueueControl::CancelQueues(
    void
)
{
    SetDatapathState(DatapathState::Stopping);
    m_event.Wait();
}

_Use_decl_annotations_
PAGEDX
void
QueueControl::StopQueues(
    void
)
{
    SetDatapathState(DatapathState::Stopped);
    m_event.Wait();
}

_Use_decl_annotations_
PAGEDX
void
QueueControl::DestroyQueues(
    void
)
{
    KLockThisExclusive lock(m_workSyncLock);

    m_txQueues.clear();
    m_rxQueues.clear();
}

_Use_decl_annotations_
void
QueueControl::SynchronizeDatapathWork(
    void
)
{
    LogInfo(FLAG_TRANSLATOR,
        "QueueControl work item entry. ClientAdapter: %p",
        m_adapter);

    ULONG counter = 0;

    while (true)
    {
        while (SynchronizeDatapathState() || !IsDatapathStateSynchronized())
        {
            counter++;
        }

        {
            KAcquireSpinLock lock(m_workQueueLock);
            if (m_loopState == SynchronizationLoop::Running)
            {
                LogInfo(FLAG_TRANSLATOR,
                    "m_loopState is set to Stopping. ClientAdapter: %p",
                    m_adapter);

                m_loopState = SynchronizationLoop::Stopping;
            }
            else if (m_loopState == SynchronizationLoop::Stopping)
            {
                LogInfo(FLAG_TRANSLATOR,
                    "m_loopState is set to Stopped. ClientAdapter: %p",
                    m_adapter);

                // between setting state to Stopping and then running another iteration,
                // no new workitem has been scheduled, so we can now quit.
                m_loopState = SynchronizationLoop::Stopped;
                break;
            }
        }
    }

    LogInfo(FLAG_TRANSLATOR,
        "QueueControl work item exit. ClientAdapter: %p, counter: %u",
        m_adapter,
        counter);
}

bool
QueueControl::SynchronizeDatapathState(
    void
)
{
    KLockThisExclusive lock(m_workSyncLock);

    bool changed = false;

    switch (m_state)
    {

    case DatapathState::Started:
        
        if (m_queueControlCriticalFailure)
        {
            // We encountered critical failure when trying to create the queues;
            // Re-enumeration request has been issued, meanwhile ignoring all consequent attempts to create the queues.
            return false;
        }
        
        for (auto & queue : m_txQueues)
        {
            auto const notCreated = ! queue->IsCreated() && queue->IsQueuingNetBufferLists();

            if (notCreated)
            {
                NT_FRE_ASSERT(queue->IsOnDemand());
                const NTSTATUS queueCreationStatus = queue->Create();
                if (!NT_SUCCESS(queueCreationStatus))
                {
                    LogError(FLAG_ADAPTER,
                        "Failed to create on demand queue. NETADAPTER: %p, Status: %!STATUS!",
                        m_adapter,
                        queueCreationStatus);

                    m_adapterDispatch->SetDeviceFailed(m_adapter, queueCreationStatus);
                    m_queueControlCriticalFailure = true;
                    return false;
                }

                return true;
            }

            auto const notStarted = queue->IsCreated() && ! queue->IsStarted();

            if (notStarted)
            {
                queue->Start();

                return true;
            }
        }

        for (auto & queue : m_rxQueues)
        {
            auto const notStarted = ! queue->IsStarted();

            if (notStarted)
            {
                queue->Start();

                return true;
            }
        }

        break;

    case DatapathState::Stopping:
        for (auto & queue : m_txQueues)
        {
            auto const created = queue->IsCreated();
            auto const started = queue->IsStarted();
            auto const stopping = queue->IsStopping();

            if (started && !stopping)
            {
                changed = true;
                queue->Cancel();
            }
            else if (!started)
            {
                // DatapathStop() can race with the creation of an on demand queue,
                // which can leave the queue in the created but not started state
                // OR both not created and not started state - if m_workitem hadn't 
                // had a chance to run at all before m_state was set to DatapathState::Stopping.
                // For such queues we must drop any currently queued NBLs. 
                // Please note, we can't drop packets for "started" queue because we'll be 
                // racing with polling callbacks of ExecutionContext that drives the queues. 
                queue->DropQueuedNetBufferLists();
            }
        }

        for (auto & queue : m_rxQueues)
        {
            auto const notStopping = queue->IsStarted() && ! queue->IsStopping();

            if (notStopping)
            {
                changed = true;
                queue->Cancel();
            }
        }

        break;

    case DatapathState::Stopped:

        for (auto & queue : m_txQueues)
        {
            auto const notStopped = queue->IsStarted();

            if (notStopped)
            {
                NT_FRE_ASSERT(queue->IsStopping());

                changed = true;
                queue->Stop();
            }
        }

        for (auto & queue : m_rxQueues)
        {
            auto const notStopped = queue->IsStarted();

            if (notStopped)
            {
                NT_FRE_ASSERT(queue->IsStopping());

                changed = true;
                queue->Stop();
            }
        }

        break;

    }

    return changed;
}

bool
QueueControl::IsDatapathStateSynchronized(
    void
)
{
    KLockThisExclusive lock(m_workSyncLock);

    switch (m_state)
    {

    case DatapathState::Started:
        if (m_queueControlCriticalFailure)
        {
            // We encountered critical failure when trying to create the queues;
            // Re-enumeration request has been issued, meanwhile ignoring all consequent attempts to create the queues.
            return true;
        }
        for (size_t i = 0u; i < m_txQueues.count(); i++)
        {
            auto & queue = m_txQueues[i];
            auto const notCreated = ! queue->IsCreated() && queue->IsQueuingNetBufferLists();

            if (notCreated)
            {
                return false;
            }

            auto const notStarted = queue->IsCreated() && ! queue->IsStarted();

            if (notStarted)
            {
                return false;
            }
        }

        for (size_t i = 0u; i < m_rxQueues.count(); i++)
        {
            auto & queue = m_rxQueues[i];
            auto const notStarted = ! queue->IsStarted();

            if (notStarted)
            {
                return false;
            }
        }

        break;

    case DatapathState::Stopped:
        for (size_t i = 0u; i < m_txQueues.count(); i++)
        {
            auto & queue = m_txQueues[i];
            auto const started = queue->IsStarted();

            if (started)
            {
                return false;
            }
        }

        for (size_t i = 0u; i < m_rxQueues.count(); i++)
        {
            auto & queue = m_rxQueues[i];
            auto const started = queue->IsStarted();

            if (started)
            {
                return false;
            }
        }

        break;
    }
    
    m_event.Set();

    return true;
}

_Use_decl_annotations_
void
QueueControl::TryQueueWorkItem(
    void
)
{
    // The primary reason for this elaboration is to avoid running functions SynchronizeDatapathState and IsDatapathStateSynchronized
    // from multiple workitems concurrently. These functions acquire pushlocks, held during potentially long operations which can lead to large
    // accumulation of stuck workitems, hogging many threads, severely degrading performance and starving system resources.

    const bool alreadyRunning = (m_loopState == SynchronizationLoop::Running) || (m_loopState == SynchronizationLoop::Stopping);

    if (m_loopState == SynchronizationLoop::Stopping || m_loopState == SynchronizationLoop::Stopped)
    {
        LogInfo(FLAG_TRANSLATOR,
            "m_loopState is set to Running. ClientAdapter: %p",
            m_adapter);

        m_loopState = SynchronizationLoop::Running;
    }

    if (alreadyRunning)
    {
        return;
    }

    m_work.Queue();
}
