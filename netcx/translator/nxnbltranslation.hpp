// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxRingContext.hpp"
#include "NxDma.hpp"
#include "NxBounceBufferPool.hpp"
#include "NxExtensions.hpp"
#include "NxStatistics.hpp"
#include "metadata/MetadataTranslator.hpp"
#include "checksum/Checksum.hpp"
#include "segmentation/Segmentation.hpp"

#include <KArray.h>

#include <net/virtualaddresstypes_p.h>
#include <net/logicaladdresstypes_p.h>

#include "NxNbl.hpp"
#include "NxPacketLayout.hpp"

typedef struct _PKTMON_EDGE_CONTEXT PKTMON_EDGE_CONTEXT;
typedef struct _PKTMON_COMPONENT_CONTEXT PKTMON_COMPONENT_CONTEXT;

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

public:

    struct PAGED PacketContext
    {
        NET_BUFFER_LIST * NetBufferListToComplete = nullptr;
        AddressType DestinationType;
    };

    NxNblTranslator(
        NET_BUFFER_LIST * & CurrentNetBufferList,
        NET_BUFFER * & CurrentNetBuffer,
        NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES const & DatapathCapabilities,
        TxExtensions const & Extensions,
        NxTxCounters & Statistics,
        NxBounceBufferPool & BouncePool,
        const NxRingContext & ContextBuffer,
        const NxDmaAdapter * DmaAdapter,
        NDIS_MEDIUM MediaType,
        NET_RING_COLLECTION * Rings,
        Checksum & ChecksumContext,
        GenericSegmentationOffload & GsoContext,
        Rtl::KArray<UINT8, NonPagedPoolNx> & LayoutParseBuffer,
        NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES & checksumHardwareCapabilities,
        NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES & txChecksumHardwareCapabilities,
        NET_CLIENT_OFFLOAD_GSO_CAPABILITIES & GsoHardwareCapabilities,
        PKTMON_LOWEREDGE_HANDLE PktMonEdgeContext,
        PKTMON_COMPONENT_HANDLE PktMonComponentContext
    );

    NxNblTranslationStatus
    TranslateNetBufferToNetPacket(
        _Inout_ NET_PACKET * netPacket
    ) const;

    ULONG
    TranslateNbls(
        void
    );

    TxPacketCompletionStatus
    CompletePackets(
        void
    ) const;

    TxPacketCounters const &
    GetCounters(
        void
    ) const;

private:

    NET_BUFFER_LIST * &
        m_currentNetBufferList;

    NET_BUFFER * &
        m_currentNetBuffer;

    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES const &
        m_datapathCapabilities;

    TxExtensions const &
        m_extensions;

    NxTxCounters &
        m_statistics;

    NxBounceBufferPool &
        m_bouncePool;

    NxRingContext const &
        m_contextBuffer;

    mutable
    TxPacketCounters
        m_counters = {};

    NxDmaAdapter const *
        m_dmaAdapter;

    NET_PACKET_LAYER2_TYPE const
        m_layer2Type;

    NET_RING_COLLECTION *
        m_rings;

    TxMetadataTranslator
        m_metadataTranslator;

    size_t
        m_maxFragmentsPerPacket = 0;

    NET_PACKET_GSO
        m_gso = {};

    NET_PACKET_CHECKSUM
        m_checksum = {};

    NET_PACKET_LAYOUT
        m_layout = {};

    NET_PACKET_IEEE8021Q
        m_ieee8021q = {};

    Checksum &
        m_checksumContext;

    GenericSegmentationOffload &
        m_gsoContext;

    Rtl::KArray<UINT8, NonPagedPoolNx> &
        m_layoutParseBuffer;

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const &
        m_checksumHardwareCapabilities;

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const &
        m_txChecksumHardwareCapabilities;

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const &
        m_gsoHardwareCapabilities;

    PKTMON_EDGE_CONTEXT const *
        m_pktmonLowerEdgeContext = nullptr;

    PKTMON_COMPONENT_CONTEXT const *
        m_pktmonComponentContext = nullptr;

    bool
    TranslateLayout(
        void
    );

    void
    TranslateNetBufferListInfo(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    TranslateNetBuffer(
        UINT32 Index
    );

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

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsSoftwareChecksumRequired(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsSoftwareGsoRequired(
        void
    ) const;

    void
    ReplaceCurrentNetBufferList(
        _In_ NET_BUFFER_LIST * NetBufferList
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    GenericSendOffloadFallback(
        void
    );
};
