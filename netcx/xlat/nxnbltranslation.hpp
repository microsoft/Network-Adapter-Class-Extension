// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxRingBufferRange.hpp"
#include "NxRingContext.hpp"
#include "NxDma.hpp"
#include "NxScatterGatherList.hpp"
#include "NxBounceBufferPool.hpp"

struct NxNblTranslationStats
{
    struct
    {
        UINT64 BounceSuccess = 0;
        UINT64 BounceFailure = 0;
        UINT64 CannotTranslate = 0;
        UINT64 UnalignedBuffer = 0;
    } Packet;

    struct
    {
        UINT64 InsufficientResouces = 0;
        UINT64 BufferTooSmall = 0;
        UINT64 CannotMapSglToFragments = 0;
        UINT64 PhysicalAddressTooLarge = 0;
        UINT64 OtherErrors = 0;
    } DMA;
};

enum class NxNblTranslationStatus
{
    Success,
    InsufficientResources,
    BounceRequired,
    CannotTranslate
};

struct MdlTranlationResult
{
    NxNblTranslationStatus Status;
    NetRbFragmentRange FragmentChain;
};

struct TxPacketCompletionStatus
{
    NET_BUFFER_LIST *
        CompletedChain = nullptr;

    ULONG
        NumCompletedNbls = 0;

    bool
        CompletedPackets = false;

};

// This class provides non-mutating operations that plumb NBLs
// into and out of a NET_RING. It should not mutate any state in the course of
// execution. The exception to this rule is the m_stats member, used to keep track
// of NBL translation statistics
//
// Since this operates almost exclusively in the datapath, all of its code
// must be non-paged.
//
// It's possible in the future that we might need to have some mutable state, such as a
// NET_FRAGMENT pool, but it should be added with a mind for test-ability.
class NONPAGED NxNblTranslator
{
    NxNblTranslationStats &m_stats;
    const NxRingContext & m_contextBuffer;
    const NxDmaAdapter * m_dmaAdapter;
    const NDIS_MEDIUM m_mediaType;
    NET_RING_COLLECTION * m_rings;
    const NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES & m_datapathCapabilities;

    void
    TranslateNetBufferListOOBDataToNetPacketExtensions(
        _In_ NET_BUFFER_LIST const &netBufferList,
        _Inout_ NET_PACKET* netPacket,
        _In_ UINT32 packetIndex
    ) const;

    void
    TranslateNetPacketExtensionsCompletionToNetBufferList(
        _In_ const NET_PACKET *netPacket,
        _Inout_ PNET_BUFFER_LIST netBufferList
    ) const;

    bool
    IsPacketChecksumEnabled() const;

    bool
    IsPacketLargeSendSegmentationEnabled() const;

    bool
    RequiresDmaMapping(
        void
    ) const;

    NetRbFragmentRange
    EmptyFragmentRange(
        void
    ) const;

    size_t
    MaximumNumberOfFragments(
        void
    ) const;

    bool
    ShouldBounceFragment(
        _In_ NET_FRAGMENT const & Fragment
    ) const;

    MdlTranlationResult
    TranslateMdlChainToFragmentRangeKvmOnly(
        _In_ MDL &Mdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy,
        _In_ NetRbFragmentRange const &AvailableFragments
    ) const;

    MdlTranlationResult
    TranslateMdlChainToDmaMappedFragmentRange(
        _In_ MDL &Mdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy,
        _In_ NET_PACKET const &Packet,
        _In_ NetRbFragmentRange const &AvailableFragments
    ) const;

    MdlTranlationResult
    TranslateMdlChainToDmaMappedFragmentRangeBypassHal(
        _In_ MDL &Mdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy,
        _In_ NetRbFragmentRange const &AvailableFragments
    ) const;

    MdlTranlationResult
    TranslateMdlChainToDmaMappedFragmentRangeUseHal(
        _In_ MDL &Mdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy,
        _In_ NxDmaTransfer const &DmaTransfer,
        _In_ NetRbFragmentRange const &AvailableFragments
    ) const;

    bool
    MapMdlChainToSystemVA(
        _Inout_ MDL &Mdl
    ) const;

    MdlTranlationResult
    TranslateScatterGatherListToFragmentRange(
        _In_ MDL &MappedMdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy,
        _In_ NxScatterGatherList const &Sgl,
        _In_ NetRbFragmentRange const &AvailableFragments
    ) const;

public:
    struct PAGED PacketContext
    {
        PNET_BUFFER_LIST NetBufferListToComplete = nullptr;
    };

    NxNblTranslator(
        NxNblTranslationStats &Stats,
        NET_RING_COLLECTION * Rings,
        const NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES & DatapathCapabilities,
        const NxDmaAdapter *DmaAdapter,
        const NxRingContext & ContextBuffer,
        NDIS_MEDIUM MediaType
    );

    NxNblTranslationStatus
    TranslateNetBufferToNetPacket(
        _In_ NET_BUFFER &netBuffer,
        _Inout_ NET_PACKET* netPacket
    ) const;

    bool
    TranslateNbls(
        _Inout_ NET_BUFFER_LIST *&currentNbl,
        _Inout_ NET_BUFFER *&currentNetBuffer,
        _In_ NxBounceBufferPool &BouncePool
    ) const;

    TxPacketCompletionStatus
    CompletePackets(
        _In_ NxBounceBufferPool &BouncePool
    ) const;

    // packet extension offsets
    NET_EXTENSION m_netPacketChecksumExtension = {};
    NET_EXTENSION m_netPacketLsoExtension = {};
};
