// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxNblDatapath.hpp"

#include "NxNblTranslation.tmh"
#include "NxNblTranslation.hpp"

#include <net/fragment.h>
#include <net/checksum_p.h>
#include <net/logicaladdress_p.h>
#include <net/gso_p.h>
#include <net/mdl_p.h>
#include <net/rsc_p.h>
#include <net/virtualaddress_p.h>
#include <net/ieee8021q_p.h>

#include "NxStatistics.hpp"
#include "framelayout/LayoutParser.hpp"
#include "NxTxNblContext.hpp"

#include <netiodef.h>
#include <pktmonclnt.h>
#include <pktmonloc.h>

static
bool
TcpHeaderContainsOptions(
    _In_ NET_PACKET_LAYOUT const & Layout
)
{
    NT_ASSERT(Layout.Layer4Type == NetPacketLayer4TypeTcp);
    return Layout.Layer4HeaderLength > 20U;
}

MdlTranlationResult::MdlTranlationResult(
    NxNblTranslationStatus XlatStatus
)
    : Status(XlatStatus)
{
    NT_FRE_ASSERT(Status != NxNblTranslationStatus::Success);
}

MdlTranlationResult::MdlTranlationResult(
    NxNblTranslationStatus XlatStatus,
    UINT32 Index,
    UINT16 Count
)
    : Status(XlatStatus)
    , FragmentIndex(Index)
    , FragmentCount(Count)
{
    NT_FRE_ASSERT(Status == NxNblTranslationStatus::Success);
}

NxNblTranslator::NxNblTranslator(
    NET_BUFFER_LIST * & CurrentNetBufferList,
    NET_BUFFER * & CurrentNetBuffer,
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES const & DatapathCapabilities,
    TxExtensions const & Extensions,
    NxTxCounters & Statistics,
    NxBounceBufferPool & BouncePool,
    const NxRingContext & ContextBuffer,
    const NxDmaAdapter * DmaAdapter,
    NDIS_MEDIUM Medium,
    NET_RING_COLLECTION * Rings,
    Checksum & ChecksumContext,
    GenericSegmentationOffload & GsoContext,
    Rtl::KArray<UINT8, NonPagedPoolNx> & LayoutParseBuffer,
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES & ChecksumHardwareCapabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES & TxChecksumHardwareCapabilities,
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES & GsoHardwareCapabilities,
    PKTMON_LOWEREDGE_HANDLE PktMonLowerEdgeContext,
    PKTMON_COMPONENT_HANDLE PktMonComponentContext
)
    : m_currentNetBufferList(CurrentNetBufferList)
    , m_currentNetBuffer(CurrentNetBuffer)
    , m_datapathCapabilities(DatapathCapabilities)
    , m_extensions(Extensions)
    , m_statistics(Statistics)
    , m_bouncePool(BouncePool)
    , m_contextBuffer(ContextBuffer)
    , m_dmaAdapter(DmaAdapter)
    , m_layer2Type(TranslateMedium(Medium))
    , m_rings(Rings)
    , m_metadataTranslator(*m_rings, m_extensions)
    , m_checksumContext(ChecksumContext)
    , m_gsoContext(GsoContext)
    , m_layoutParseBuffer(LayoutParseBuffer)
    , m_checksumHardwareCapabilities(ChecksumHardwareCapabilities)
    , m_txChecksumHardwareCapabilities(TxChecksumHardwareCapabilities)
    , m_gsoHardwareCapabilities(GsoHardwareCapabilities)
    , m_pktmonLowerEdgeContext(reinterpret_cast<PKTMON_EDGE_CONTEXT const *>(PktMonLowerEdgeContext))
    , m_pktmonComponentContext(reinterpret_cast<PKTMON_COMPONENT_CONTEXT const *>(PktMonComponentContext))
{
    //
    // The maximum number of fragments per packet is either the maximum number
    // of fragments the NIC driver supports or the maximum number of elements
    // in the fragment ring, whichever is smaller
    //
    m_maxFragmentsPerPacket = min(
        m_datapathCapabilities.MaximumNumberOfTxFragments,
        NetRingCollectionGetFragmentRing(m_rings)->NumberOfElements - 1);

    //
    // Also take in account NET_PACKET::FragmentCount being an UINT16
    //
    m_maxFragmentsPerPacket = min(m_maxFragmentsPerPacket, UINT16_MAX);
}

bool
NxNblTranslator::RequiresDmaMapping(
    void
) const
{
    return m_datapathCapabilities.TxMemoryConstraints.MappingRequirement
        == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED;
}

