// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

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

class NxTxCounters
{

public:

    NblTranslationPacketCounter
        Packet = {};

    NblTranslationDMACounter
        DMA = {};

    NblTranslationSoftwareChecksum
        Checksum = {};

    NblTranslationSoftwareSegment
        Segment = {};

};
