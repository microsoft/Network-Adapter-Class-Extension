// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

enum class NxStatisticsCounters {
// OID_GEN_STATISTICS
    NumberOfPackets,
    BytesOfData,
    NumberOfErrors,
// Queue Perf Counter
    IterationCount,     // # of times polling loop runs
    QueueDepth,         // # of packets own by the client driver
    NblPending,         // # of NBL pending
    PacketsCompleted,   // # of packets done processing
    NumberofStatisticsCounters
};

struct NETADAPTER_QUEUE_PC
{
    UINT64 NumberOfPackets;
    UINT64 Reserved0;
    UINT64 Reserved1;
    UINT64 IterationCount; 
    UINT64 QueueDepth;   
    UINT64 NblPending;       
    UINT64 PacketsCompleted;
    UINT32 IterationCountBase;
    UINT32 Reserved2;
};

// sizeof(NxStatisticsCounters) must be multiple of cacheline size to avoid false sharing
class DECLSPEC_CACHEALIGN NxStatistics
{

private:

    static ULONG 
    	s_LastStatId;

    ULONG64
        m_statistics[static_cast<int>(NxStatisticsCounters::NumberofStatisticsCounters)] = {};

    static_assert(sizeof(NETADAPTER_QUEUE_PC) >= sizeof(m_statistics),
                  "NETADAPTER_QUEUE_PC must be large enough to store all counters");
public:

    const ULONG 
    	m_StatId = InterlockedIncrement((LONG *) &s_LastStatId);

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Increment(
        _In_ NxStatisticsCounters counterType
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    IncrementBy(
        _In_ NxStatisticsCounters counterType,
        _In_ ULONG64 count
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    ULONG64
    GetCounter(
        _In_ NxStatisticsCounters counterType
    ) const;

    void
    GetPerfCounter(
        _Out_ NETADAPTER_QUEUE_PC* perfCounter
    ) const;
};

static_assert(IS_ALIGNED(sizeof(NxStatistics), SYSTEM_CACHE_ALIGNMENT_SIZE),
              "NxStatistics must be multiple of cacheline size");
