// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

#ifdef _KERNEL_MODE

#include <rsclib.h>

#else

using RSCLIB_SERIAL_COALESCING_CONTEXT = HANDLE;
using RSCLIB_STATS = HANDLE;

#endif

struct _NET_BUFFER_LIST;
typedef struct _NET_BUFFER_LIST NET_BUFFER_LIST;


class ReceiveSegmentCoalescing
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Initialize(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    PerformReceiveSegmentCoalescing(
        _Inout_ NET_BUFFER_LIST * & NblChain,
        _In_ bool Ipv4HardwareCapabilities,
        _In_ bool Ipv6HardwareCapabilities,
        _Out_ NET_BUFFER_LIST * & OutputNblChainTail,
        _Out_ ULONG & NumberOfNbls
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    PerformReceiveSegmentUncoalescing(
        _Inout_ NET_BUFFER_LIST * & NblChain,
        _Out_ NET_BUFFER_LIST * & OutputNblChainTail,
        _Out_ ULONG & NumberOfNbls
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    USHORT
    GetNblsCoalescedCount(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    USHORT
    GetScusGeneratedCount(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    GetBytesCoalescedCount(
        void
    ) const;

private:

    RSCLIB_SERIAL_COALESCING_CONTEXT
        m_rsclibContext = {};

    RSCLIB_STATS
        m_rscStats = {};

};
