// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxRingBufferRange.hpp"
#include "NxContextBuffer.hpp"

enum class NxNblTranslationStatus
{
    Success,
    InsufficientFragments,
    CannotTranslate
};

struct TxPacketCompletionStatus
{
    NetRbPacketIterator CompletedTo;
    NET_BUFFER_LIST *CompletedChain = nullptr;
    UINT32 NumCompletedNbls = 0;

    explicit TxPacketCompletionStatus(
        NetRbPacketIterator it) :
        CompletedTo(it)
    {

    }
};

// This class provides non-mutating operations that plumb NBLs
// into and out of a NET_RING_BUFFER. It should not mutate any state in the course of
// execution.
//
// Since this operates almost exclusively in the datapath, all of its code
// must be non-paged.
//
// It's possible in the future that we might need to have some mutable state, such as a
// NET_PACKET_FRAGMENT pool, but it should be added with a mind for test-ability.
class NONPAGED NxNblTranslator
{
    const NxContextBuffer & m_contextBuffer;
    const NDIS_MEDIUM m_mediaType;

    static
    ULONG
    CountNumberOfFragments(
        _In_ NET_BUFFER const & nb
        );

    void
    TranslateNetBufferListOOBDataToNetPacketExtensions(
        _In_ NET_BUFFER_LIST const &netBufferList,
        _Inout_ NET_PACKET* netPacket
        ) const;

    void
    TranslateNetPacketExtensionsCompletionToNetBufferList(
        _In_ NET_DATAPATH_DESCRIPTOR const* descriptor,
        _In_ const NET_PACKET *netPacket,
        _Inout_ PNET_BUFFER_LIST netBufferList
        ) const;

    bool
    IsPacketChecksumEnabled() const;

    bool
    IsPacketLargeSendSegmentationEnabled() const;

public:
    struct PAGED PacketContext
    {
        PNET_BUFFER_LIST NetBufferListToComplete = nullptr;
    };

    NxNblTranslator(
        const NxContextBuffer & ContextBuffer,
        NDIS_MEDIUM MediaType
        );

    NxNblTranslationStatus
    TranslateNetBufferToNetPacket(
        _In_ NET_BUFFER const &netBuffer,
        _In_ NET_BUFFER_LIST const &netBufferList,
        _In_ NET_DATAPATH_DESCRIPTOR const* descriptor,
        _Inout_ NET_PACKET* netPacket
        ) const;

    NetRbPacketIterator
    TranslateNbls(
        _Inout_ NET_BUFFER_LIST *&currentNbl,
        _Inout_ NET_BUFFER *&currentNetBuffer,
        _In_ NET_DATAPATH_DESCRIPTOR const* descriptor,
        _In_ NetRbPacketRange const &rb
        ) const;

    TxPacketCompletionStatus
    CompletePackets(
        _In_ NET_DATAPATH_DESCRIPTOR const* descriptor,
        _In_ NetRbPacketRange const &rb
        ) const;

    // packet extension offsets
    size_t m_netPacketChecksumOffset = NET_PACKET_EXTENSION_INVALID_OFFSET;
    size_t m_netPacketLsoOffset = NET_PACKET_EXTENSION_INVALID_OFFSET;
};
