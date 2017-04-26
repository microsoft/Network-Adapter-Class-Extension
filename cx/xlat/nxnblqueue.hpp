/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxNblQueue.hpp

Abstract:

    The NxNblQueue is a FIFO queues of NET_BUFFER_LISTs, with 
    built-in synchronization.

--*/

#pragma once

#include <KSpinLock.h>

class NxNblQueue
{
public:

    PAGED NxNblQueue();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void Enqueue(_In_ PNET_BUFFER_LIST pNbl);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PNET_BUFFER_LIST DequeueAll();

private:

    NBL_QUEUE m_nblQueue;
    KSpinLock m_SpinLock;
};
