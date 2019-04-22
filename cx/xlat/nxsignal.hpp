// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxRingBuffer wraps a NET_RING, providing simple accessor methods
    for inserting and removing items into the ring buffer.

--*/

#pragma once

class NxInterlockedFlag
{
    _Interlocked_ volatile LONG m_flag = false;

public:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void Set() { InterlockedExchange(&m_flag, true); }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool TestAndClear() { return !!InterlockedExchange(&m_flag, false); }
};
