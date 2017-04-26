/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxNblQueue.cpp

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
NxNblQueue::Enqueue(PNET_BUFFER_LIST pNbl)
{
    KAcquireSpinLock lock(m_SpinLock);
    ndisAppendNblChainToNblQueue(&m_nblQueue, pNbl);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
PNET_BUFFER_LIST
NxNblQueue::DequeueAll()
{
    KAcquireSpinLock lock(m_SpinLock);
    auto nblChain = ndisPopAllFromNblQueue(&m_nblQueue);

    return nblChain;
}
