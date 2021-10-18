// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxStatistics.tmh"
#include "NxStatistics.hpp"

_Use_decl_annotations_
void
NxRxCounters::Add(
    _In_ const NxRxCounters & rxCounters
)
{
    Perf.IterationCount += rxCounters.Perf.IterationCount;
    Perf.QueueDepth += rxCounters.Perf.QueueDepth;
    Perf.NblPending += rxCounters.Perf.NblPending;
    Perf.PacketsCompleted += rxCounters.Perf.PacketsCompleted;
}

_Use_decl_annotations_
void
NxRxCounters::GetPerfCounter(NETADAPTER_QUEUE_PC & perfCounter) const
{
    perfCounter.IterationCount = Perf.IterationCount;
    perfCounter.QueueDepth = Perf.QueueDepth;
    perfCounter.NblPending = Perf.NblPending;
    perfCounter.PacketsCompleted = Perf.PacketsCompleted;
    perfCounter.IterationCountBase = static_cast<UINT32>(perfCounter.IterationCount);
}

_Use_decl_annotations_
void
NxTxCounters::Add(
    _In_ const NxTxCounters & txCounters
)
{
    Perf.IterationCount += txCounters.Perf.IterationCount;
    Perf.QueueDepth += txCounters.Perf.QueueDepth;
    Perf.NblPending += txCounters.Perf.NblPending;
    Perf.PacketsCompleted += txCounters.Perf.PacketsCompleted;

    Packet.BounceSuccess += txCounters.Packet.BounceSuccess;
    Packet.BounceFailure += txCounters.Packet.BounceFailure;
    Packet.CannotTranslate += txCounters.Packet.CannotTranslate;
    Packet.UnalignedBuffer += txCounters.Packet.UnalignedBuffer;

    DMA.InsufficientResources += txCounters.DMA.InsufficientResources;
    DMA.BufferTooSmall += txCounters.DMA.BufferTooSmall;
    DMA.CannotMapSglToFragments += txCounters.DMA.CannotMapSglToFragments;
    DMA.PhysicalAddressTooLarge += txCounters.DMA.PhysicalAddressTooLarge;
    DMA.OtherErrors += txCounters.DMA.OtherErrors;
}

_Use_decl_annotations_
void
NxTxCounters::GetPerfCounter(NETADAPTER_QUEUE_PC & perfCounter) const
{
    perfCounter.IterationCount = Perf.IterationCount;
    perfCounter.QueueDepth = Perf.QueueDepth;
    perfCounter.NblPending = Perf.NblPending;
    perfCounter.PacketsCompleted = Perf.PacketsCompleted;
    perfCounter.IterationCountBase = static_cast<UINT32>(perfCounter.IterationCount);
}

_Use_decl_annotations_
const NxTxCounters &
NxStatistics::GetTxCounters(
    void
) const
{
    return m_txStatistics;
}

_Use_decl_annotations_
const NxRxCounters &
NxStatistics::GetRxCounters(
    void
) const
{
    return m_rxStatistics;
}

_Use_decl_annotations_
void
NxStatistics::UpdateRxCounters(
    _In_ const NxRxCounters & rxCounters
)
{
    m_rxStatistics.Add(rxCounters);
}

_Use_decl_annotations_
void
NxStatistics::UpdateTxCounters(
    _In_ const NxTxCounters & txCounters
)
{
    m_txStatistics.Add(txCounters);
}
