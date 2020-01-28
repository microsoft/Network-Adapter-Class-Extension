// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxRingContext.hpp"
#include "NxDma.hpp"
#include "NxBounceBufferPool.hpp"
#include "NxExtensions.hpp"
#include "NxStatistics.hpp"
#include "metadata/MetadataTranslator.hpp"

#include <net/virtualaddresstypes_p.h>
#include <net/logicaladdresstypes_p.h>

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
    MdlTranlationResult(
        NxNblTranslationStatus XlatStatus
    );

    MdlTranlationResult(
        NxNblTranslationStatus XlatStatus,
        UINT32 Index,
        UINT16 Count
    );

    NxNblTranslationStatus const
        Status;

    UINT32 const
        FragmentIndex = 0;

    UINT16 const
        FragmentCount = 0;
};

struct TxPacketCompletionStatus
{
    NET_BUFFER_LIST *
        CompletedChain = nullptr;

    ULONG
        NumCompletedNbls = 0;

    ULONG
        CompletedPackets = 0;

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
    TxExtensions const &
        m_extensions;

    NxNblTranslationStats &
        m_stats;

    NxRingContext const &
        m_contextBuffer;

    NxDmaAdapter const *
        m_dmaAdapter;

    NDIS_MEDIUM const
        m_mediaType;

    NET_RING_COLLECTION *
        m_rings;

    TxMetadataTranslator
        m_metadataTranslator;

    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES const &
        m_datapathCapabilities;

    size_t
        m_maxFragmentsPerPacket;

    NxStatistics &
        m_genStats;

    bool
    RequiresDmaMapping(
        void
    ) const;

    bool
    ShouldBounceFragment(
        _In_ NET_FRAGMENT const * const Fragment,
        _In_ NET_FRAGMENT_VIRTUAL_ADDRESS const * const VirtualAddress,
        _In_ NET_FRAGMENT_LOGICAL_ADDRESS const * const LogicalAddress
    ) const;

    MdlTranlationResult
    TranslateMdlChainToFragmentRangeKvmOnly(
        _In_ MDL & Mdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy
    ) const;

    MdlTranlationResult
    TranslateMdlChainToDmaMappedFragmentRange(
        _In_ MDL & Mdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy,
        _In_ NET_PACKET const & Packet
    ) const;

    MdlTranlationResult
    TranslateMdlChainToDmaMappedFragmentRangeBypassHal(
        _In_ MDL & Mdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy
    ) const;

    MdlTranlationResult
    TranslateMdlChainToDmaMappedFragmentRangeUseHal(
        _In_ MDL & Mdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy,
        _In_ NxDmaTransfer const &DmaTransfer
    ) const;

    bool
    MapMdlChainToSystemVA(
        _Inout_ MDL & Mdl
    ) const;

    MdlTranlationResult
    TranslateScatterGatherListToFragmentRange(
        _In_ MDL & MappedMdl,
        _In_ size_t MdlOffset,
        _In_ size_t BytesToCopy,
        _In_ SCATTER_GATHER_LIST const * Sgl
    ) const;

public:
    struct PAGED PacketContext
    {
        PNET_BUFFER_LIST NetBufferListToComplete = nullptr;
    };

    NxNblTranslator(
        TxExtensions const & Extensions,
        NxNblTranslationStats & Stats,
        NET_RING_COLLECTION * Rings,
        const NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES & DatapathCapabilities,
        const NxDmaAdapter * DmaAdapter,
        const NxRingContext & ContextBuffer,
        NDIS_MEDIUM MediaType,
        NxStatistics & GenStats
    );

    NxNblTranslationStatus
    TranslateNetBufferToNetPacket(
        _In_ NET_BUFFER & netBuffer,
        _Inout_ NET_PACKET * netPacket
    ) const;

    ULONG
    TranslateNbls(
        _Inout_ NET_BUFFER_LIST * & currentNbl,
        _Inout_ NET_BUFFER * & currentNetBuffer,
        _In_ NxBounceBufferPool & BouncePool
    ) const;

    TxPacketCompletionStatus
    CompletePackets(
        _In_ NxBounceBufferPool & BouncePool
    ) const;
};

