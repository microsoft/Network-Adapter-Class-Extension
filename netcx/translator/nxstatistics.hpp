// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

struct NETADAPTER_QUEUE_PC
{
    UINT64 NumberOfPackets;
    UINT64 IterationCount;
    UINT64 QueueDepth;
    UINT64 NblPending;
    UINT64 PacketsCompleted;
    UINT32 IterationCountBase;
};

struct PerfCounter
{
    UINT64 IterationCount = 0;     // # of times polling loop runs
    UINT64 QueueDepth = 0;         // # of packets own by the client driver
    UINT64 NblPending = 0;         // # of NBL pending
    UINT64 PacketsCompleted = 0;   // # of packets done processing
};

struct NblTranslationPacketCounter
{
    UINT64 BounceSuccess = 0;
    UINT64 BounceFailure = 0;
    UINT64 CannotTranslate = 0;
    UINT64 UnalignedBuffer = 0;
};

struct NblTranslationSoftwareChecksum
{
    UINT64 Required = 0;
    UINT64 Failure = 0;     // Success = Required - Failure
};

struct NblTranslationSoftwareSegment
{
    UINT64 TcpSuccess = 0;
    UINT64 TcpFailure = 0;
    UINT64 UdpSuccess = 0;
    UINT64 UdpFailure = 0;
};

struct NblTranslationDMACounter
{
    UINT64 InsufficientResources = 0;
    UINT64 BufferTooSmall = 0;
    UINT64 CannotMapSglToFragments = 0;
    UINT64 PhysicalAddressTooLarge = 0;
    UINT64 OtherErrors = 0;
};

class NxRxCounters
{

public:

    PerfCounter
        Perf = {};

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Add(
        _In_ const NxRxCounters & rxCounters
    );

    void
    GetPerfCounter(
        _Out_ NETADAPTER_QUEUE_PC & perfCounter
    ) const;

};

class NxTxCounters
{

public:

    PerfCounter
        Perf = {};

    NblTranslationPacketCounter
        Packet = {};

    NblTranslationDMACounter
        DMA = {};

    NblTranslationSoftwareChecksum
        Checksum = {};

    NblTranslationSoftwareSegment
        Segment = {};

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Add(
        _In_ const NxTxCounters & txCounters
    );

    void
    GetPerfCounter(
        _Out_ NETADAPTER_QUEUE_PC & perfCounter
    ) const;

};

class DECLSPEC_CACHEALIGN NxStatistics
{

private:

    NxRxCounters
        m_rxStatistics = {};

    NxTxCounters
        m_txStatistics = {};

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    const NxTxCounters &
    GetTxCounters(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    const NxRxCounters &
    GetRxCounters(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UpdateRxCounters(
        _In_ const NxRxCounters & rxCounters
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UpdateTxCounters(
        _In_ const NxTxCounters & txCounters
    );
};
