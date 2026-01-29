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


class ReceiveSegmentCoalescingContext
{

friend class ReceiveSegmentCoalescingBatch;

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    ReceiveSegmentCoalescingContext(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    PerformReceiveSegmentUncoalescing(
        _Inout_ NET_BUFFER_LIST * & NblChain,
        _Out_ NET_BUFFER_LIST * & OutputNblChainTail,
        _Out_ ULONG & NumberOfNbls
    );

private:

    RSCLIB_SERIAL_COALESCING_CONTEXT
        m_rsclibContext = {};
};


class ReceiveSegmentCoalescingBatch
{

public:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ReceiveSegmentCoalescingBatch(
        _In_ ReceiveSegmentCoalescingContext & RsclibContext,
        _In_ bool IsRscEnabled,
        _In_ bool IsIpv4HardwareRscSupported,
        _In_ bool IsIpv6HardwareRscSupported,
        _In_ bool IsUdpRscEnabled,
        _In_ bool IsHardwareUdpRscSupported,
        _In_ NDIS_MEDIUM MediaType
        );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    TryCoalesceOneReceiveSegment(
        _In_ NET_BUFFER_LIST * NetBufferList,
        _Inout_ NET_BUFFER_LIST ** & TailOfNblChain,
        _Inout_ ULONG & NumberOfNbls
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    FinalizeCoalescing(
        _Inout_ NET_BUFFER_LIST ** & TailOfNblChain,
        _Inout_ ULONG & NumberOfNbls
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

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    GetNblsAttemptedCount(
        void
    ) const;

private:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsNetBufferListCoalescable(
        _In_ NET_BUFFER_LIST const * Nbl,
        _Out_ UCHAR * IpHeaderOffset
    );

    RSCLIB_SERIAL_COALESCING_CONTEXT *
        m_pRsclibContext;

    RSCLIB_STATS
        m_rscStats = {};

    ULONG
        m_nblsAttemptedCount = 0;

    bool const
        m_isIpv4HardwareRscSupported;

    bool const
        m_isIpv6HardwareRscSupported;

    bool const
        m_isHardwareUdpRscSupported;

    bool
        m_shouldCoalesce = false;

    UCHAR
        m_coalesceFlags;

    NDIS_MEDIUM
        m_mediaType{};
};