_Use_decl_annotations_
bool
NxNblTranslator::IsSoftwareChecksumRequired(
    void
) const
{
    // No need for software checksum if neither layer 3 nor layer 4 requires checksum.
    if (m_checksum.Layer4 != NetPacketTxChecksumActionRequired &&
        m_checksum.Layer3 != NetPacketTxChecksumActionRequired)
    {
        return false;
    }

    // Don't perform software offload for Checksum v1
    if (m_checksumHardwareCapabilities.IPv4 == TRUE ||
        m_checksumHardwareCapabilities.Tcp == TRUE ||
        m_checksumHardwareCapabilities.Udp == TRUE)
    {
        return false;
    }

    if (m_txChecksumHardwareCapabilities.Layer3Flags == 0x0 &&
        m_txChecksumHardwareCapabilities.Layer4Flags == 0x0)
    {
        return true;
    }

    // Software checksum is required if
    // 1) Hardware has any layer 4 limitation
    if (m_checksum.Layer4 == NetPacketTxChecksumActionRequired)
    {
        switch (m_layout.Layer4Type)
        {
            case NetPacketLayer4TypeTcp:
                {
                    auto const tcpHeaderContainsOptions = TcpHeaderContainsOptions(m_layout);
                    if (tcpHeaderContainsOptions &&
                        WI_IsFlagClear(m_txChecksumHardwareCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions))
                    {
                        return true;
                    }

                    if (! tcpHeaderContainsOptions &&
                        WI_IsFlagClear(m_txChecksumHardwareCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions))
                    {
                        return true;
                    }
                }
                break;

            case NetPacketLayer4TypeUdp:
                if (WI_IsFlagClear(m_txChecksumHardwareCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
                {
                    return true;
                }
                break;

            case NetPacketLayer4TypeUnspecified:
            case NetPacketLayer4TypeIPFragment:
            case NetPacketLayer4TypeIPNotFragment:
                break;

        }
    }

    // 2) Hardware has any layer 3 limitation
    if (m_checksum.Layer3 == NetPacketTxChecksumActionRequired ||
        m_checksum.Layer4 == NetPacketTxChecksumActionRequired)
    {
        switch (m_layout.Layer3Type)
        {
            case NetPacketLayer3TypeIPv4NoOptions:
                if (WI_IsFlagClear(m_txChecksumHardwareCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
                {
                    return true;
                }
                break;

            case NetPacketLayer3TypeIPv4WithOptions:
                if (WI_IsFlagClear(m_txChecksumHardwareCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions))
                {
                    return true;
                }
                break;

            case NetPacketLayer3TypeIPv6NoExtensions:
                if (WI_IsFlagClear(m_txChecksumHardwareCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
                {
                    return true;
                }
                break;

            case NetPacketLayer3TypeIPv6WithExtensions:
                if (WI_IsFlagClear(m_txChecksumHardwareCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions))
                {
                    return true;
                }
                break;

            case NetPacketLayer3TypeUnspecified:
            case NetPacketLayer3TypeIPv4UnspecifiedOptions:
            case NetPacketLayer3TypeIPv6UnspecifiedExtensions:
                return false;
        }
    }

    // 3) Hardware has any header offset limitation
    auto const layer4HeaderOffset = m_layout.Layer2HeaderLength + m_layout.Layer3HeaderLength;
    if (m_txChecksumHardwareCapabilities.Layer4HeaderOffsetLimit != 0 &&
        m_txChecksumHardwareCapabilities.Layer4HeaderOffsetLimit < layer4HeaderOffset)
    {
        return true;
    }

    if (m_txChecksumHardwareCapabilities.Layer3HeaderOffsetLimit != 0 &&
        m_txChecksumHardwareCapabilities.Layer3HeaderOffsetLimit < m_layout.Layer2HeaderLength)
    {
        return true;
    }

    return false;
}

_Use_decl_annotations_
NxNblTranslationStatus
NxNblTranslator::TranslateNetBufferToNetPacket(
    NET_PACKET * netPacket
) const
{
    if (IsSoftwareChecksumRequired())
    {
        InterlockedIncrementNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Checksum.Required));

        auto const & info = *reinterpret_cast<NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const *>(
            &m_currentNetBufferList->NetBufferListInfo[TcpIpChecksumNetBufferListInfo]);

        if (STATUS_SUCCESS != m_checksumContext.CalculateChecksum(m_currentNetBuffer, info, m_layout))
        {
            InterlockedIncrementNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Checksum.Failure));

            PKTMON_DROP_NBL(
                const_cast<PKTMON_COMPONENT_CONTEXT *>(m_pktmonComponentContext),
                m_currentNetBufferList,
                PktMonDir_Out,
                PktMonDrop_NetCx_SoftwareChecksumFailure,
                PMLOC_NETCX_SOFTWARE_CHECKSUM_FAILURE);

            return NxNblTranslationStatus::CannotTranslate;
        }
    }

    auto backfill = static_cast<ULONG>(m_datapathCapabilities.TxPayloadBackfill);

    if (backfill > 0)
    {
        // Check if there is enough space in the NET_BUFFER's current MDL to satisfy
        // the client driver's required backfill
        if (backfill > NET_BUFFER_CURRENT_MDL_OFFSET(m_currentNetBuffer))
        {
            // In the future we can try to bounce only the first fragment,
            // for now bounce the whole packet

            return NxNblTranslationStatus::BounceRequired;
        }

        // Retreat the used data in the current MDL so that any mapping operations
        // take in account the NIC backfill space in the NET_PACKET payload
#if DBG
        auto ndisStatus =
#endif
            NdisRetreatNetBufferDataStart(
                m_currentNetBuffer,
                backfill,
                0,
                nullptr);

#if DBG
        // Because we already checked the current MDL has enough space to retreat the
        // NET_BUFFER's data start by 'backfill' we're sure this will never fail
        NT_ASSERT(ndisStatus == NDIS_STATUS_SUCCESS);
#endif
    }

    // Make sure we advance the NET_BUFFER's data start on return of this method
    auto advanceDataStart = wil::scope_exit([&CurrentNetBuffer = m_currentNetBuffer, backfill]()
    {
        // Avoid calling into NDIS if backfill is zero
        if (backfill > 0)
        {
            NdisAdvanceNetBufferDataStart(
                CurrentNetBuffer,
                backfill,
                TRUE,
                nullptr);
        }
    });

    PMDL mdl = NET_BUFFER_CURRENT_MDL(m_currentNetBuffer);
    size_t mdlOffset = NET_BUFFER_CURRENT_MDL_OFFSET(m_currentNetBuffer);
    auto bytesToCopy = NET_BUFFER_DATA_LENGTH(m_currentNetBuffer);

    if (bytesToCopy == 0 || bytesToCopy > m_datapathCapabilities.MaximumTxFragmentSize)
    {
        PKTMON_DROP_NBL(
            const_cast<PKTMON_COMPONENT_CONTEXT *>(m_pktmonComponentContext),
            m_currentNetBufferList,
            PktMonDir_Out,
            PktMonDrop_NetCx_InvalidNetBufferLength,
            PMLOC_NETCX_INVALID_NETBUFFER_LENGTH);

        return NxNblTranslationStatus::CannotTranslate;
    }

    auto const result =
        RequiresDmaMapping()
            ? TranslateMdlChainToDmaMappedFragmentRange(*mdl, mdlOffset, bytesToCopy, *netPacket)
            : TranslateMdlChainToFragmentRangeKvmOnly(*mdl, mdlOffset, bytesToCopy);

    if (result.Status == NxNblTranslationStatus::Success &&
        result.FragmentCount > 0)
    {
        auto fr = NetRingCollectionGetFragmentRing(m_rings);

        // Commit the fragment chain to the packet
        netPacket->FragmentCount = result.FragmentCount;
        netPacket->FragmentIndex = result.FragmentIndex;
        fr->EndIndex = (fr->EndIndex + result.FragmentCount) & fr->ElementIndexMask;
    }

    return result.Status;
}

inline
bool
IsAddressAligned(
    _In_ ULONG_PTR Address,
    _In_ size_t Alignment
)
{
    return (Address & (Alignment - 1)) == 0;
}

_Use_decl_annotations_
bool
NxNblTranslator::ShouldBounceFragment(
    NET_FRAGMENT const * const Fragment,
    NET_FRAGMENT_VIRTUAL_ADDRESS const * const VirtualAddress,
    NET_FRAGMENT_LOGICAL_ADDRESS const * const LogicalAddress
) const
{
    auto const alignment = m_datapathCapabilities.TxMemoryConstraints.AlignmentRequirement;
    auto const maxPhysicalAddress = m_datapathCapabilities.TxMemoryConstraints.Dma.MaximumPhysicalAddress;

    auto const address =
        reinterpret_cast<ULONG_PTR>(VirtualAddress->VirtualAddress) +
        static_cast<ULONG_PTR>(Fragment->Offset);

    if (!IsAddressAligned(address, alignment))
    {
        InterlockedIncrementNoFence64(
            reinterpret_cast<LONG64 *>(&m_statistics.Packet.UnalignedBuffer));

        return true;
    }

    auto const maxLogicalAddress = static_cast<UINT64>(maxPhysicalAddress.QuadPart);
    auto const checkMaxPhysicalAddress = RequiresDmaMapping() && maxLogicalAddress != 0;
    if (checkMaxPhysicalAddress && LogicalAddress->LogicalAddress > maxLogicalAddress)
    {
        InterlockedIncrementNoFence64(
            reinterpret_cast<LONG64 *>(&m_statistics.DMA.PhysicalAddressTooLarge));

        return true;
    }

    return false;
}

_Use_decl_annotations_
MdlTranlationResult
NxNblTranslator::TranslateMdlChainToFragmentRangeKvmOnly(
    MDL &Mdl,
    size_t MdlOffset,
    size_t BytesToCopy
) const
{
    auto const fr = NetRingCollectionGetFragmentRing(m_rings);
    auto const frOsBegin = fr->EndIndex;
    auto const frOsEnd = (fr->BeginIndex - 1) & fr->ElementIndexMask;

    auto const availableFragments = NetRingGetRangeCount(fr, frOsBegin, frOsEnd);
    auto const maximumFragmentCount = min(availableFragments, m_maxFragmentsPerPacket);

    if (m_extensions.Extension.Mdl.Enabled && MdlOffset >=  (1 << 10))
    {
        // when the MDL extension is enabled, the NET_FRAGMENT.Offset field is used to
        // store the NET_BUFFER's currentMdlOffset. Because NET_FRAGMENT.Offset is a 
        // 10-bit only field, so we have to bounce this packet if the currentMdlOffset is
        // larger than 10-bit.
        return { NxNblTranslationStatus::BounceRequired };
    }

    //
    // While there is remaining data to be described using NET_FRAGMENT, and we have MDLs to walk...
    //
    auto mdl = &Mdl;
    UINT16 i = 0;
    for (auto remain = BytesToCopy; remain > 0; mdl = mdl->Next)
    {
        //
        // Skip zero length MDLs.
        //
        size_t const mdlByteCount = MmGetMdlByteCount(mdl);
        if (mdlByteCount == 0)
        {
            continue;
        }

        if (i == maximumFragmentCount)
        {
            //
            // We're out of fragments to complete the translation. We could
            // be in this situation for two reasons:
            //
            // 1) We don't have enough fragments at the moment but will have
            // it at some point in the future
            //
            // 2) The packet requires more fragments than m_maxFragmentsPerPacket
            //
            // This logic assumes that 1) is the common case, returing insufficient
            // resources from this funcion (try again) until we have at least
            // m_maxFragmentsPerPacket available fragments, at which point we bounce
            // the entire packet if we still can't finish the translation.
            //

            if (maximumFragmentCount == m_maxFragmentsPerPacket)
            {
                return { NxNblTranslationStatus::BounceRequired };
            }

            return { NxNblTranslationStatus::InsufficientResources };
        }

        //
        // Get the current fragment, increment the index
        //

        auto const frIndex = (frOsBegin + i) & fr->ElementIndexMask;
        i += 1;

        auto fragment = NetRingGetFragmentAtIndex(fr, frIndex);
        auto virtualAddress = NetExtensionGetFragmentVirtualAddress(
            &m_extensions.Extension.VirtualAddress, frIndex);
        auto const logicalAddress = NetExtensionGetFragmentLogicalAddress(
            &m_extensions.Extension.LogicalAddress, frIndex);

        //
        // Compute the amount to transfer this time.
        //
        size_t const copySize = min(remain, mdlByteCount - MdlOffset);

        RtlZeroMemory(fragment, sizeof(NET_FRAGMENT));

        if (m_extensions.Extension.Mdl.Enabled)
        {
            // when the MDL extension is enabled, the NET_FRAGMENT.Offset field is used to
            // store the NET_BUFFER's currentMdlOffset. We need to preserve that
            
            auto fragmentMdl = NetExtensionGetFragmentMdl(
                &m_extensions.Extension.Mdl, frIndex);

            fragment->Offset = MdlOffset;
            fragment->Capacity = mdlByteCount;
            fragment->ValidLength = copySize;
            virtualAddress->VirtualAddress = MmGetSystemAddressForMdlSafe(
                mdl, LowPagePriority | MdlMappingNoExecute);
            fragmentMdl->Mdl = mdl;
        }
        else
        {
            // otherwise roll the offset to the base Va
            fragment->Offset = 0;
            fragment->Capacity = mdlByteCount - MdlOffset;
            fragment->ValidLength = copySize;
            virtualAddress->VirtualAddress = (PCHAR) MmGetSystemAddressForMdlSafe(
                mdl, LowPagePriority | MdlMappingNoExecute) + MdlOffset;
        }

        if (ShouldBounceFragment(fragment, virtualAddress, logicalAddress))
        {
            return { NxNblTranslationStatus::BounceRequired };
        }

        MdlOffset = 0;
        remain -= copySize;
    }

    return { NxNblTranslationStatus::Success, frOsBegin, i };
}

MdlTranlationResult
NxNblTranslator::TranslateMdlChainToDmaMappedFragmentRange(
    MDL &Mdl,
    size_t MdlOffset,
    size_t BytesToCopy,
    NET_PACKET const &Packet
) const
{
    if (m_dmaAdapter->AlwaysBounce())
    {
        return { NxNblTranslationStatus::BounceRequired };
    }

    if (m_dmaAdapter->BypassHal())
    {
        return TranslateMdlChainToDmaMappedFragmentRangeBypassHal(
            Mdl,
            MdlOffset,
            BytesToCopy);
    }
    else
    {
        auto dmaTransfer = m_dmaAdapter->InitializeDmaTransfer(Packet);

        if (!dmaTransfer)
        {
            return { NxNblTranslationStatus::InsufficientResources };
        }

        return TranslateMdlChainToDmaMappedFragmentRangeUseHal(
            Mdl,
            MdlOffset,
            BytesToCopy,
            dmaTransfer);
    }
}

static
bool
CanTranslateSglToNetPacket(
    _In_ SCATTER_GATHER_LIST const & Sgl,
    _In_ MDL const & Mdl
)
{
    auto mdlCount = 0ull;
    for (auto mdl = &Mdl; mdl != nullptr; mdl = mdl->Next)
    {
        mdlCount++;
    }

    return Sgl.NumberOfElements >= mdlCount;
}

_Use_decl_annotations_
MdlTranlationResult
NxNblTranslator::TranslateScatterGatherListToFragmentRange(
    MDL & MappedMdl,
    size_t BytesToCopy,
    size_t MdlOffset,
    SCATTER_GATHER_LIST const * Sgl
) const
{
    if (Sgl->NumberOfElements > m_maxFragmentsPerPacket)
    {
        return { NxNblTranslationStatus::BounceRequired };
    }

    auto const fr = NetRingCollectionGetFragmentRing(m_rings);
    auto const frOsBegin = fr->EndIndex;
    auto const frOsEnd = (fr->BeginIndex - 1) & fr->ElementIndexMask;

    auto const availableFragments = NetRingGetRangeCount(fr, frOsBegin, frOsEnd);

    if (Sgl->NumberOfElements > availableFragments)
    {
        return { NxNblTranslationStatus::InsufficientResources };
    }

    MDL * currentMdl = &MappedMdl;
    size_t currentMdlBytesToCopy = MmGetMdlByteCount(currentMdl) - MdlOffset;
    size_t currentMdlOffset = MdlOffset;
    size_t remain = BytesToCopy;

    for (auto i = 0u; i < Sgl->NumberOfElements; i++)
    {
        auto const frIndex = (frOsBegin + i) & fr->ElementIndexMask;
        auto fragment = NetRingGetFragmentAtIndex(fr, frIndex);
        auto virtualAddress = NetExtensionGetFragmentVirtualAddress(
            &m_extensions.Extension.VirtualAddress, frIndex);
        auto logicalAddress = NetExtensionGetFragmentLogicalAddress(
            &m_extensions.Extension.LogicalAddress, frIndex);

        auto const kva = static_cast<unsigned char *>(
            MmGetSystemAddressForMdlSafe(currentMdl, LowPagePriority | MdlMappingNoExecute));

        SCATTER_GATHER_ELEMENT const &sge = Sgl->Elements[i];

        RtlZeroMemory(fragment, sizeof(NET_FRAGMENT));
        fragment->Capacity = sge.Length;
        fragment->ValidLength = sge.Length;
        fragment->Offset = 0;

        // The fragment offset has to be zero because Scatter/Gather elements do
        // not have an offset, so we need to adjust the virtual address as appropriate
        virtualAddress->VirtualAddress = kva + currentMdlOffset;
        logicalAddress->LogicalAddress = sge.Address.QuadPart;

        if (ShouldBounceFragment(fragment, virtualAddress, logicalAddress))
        {
            return { NxNblTranslationStatus::BounceRequired };
        }

        auto const fragmentLength = static_cast<size_t>(fragment->ValidLength);
        currentMdlBytesToCopy -= fragmentLength;
        remain -= fragmentLength;
        currentMdlOffset += fragmentLength;

        if (currentMdlBytesToCopy == 0 && currentMdl->Next != nullptr)
        {
            // If we're done with the currently mapped MDL let's get the next one
            currentMdl = currentMdl->Next;
            currentMdlOffset = 0;
            currentMdlBytesToCopy = MmGetMdlByteCount(currentMdl);
        }
    }

    if (remain != 0)
    {
        // Something went wrong, we need to bounce this packet

        return { NxNblTranslationStatus::BounceRequired };
    }

    return { NxNblTranslationStatus::Success, frOsBegin, static_cast<UINT16>(Sgl->NumberOfElements) };
}

_Use_decl_annotations_
bool
NxNblTranslator::MapMdlChainToSystemVA(
    MDL & Mdl
) const
{
    auto currentMdl = &Mdl;
    while (currentMdl != nullptr)
    {
        // Pages should always be locked for DMA.
        NT_ASSERT(WI_IsFlagSet(currentMdl->MdlFlags, MDL_PAGES_LOCKED));

        // We're not expecting the new MDL chain to have a kvm already
        if (!WIN_VERIFY(WI_IsFlagClear(currentMdl->MdlFlags, MDL_MAPPED_TO_SYSTEM_VA)))
        {
            break;
        }

        // We expect HAL to build the MDLs directly from the physical addresses
        if (!WIN_VERIFY(WI_IsFlagClear(currentMdl->MdlFlags, MDL_SOURCE_IS_NONPAGED_POOL)))
        {
            break;
        }

        auto kvm = MmMapLockedPagesSpecifyCache(
            currentMdl,
            KernelMode,
            MmCached,
            nullptr,
            FALSE,
            LowPagePriority | MdlMappingNoExecute);

        if (kvm == nullptr)
        {
            break;
        }

        currentMdl = currentMdl->Next;
    }

    // If we we're not able to map the whole MDL chain into system VA we need to cleanup
    // the existing mappings here
    if (currentMdl != nullptr)
    {
        for (auto mdl = &Mdl; mdl != currentMdl; mdl = mdl->Next)
        {
            NT_ASSERT(WI_IsFlagSet(mdl->MdlFlags, MDL_MAPPED_TO_SYSTEM_VA));

            MmUnmapLockedPages(
                mdl->MappedSystemVa,
                mdl);
        }

        return false;
    }

    return true;
}

_Use_decl_annotations_
MdlTranlationResult
NxNblTranslator::TranslateMdlChainToDmaMappedFragmentRangeUseHal(
    MDL & Mdl,
    size_t MdlOffset,
    size_t BytesToCopy,
    NxDmaTransfer const & DmaTransfer
) const
{
    SCATTER_GATHER_LIST * sgl;

    // Build the scatter/gather list using HAL
    NTSTATUS const buildSglStatus = m_dmaAdapter->BuildScatterGatherListEx(
        DmaTransfer,
        &Mdl,
        MdlOffset,
        BytesToCopy,
        &sgl);

    if (buildSglStatus == STATUS_INSUFFICIENT_RESOURCES)
    {
        // DMA engine might be out of map registers or some other resource,
        // let's try again later
        InterlockedIncrementNoFence64(
            reinterpret_cast<LONG64 *>(&m_statistics.DMA.InsufficientResources));

        return { NxNblTranslationStatus::InsufficientResources };
    }
    else if (buildSglStatus == STATUS_BUFFER_TOO_SMALL)
    {
        // Our pre-allocated SGL did not have enough space to hold the Scatter/Gather
        // list, bounce the packet
        InterlockedIncrementNoFence64(
            reinterpret_cast<LONG64 *>(&m_statistics.DMA.BufferTooSmall));

        return { NxNblTranslationStatus::BounceRequired };
    }
    else if (buildSglStatus != STATUS_SUCCESS)
    {
        // If we can't map this packet using HAL APIs let's try to bounce it
        InterlockedIncrementNoFence64(
            reinterpret_cast<LONG64 *>(&m_statistics.DMA.OtherErrors));

        return { NxNblTranslationStatus::BounceRequired };
    }

    auto sglGuard = wil::scope_exit([this, sgl]()
    {
        m_dmaAdapter->PutScatterGatherList(sgl);
    });

    auto const mappedMdl = m_dmaAdapter->GetMappedMdl(
        sgl,
        &Mdl);

    if (mappedMdl == nullptr)
    {
        // We need the mapped MDL chain to be able to translate the Scatter/Gather list
        // to a chain of NET_FRAGMENT, if we don't have it we need to bounce

        return { NxNblTranslationStatus::BounceRequired };
    }

    if (!CanTranslateSglToNetPacket(*sgl, *mappedMdl))
    {
        // If this is the case we can't calculate the VAs of each physical fragment.
        // This should be really rare, so instead of trying to create MDLs and map each
        // physical fragment into VA space just bounce the buffers
        InterlockedIncrementNoFence64(
            reinterpret_cast<LONG64 *>(&m_statistics.DMA.CannotMapSglToFragments));

        return { NxNblTranslationStatus::BounceRequired };
    }

    auto& dmaContext = DmaTransfer.GetTransferContext();
    dmaContext.MdlChain = mappedMdl;

    if (mappedMdl != &Mdl)
    {
        // HAL did not directly use the buffers we passed in to BuildScatterGatherList.
        // This means we have a new MDL chain that does not have virtual memory mapping.
        // The physical pages are locked.
        MdlOffset = 0;

        if (MapMdlChainToSystemVA(*mappedMdl))
        {
            dmaContext.UnmapMdlChain = true;
        }
        else
        {
            return { NxNblTranslationStatus::BounceRequired };
        }
    }

    auto const result = TranslateScatterGatherListToFragmentRange(
        *mappedMdl,
        BytesToCopy,
        MdlOffset,
        sgl);

    if (result.Status == NxNblTranslationStatus::Success)
    {
        // When the packet is returned by the client driver we will call PutScatterGatherList
        sglGuard.release();
        dmaContext.ScatterGatherList = sgl;
    }

    return result;
}

_Use_decl_annotations_
MdlTranlationResult
NxNblTranslator::TranslateMdlChainToDmaMappedFragmentRangeBypassHal(
    MDL & Mdl,
    size_t MdlOffset,
    size_t BytesToCopy
) const
{
    auto const fr = NetRingCollectionGetFragmentRing(m_rings);
    auto const frOsBegin = fr->EndIndex;
    auto const frOsEnd = (fr->BeginIndex - 1) & fr->ElementIndexMask;

    auto const availableFragments = NetRingGetRangeCount(fr, frOsBegin, frOsEnd);
    auto const maximumFragmentCount = min(availableFragments, m_maxFragmentsPerPacket);

    //
    // While there is remaining data to be copied, and we have MDLs to walk...
    //

    auto mdl = &Mdl;
    UINT16 i = 0;
    for (size_t remain = BytesToCopy; mdl && (remain > 0); mdl = mdl->Next)
    {
        // Skip zero length MDLs.
        size_t const mdlByteCount = MmGetMdlByteCount(mdl);
        if (mdlByteCount == 0)
        {
            continue;
        }

        size_t const copySize = min(remain, mdlByteCount - MdlOffset);

        ULONG_PTR vaStart = reinterpret_cast<ULONG_PTR>(
            MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute)) + MdlOffset;
        ULONG_PTR vaEnd = vaStart + static_cast<ULONG>(copySize);

        if (vaStart == vaEnd)
        {
            continue;
        }

        for (auto va = vaStart; va < vaEnd; va = reinterpret_cast<ULONG_PTR>((PAGE_ALIGN(va))) + PAGE_SIZE)
        {
            if (i == maximumFragmentCount)
            {
                //
                // We're out of fragments to complete the translation. We could
                // be in this situation for two reasons:
                //
                // 1) We don't have enough fragments at the moment but will have
                // it at some point in the future
                //
                // 2) The packet requires more fragments than m_maxFragmentsPerPacket
                //
                // This logic assumes that 1) is the common case, returing insufficient
                // resources from this funcion (try again) until we have at least
                // m_maxFragmentsPerPacket available fragments, at which point we bounce
                // the entire packet if we still can't finish the translation.
                //

                if (maximumFragmentCount == m_maxFragmentsPerPacket)
                {
                    return { NxNblTranslationStatus::BounceRequired };
                }

                return { NxNblTranslationStatus::InsufficientResources };
            }

            // Get the current fragment, increment the index
            auto const frIndex = (frOsBegin + i) & fr->ElementIndexMask;
            i += 1;

            auto const fragmentLength = PAGE_ALIGN(va) != PAGE_ALIGN(vaEnd) ?
                PAGE_SIZE - BYTE_OFFSET(va) :
                (ULONG)(vaEnd - va);

            auto fragment = NetRingGetFragmentAtIndex(fr, frIndex);
            auto virtualAddress = NetExtensionGetFragmentVirtualAddress(
                &m_extensions.Extension.VirtualAddress, frIndex);
            auto logicalAddress = NetExtensionGetFragmentLogicalAddress(
                &m_extensions.Extension.LogicalAddress, frIndex);

            // Performance can be optimized by coalescing adjacent fragments

            RtlZeroMemory(fragment, sizeof(NET_FRAGMENT));
            fragment->ValidLength = fragmentLength;
            fragment->Capacity = fragmentLength;
            fragment->Offset = 0;

            virtualAddress->VirtualAddress = reinterpret_cast<void *>(va);
            logicalAddress->LogicalAddress = MmGetPhysicalAddress(reinterpret_cast<void *>(va)).QuadPart;

            if (ShouldBounceFragment(fragment, virtualAddress, logicalAddress))
            {
                return { NxNblTranslationStatus::BounceRequired };
            }

            remain -= fragmentLength;
        }

        MdlOffset = 0;
    }

    return { NxNblTranslationStatus::Success, frOsBegin, i };
}

static
UINT32
GetPacketBytes(
    _In_ NET_RING_COLLECTION const * descriptor,
    _In_ NET_PACKET const * packet
)
{
    NT_ASSERT(packet->FragmentCount != 0);

    auto const fr = NetRingCollectionGetFragmentRing(descriptor);
    auto const endIndex = NetRingAdvanceIndex(fr, packet->FragmentIndex, packet->FragmentCount);

    UINT64 bytes = 0u;
    for (auto index = packet->FragmentIndex; index != endIndex; index = NetRingIncrementIndex(fr, index))
    {
        bytes += NetRingGetFragmentAtIndex(fr, index)->ValidLength;
    }

    return static_cast<UINT32>(bytes);
}

bool
NxNblTranslator::TranslateLayout(
    void
)
{
    int layerDone = 0;
    size_t offset = 0;
    auto mdl = m_currentNetBuffer->CurrentMdl;
    size_t remainingLength = m_currentNetBuffer->DataLength;
    auto pointer = 
        static_cast<UINT8 const *>(MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute))
        + m_currentNetBuffer->CurrentMdlOffset;
    size_t length = 
        min(MmGetMdlByteCount(mdl) - m_currentNetBuffer->CurrentMdlOffset, 
            m_currentNetBuffer->DataLength);

    m_layout = {};
    m_layout.Layer2Type = m_layer2Type;

    // parse L2, L3 and L4 headers, and each layer's header must be contiguous
    for (auto ParseLayerFunc : ParseLayerDispatch)
    {
        if (ParseLayerFunc(m_layout, pointer, offset, length))
        {
            layerDone++;
            
            if (layerDone == 3)
            {
                // all 3 layers have been parsed, we are done
                break;
            }
            
            // if the length remained is larger than zero, we will continue to parse next 
            // layer header in the current MDL; if the length is 0, it means we reach the 
            // end of current MDL, and we need to move to next MDL before contiuing to parse
            if (length == 0)
            {
                if (mdl->Next == nullptr)
                {
                    // encountered a layer header that we cannot parse properly
                    return false; 
                }

                // end of current MDL, continue to parse the next MDL
                mdl = mdl->Next;
                pointer = static_cast<UINT8 const *>(
                    MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute));
                remainingLength -= offset;
                length = min(MmGetMdlByteCount(mdl), remainingLength);
                offset = 0;
            }
        }
        else
        {
            return false; // encountered a layer header that we cannot parse properly
        }
    }

    return true;
}

