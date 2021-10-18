// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxNblQueue is a FIFO queues of NET_BUFFER_LISTs, with
    built-in synchronization.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxNblQueue.tmh"
#include "NxNblQueue.hpp"
#include <ndis/nblchain.h>

PAGED
NxNblQueue::NxNblQueue()
{
    NdisInitializeNblCountedQueue(&m_nblQueue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
NxNblQueue::Enqueue(_In_ PNET_BUFFER_LIST pNbl)
{
    SIZE_T nblCount = 0;
    auto lastNbl = NdisLastNblInNblChainWithCount(pNbl, &nblCount);

    KAcquireSpinLock lock(m_SpinLock);
    m_nblQueue.NblCount += nblCount;
    NdisAppendNblChainToNblQueueFast(&m_nblQueue.Queue, pNbl, lastNbl);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
NxNblQueue::Enqueue(_Inout_ NBL_COUNTED_QUEUE *queue)
{
    KAcquireSpinLock lock(m_SpinLock);
    NdisAppendNblQueueToNblQueueFast(&m_nblQueue.Queue, &(queue->Queue));
    m_nblQueue.NblCount += queue->NblCount;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
NxNblQueue::DequeueAll(
    _Out_ NBL_QUEUE *destination)
{
    KAcquireSpinLock lock(m_SpinLock);
    NdisInitializeNblQueue(destination);
    NdisAppendNblQueueToNblQueueFast(destination, &m_nblQueue.Queue);
    m_nblQueue.NblCount = 0;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NET_BUFFER_LIST *
NxNblQueue::DequeueAll()
{
    KAcquireSpinLock lock(m_SpinLock);
    m_nblQueue.NblCount = 0;
    return NdisPopAllFromNblQueue(&m_nblQueue.Queue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG64
NxNblQueue::GetNblQueueDepth() const
{
    return m_nblQueue.NblCount;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NET_BUFFER_LIST const *
NxNblQueue::PeekNbl() const
{
    if (m_nblQueue.NblCount > 0U)
    {
        return m_nblQueue.Queue.First;
    }

    return nullptr;
}
