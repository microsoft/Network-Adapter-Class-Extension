// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxStatistics.tmh"
#include "NxStatistics.hpp"

ULONG NxStatistics::s_LastStatId = 0;

_Use_decl_annotations_
void
NxStatistics::Increment(
    NxStatisticsCounters counterType
)
{
    InterlockedIncrementNoFence64((LONG64 *)&m_statistics[static_cast<int>(counterType)]);
}

_Use_decl_annotations_
void
NxStatistics::IncrementBy(
    NxStatisticsCounters counterType,
    ULONG64 count
)
{
    InterlockedAddNoFence64((LONG64 *)&m_statistics[static_cast<int>(counterType)], count);
}

_Use_decl_annotations_
ULONG64
NxStatistics::GetCounter(
    NxStatisticsCounters counterType
) const
{
    return ReadULong64NoFence(&m_statistics[static_cast<int>(counterType)]);
}

_Use_decl_annotations_
void
NxStatistics::GetPerfCounter(NETADAPTER_QUEUE_PC* perfCounter) const
{
    RtlCopyMemory(perfCounter, m_statistics, sizeof(m_statistics));
    perfCounter->IterationCountBase = (UINT32) perfCounter->IterationCount;
}
