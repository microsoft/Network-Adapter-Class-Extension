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

PAGED
NxNblQueue::NxNblQueue()
{
    ndisInitializeNblQueue(&m_nblQueue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
NxNblQueue::Enqueue(_In_ PNET_BUFFER_LIST pNbl)
{
    auto lastNbl = ndisLastNblInNblChain(pNbl);

    KAcquireSpinLock lock(m_SpinLock);
    ndisAppendNblChainToNblQueueFast(&m_nblQueue, pNbl, lastNbl);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
NxNblQueue::Enqueue(_Inout_ NBL_QUEUE *queue)
{
    KAcquireSpinLock lock(m_SpinLock);
    ndisAppendNblQueueToNblQueueFast(&m_nblQueue, queue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
NxNblQueue::DequeueAll(
    _Out_ NBL_QUEUE *destination)
{
    KAcquireSpinLock lock(m_SpinLock);
    ndisInitializeNblQueue(destination);
    ndisAppendNblQueueToNblQueueFast(destination, &m_nblQueue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NET_BUFFER_LIST *
NxNblQueue::DequeueAll()
{
    KAcquireSpinLock lock(m_SpinLock);
    return ndisPopAllFromNblQueue(&m_nblQueue);
}
