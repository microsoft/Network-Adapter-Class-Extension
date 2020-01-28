// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxNblQueue is a FIFO queues of NET_BUFFER_LISTs, with
    built-in synchronization.

--*/

#pragma once

#include <KSpinLock.h>
#include <nblutil.h>

class NxNblQueue
{
public:

    PAGED NxNblQueue();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void Enqueue(_In_ PNET_BUFFER_LIST pNbl);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void Enqueue(_Inout_ NBL_COUNTED_QUEUE *queue);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void DequeueAll(_Out_ NBL_QUEUE *destination);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_BUFFER_LIST *DequeueAll();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG64 GetNblQueueDepth() const;

private:

    NBL_COUNTED_QUEUE m_nblQueue;
    KSpinLock m_SpinLock;
};