_Use_decl_annotations_
void
NxNblTranslator::ReplaceCurrentNetBufferList(
    NET_BUFFER_LIST * NetBufferList
)
{
    NetBufferList->Next = m_currentNetBufferList->Next;
    m_currentNetBufferList = NetBufferList;
    m_currentNetBuffer = NetBufferList->FirstNetBuffer;
}

_Use_decl_annotations_
bool
NxNblTranslator::IsSoftwareGsoRequired(
    void
) const
{
    // Check if this packet needs GSO
    if (m_layout.Layer4Type != NetPacketLayer4TypeTcp && m_layout.Layer4Type != NetPacketLayer4TypeUdp)
    {
        return false;
    }

    if (m_layout.Layer4Type == NetPacketLayer4TypeTcp && m_gso.TCP.Mss == 0U)
    {
        return false;
    }

    if (m_layout.Layer4Type == NetPacketLayer4TypeUdp && m_gso.UDP.Mss == 0U)
    {
        return false;
    }

    if (m_gsoHardwareCapabilities.Layer3Flags == 0x0 &&
        m_gsoHardwareCapabilities.Layer4Flags == 0x0)
    {
        return true;
    }

    // Software fallback is required if
    // 1) Hardware has any layer 3 limitation
    switch (m_layout.Layer3Type)
    {

    case NetPacketLayer3TypeIPv4NoOptions:
        if (WI_IsFlagClear(m_gsoHardwareCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
        {
            return true;
        }
        break;

    case NetPacketLayer3TypeIPv4WithOptions:
        if (WI_IsFlagClear(m_gsoHardwareCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions))
        {
            return true;
        }
        break;

    case NetPacketLayer3TypeIPv6NoExtensions:
        if (WI_IsFlagClear(m_gsoHardwareCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
        {
            return true;
        }
        break;

    case NetPacketLayer3TypeIPv6WithExtensions:
        if (WI_IsFlagClear(m_gsoHardwareCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions))
        {
            return true;
        }
        break;

    case NetPacketLayer3TypeUnspecified:
    case NetPacketLayer3TypeIPv4UnspecifiedOptions:
    case NetPacketLayer3TypeIPv6UnspecifiedExtensions:
        return false;
    }

    // 2) Hardware has any layer 4 limitation
    switch (m_layout.Layer4Type)
    {

    case NetPacketLayer4TypeTcp:
        {
            auto const tcpHeaderContainsOptions = TcpHeaderContainsOptions(m_layout);
            if (tcpHeaderContainsOptions &&
                WI_IsFlagClear(m_gsoHardwareCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions))
            {
                return true;
            }

            if (! tcpHeaderContainsOptions &&
                WI_IsFlagClear(m_gsoHardwareCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions))
            {
                return true;
            }
        }
        break;

    case NetPacketLayer4TypeUdp:
        if (WI_IsFlagClear(m_gsoHardwareCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
        {
            return true;
        }
        break;

    }

    // 3) Hardware has layer 4 header offset limitation
    auto const layer4HeaderOffset = m_layout.Layer2HeaderLength + m_layout.Layer3HeaderLength;

    if (m_gsoHardwareCapabilities.Layer4HeaderOffsetLimit != 0U &&
        m_gsoHardwareCapabilities.Layer4HeaderOffsetLimit < layer4HeaderOffset)
    {
        return true;
    }

    return false;
}

_Use_decl_annotations_
bool
NxNblTranslator::GenericSendOffloadFallback(
    void
)
{
    NT_FRE_ASSERT(m_layout.Layer4Type == NetPacketLayer4TypeTcp ||
                  m_layout.Layer4Type == NetPacketLayer4TypeUdp);

    NET_BUFFER_LIST * segmentedNbl = nullptr;

    if (m_layout.Layer4Type == NetPacketLayer4TypeTcp)
    {
        if (m_gsoContext.PerformLso(m_currentNetBufferList, m_layout, segmentedNbl) == STATUS_SUCCESS)
        {
            InterlockedIncrementNoFence64(
                reinterpret_cast<LONG64 *>(&m_statistics.Segment.TcpSuccess));

            // Replace current NBL and NB
            ReplaceCurrentNetBufferList(segmentedNbl);

            // Clear the metadata translated from m_currentNetBufferList.
            // Since the LSO/USO OOB info in segmentedNbl is also cleared in PerformLso/PerformUso(),
            // further metadata translating on segmentedNbl will produce m_gso.TCP/UDP.Mss = 0.
            m_gso.TCP.Mss = 0U;

            return true;
        }
        else
        {
            InterlockedIncrementNoFence64(
                reinterpret_cast<LONG64 *>(&m_statistics.Segment.TcpFailure));

            PKTMON_DROP_NBL(
                const_cast<PKTMON_COMPONENT_CONTEXT *>(m_pktmonComponentContext),
                m_currentNetBufferList,
                PktMonDir_Out,
                PktMonDrop_NetCx_LSOFailure,
                PMLOC_NETCX_NBL_LSO_FAILURE);

            return false;
        }
    }

    if (m_layout.Layer4Type == NetPacketLayer4TypeUdp)
    {
        if (m_gsoContext.PerformUso(m_currentNetBufferList, m_layout, segmentedNbl) == STATUS_SUCCESS)
        {
            InterlockedIncrementNoFence64(
                reinterpret_cast<LONG64 *>(&m_statistics.Segment.UdpSuccess));
            ReplaceCurrentNetBufferList(segmentedNbl);
            m_gso.UDP.Mss = 0U;

            return true;
        }
        else
        {
            InterlockedIncrementNoFence64(
                reinterpret_cast<LONG64 *>(&m_statistics.Segment.UdpFailure));

            PKTMON_DROP_NBL(
                const_cast<PKTMON_COMPONENT_CONTEXT *>(m_pktmonComponentContext),
                m_currentNetBufferList,
                PktMonDir_Out,
                PktMonDrop_NetCx_USOFailure,
                PMLOC_NETCX_NBL_USO_FAILURE);

            return false;
        }
    }

    return true;
}

_Use_decl_annotations_
bool
NxNblTranslator::TranslateNetBuffer(
    UINT32 Index
)
{
    auto & packet = *NetRingGetPacketAtIndex(NetRingCollectionGetPacketRing(m_rings), Index);

    (void) TranslateLayout();

    if (IsSoftwareGsoRequired())
    {
        if (! GenericSendOffloadFallback())
        {
            m_counters.Errors += 1;

            packet.Ignore = true;
            packet.FragmentCount = 0U;

            return true;
        }
        else
        {
            m_checksum = m_metadataTranslator.TranslatePacketChecksum(*m_currentNetBufferList);
        }
    }

    switch (TranslateNetBufferToNetPacket(&packet))
    {

    case NxNblTranslationStatus::BounceRequired:
        // The buffers in the NET_BUFFER's MDL chain cannot be transmitted as is. As such we need
        // to bounce the packet

        if (! m_bouncePool.BounceNetBuffer(*m_currentNetBuffer, packet))
        {
            if (packet.Ignore)
            {
                // If it was not possible to bounce the NET_BUFFER and the
                // current packet was marked to be ignored we should *not*
                // try to translate it again.
                InterlockedIncrementNoFence64(
                    reinterpret_cast<LONG64 *>(&m_statistics.Packet.CannotTranslate));
                m_counters.Errors += 1;

                PKTMON_DROP_NBL(
                    const_cast<PKTMON_COMPONENT_CONTEXT *>(m_pktmonComponentContext),
                    m_currentNetBufferList,
                    PktMonDir_Out,
                    PktMonDrop_NetCx_BufferBounceFailureAndPacketIgnore,
                    PMLOC_NETCX_BUFFER_BOUNCE_FAILURE)

                break;
            }
            else
            {
                // It was not possible to bounce the NET_BUFFER because we're
                // out of some resource. Try again later.
                InterlockedIncrementNoFence64(
                    reinterpret_cast<LONG64 *>(&m_statistics.Packet.BounceFailure));

                return false;
            }
        }

        InterlockedIncrementNoFence64(
            reinterpret_cast<LONG64 *>(&m_statistics.Packet.BounceSuccess));

        __fallthrough;

    case NxNblTranslationStatus::Success:
        packet.Layout = m_layout;

        m_metadataTranslator.TranslatePacketWifiExemptionAction(*m_currentNetBufferList, Index);
        if (m_extensions.Extension.Gso.Enabled)
        {
            *NetExtensionGetPacketGso(&m_extensions.Extension.Gso, Index) = m_gso;
        }
        if (m_extensions.Extension.Checksum.Enabled)
        {
            if (IsSoftwareChecksumRequired())
            {
                NET_PACKET_CHECKSUM checksum = m_checksum;
                checksum.Layer3 = NetPacketTxChecksumActionPassthrough;
                checksum.Layer4 = NetPacketTxChecksumActionPassthrough;

                *NetExtensionGetPacketChecksum(&m_extensions.Extension.Checksum, Index) = checksum;
            }
            else
            {
                *NetExtensionGetPacketChecksum(&m_extensions.Extension.Checksum, Index) = m_checksum;
            }
        }
        if (m_extensions.Extension.Ieee8021q.Enabled)
        {
            *NetExtensionGetPacketIeee8021Q(&m_extensions.Extension.Ieee8021q, Index) = m_ieee8021q;
        }

        break;

    case NxNblTranslationStatus::InsufficientResources:
        // There are not enough resources at the moment to translate the NET_BUFFER,
        // stop processing here. Once there are enough resources available this will
        // make forward progress

        return false;

    case NxNblTranslationStatus::CannotTranslate:
        // For some reason we won't ever be able to translate the NET_BUFFER,
        // mark the corresponding NET_PACKET to be dropped.

        packet.Ignore = true;
        packet.FragmentCount = 0;

        InterlockedIncrementNoFence64(
            reinterpret_cast<LONG64 *>(&m_statistics.Packet.CannotTranslate));
        m_counters.Errors += 1;

        break;
    }

    return true;
}

ULONG
NxNblTranslator::TranslateNbls(
    void
)
{
    auto pr = NetRingCollectionGetPacketRing(m_rings);
    auto const endIndex = pr->EndIndex;

    TranslateNetBufferListInfo();

    while (m_currentNetBufferList && pr->EndIndex != NetRingAdvanceIndex(pr, pr->OSReserved0, -1))
    {
        auto & context = m_contextBuffer.GetContext<PacketContext>(pr->EndIndex);

        if (! m_currentNetBuffer)
        {
            m_currentNetBuffer = m_currentNetBufferList->FirstNetBuffer;
        }

        context.DestinationType = NxGetPacketAddressType(m_layer2Type, *m_currentNetBuffer);

        if (! TranslateNetBuffer(pr->EndIndex))
        {
            break;
        }

        m_currentNetBuffer = m_currentNetBuffer->Next;

        if (! m_currentNetBuffer)
        {
            // This is the final NB in the NBL, so let's bundle the NBL
            // up with the NET_PACKET.  We'll find it later when completing packets.

            PKTMON_LOG_NBL(
                const_cast<PKTMON_EDGE_CONTEXT *>(m_pktmonLowerEdgeContext),
                m_currentNetBufferList,
                PktMonPayload_Ethernet,
                true,
                PktMonDir_Out);

            context.NetBufferListToComplete = m_currentNetBufferList;

            // Now let's advance to the next NBL.
            m_currentNetBufferList = m_currentNetBufferList->Next;
            if (m_currentNetBufferList)
            {
                TranslateNetBufferListInfo();
            }
        }

        pr->EndIndex = NetRingIncrementIndex(pr, pr->EndIndex);
    }

    return NetRingGetRangeCount(pr, endIndex, pr->EndIndex);
}

void
NxNblTranslator::TranslateNetBufferListInfo(
    void
)
{
    m_gso = m_metadataTranslator.TranslatePacketGso(*m_currentNetBufferList);
    m_checksum = m_metadataTranslator.TranslatePacketChecksum(*m_currentNetBufferList);
    m_ieee8021q = m_metadataTranslator.TranslatePacketIeee8021q(*m_currentNetBufferList);
}

TxPacketCompletionStatus
NxNblTranslator::CompletePackets(
    void
) const
{
    TxPacketCompletionStatus result;

    auto pr = NetRingCollectionGetPacketRing(m_rings);
    auto const osreserved0 = pr->OSReserved0;

    for (; ((pr->OSReserved0 != pr->BeginIndex) && (result.NumCompletedNbls < NBL_SEND_COMPLETION_BATCH_SIZE));
        pr->OSReserved0 = NetRingIncrementIndex(pr, pr->OSReserved0))
    {
        auto packet = NetRingGetPacketAtIndex(pr, pr->OSReserved0);
        auto & extension = m_contextBuffer.GetContext<PacketContext>(pr->OSReserved0);

        if (m_dmaAdapter)
        {
            m_dmaAdapter->CleanupNetPacket(*packet);
        }

        // Free any bounce buffers allocated for this packet
        m_bouncePool.FreeBounceBuffers(*packet);

        if (auto completedNbl = extension.NetBufferListToComplete)
        {
            extension.NetBufferListToComplete = nullptr;

            if (IsNetBufferListSoftwareSegmented(completedNbl))
            {
                auto originNbl = completedNbl->ParentNetBufferList;
                m_gsoContext.FreeSoftwareSegmentedNetBufferList(completedNbl);
                completedNbl = originNbl;
            }

            completedNbl->Status = NDIS_STATUS_SUCCESS;
            completedNbl->Next = result.CompletedChain;
            result.CompletedChain = completedNbl;

            m_metadataTranslator.TranslatePacketGsoComplete(*completedNbl, pr->OSReserved0);

            result.NumCompletedNbls += 1;
        }

        if (! packet->Ignore)
        {
            switch (extension.DestinationType)
            {

            case AddressType::Unicast:
                m_counters.Unicast.Packets += 1;
                m_counters.Unicast.Bytes += GetPacketBytes(m_rings, packet);

                break;

            case AddressType::Multicast:
                m_counters.Multicast.Packets += 1;
                m_counters.Multicast.Bytes += GetPacketBytes(m_rings, packet);

                break;

            case AddressType::Broadcast:

                m_counters.Broadcast.Packets += 1;
                m_counters.Broadcast.Bytes += GetPacketBytes(m_rings, packet);

                break;

            }
        }

        RtlZeroMemory(packet, pr->ElementStride);
    }

    result.CompletedPackets = NetRingGetRangeCount(pr, osreserved0, pr->OSReserved0);

    return result;
}

TxPacketCounters const &
NxNblTranslator::GetCounters(
    void
) const
{
    return m_counters;
}
