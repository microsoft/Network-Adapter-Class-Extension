// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxNblTranslation.tmh"
#include "NxNblTranslation.hpp"

#include <net/fragment.h>
#include <net/checksum_p.h>
#include <net/logicaladdress_p.h>
#include <net/lso_p.h>
#include <net/mdl_p.h>
#include <net/rsc_p.h>
#include <net/virtualaddress_p.h>

#include "NxPacketLayout.hpp"
#include "NxStatistics.hpp"

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
    TxExtensions const & Extensions,
    NxNblTranslationStats & Stats,
    NET_RING_COLLECTION * Rings,
    const NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES & DatapathCapabilities,
    const NxDmaAdapter * DmaAdapter,
    const NxRingContext & ContextBuffer,
    NDIS_MEDIUM MediaType,
    NxStatistics & GenStats
) :
    m_extensions(Extensions),
    m_stats(Stats),
    m_rings(Rings),
    m_metadataTranslator(*Rings, Extensions),
    m_datapathCapabilities(DatapathCapabilities),
    m_contextBuffer(ContextBuffer),
    m_mediaType(MediaType),
    m_dmaAdapter(DmaAdapter),
    m_genStats(GenStats)
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
    return m_datapathCapabilities.TxMemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED;
}

_Use_decl_annotations_
NxNblTranslationStatus
NxNblTranslator::TranslateNetBufferToNetPacket(
    NET_BUFFER & netBuffer,
    NET_PACKET * netPacket
) const
{
    auto backfill = static_cast<ULONG>(m_datapathCapabilities.TxPayloadBackfill);

    if (backfill > 0)
    {
        // Check if there is enough space in the NET_BUFFER's current MDL to satisfy
        // the client driver's required backfill
        if (backfill > NET_BUFFER_CURRENT_MDL_OFFSET(&netBuffer))
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
                &netBuffer,
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
    auto advanceDataStart = wil::scope_exit([&netBuffer, backfill]()
    {
        // Avoid calling into NDIS if backfill is zero
        if (backfill > 0)
        {
            NdisAdvanceNetBufferDataStart(
                &netBuffer,
                backfill,
                TRUE,
                nullptr);
        }
    });

    PMDL mdl = NET_BUFFER_CURRENT_MDL(&netBuffer);
    size_t mdlOffset = NET_BUFFER_CURRENT_MDL_OFFSET(&netBuffer);
    auto bytesToCopy = NET_BUFFER_DATA_LENGTH(&netBuffer);

    if (bytesToCopy == 0 || bytesToCopy > m_datapathCapabilities.MaximumTxFragmentSize)
    {
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
        m_stats.Packet.UnalignedBuffer += 1;
        return true;
    }

    auto const maxLogicalAddress = static_cast<UINT64>(maxPhysicalAddress.QuadPart);
    auto const checkMaxPhysicalAddress = RequiresDmaMapping() && maxLogicalAddress != 0;
    if (checkMaxPhysicalAddress && LogicalAddress->LogicalAddress > maxLogicalAddress)
    {
        m_stats.DMA.PhysicalAddressTooLarge += 1;
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

    //
    // While there is remaining data to be copied, and we have MDLs to walk...
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
        auto fragmentMdl = NetExtensionGetFragmentMdl(
            &m_extensions.Extension.Mdl, frIndex);

        //
        // Compute the amount to transfer this time.
        //
        size_t const copySize = min(remain, mdlByteCount - MdlOffset);

        RtlZeroMemory(fragment, sizeof(NET_FRAGMENT));
        fragment->Offset = MdlOffset;
        fragment->Capacity = mdlByteCount;
        fragment->ValidLength = copySize;
        virtualAddress->VirtualAddress = MmGetSystemAddressForMdlSafe(
            mdl, LowPagePriority | MdlMappingNoExecute);
        fragmentMdl->Mdl = mdl;

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
        // DMA engine might be out of map registers or some other resource, let's try again later
        m_stats.DMA.InsufficientResouces += 1;
        return { NxNblTranslationStatus::InsufficientResources };
    }
    else if (buildSglStatus == STATUS_BUFFER_TOO_SMALL)
    {
        // Our pre-allocated SGL did not have enough space to hold the Scatter/Gather list, bounce the packet
        m_stats.DMA.BufferTooSmall += 1;
        return { NxNblTranslationStatus::BounceRequired };
    }
    else if (buildSglStatus != STATUS_SUCCESS)
    {
        // If we can't map this packet using HAL APIs let's try to bounce it
        m_stats.DMA.OtherErrors += 1;
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
        // If this is the case we can't calculate the VAs of each physical fragment. This should be really rare,
        // so instead of trying to create MDLs and map each physical fragment into VA space just bounce the buffers
        m_stats.DMA.CannotMapSglToFragments += 1;
        return { NxNblTranslationStatus::BounceRequired };
    }

    auto& dmaContext = DmaTransfer.GetTransferContext();
    dmaContext.MdlChain = mappedMdl;

    if (mappedMdl != &Mdl)
    {
        // HAL did not directly use the buffers we passed in to BuildScatterGatherList. This means we have a new
        // MDL chain that does not have virtual memory mapping. The physical pages are locked.
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

        ULONG_PTR vaStart = reinterpret_cast<ULONG_PTR>(MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute)) + MdlOffset;
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

UINT32
GetPacketBytes(
    _In_ NET_RING_COLLECTION const * descriptor,
    _In_ NET_PACKET const * packet
)
{
    NT_ASSERT(packet->FragmentCount != 0);
    
    UINT32 bytes = 0;
    for (UINT32 i = 0; i < packet->FragmentCount; ++i)
    {
        auto fr = NetRingCollectionGetFragmentRing(descriptor);
        auto const fragment = NetRingGetFragmentAtIndex(fr, i);
        bytes += (UINT32) fragment->ValidLength;
    }

    return bytes;
}

_Use_decl_annotations_
ULONG
NxNblTranslator::TranslateNbls(
    NET_BUFFER_LIST * & currentNbl,
    NET_BUFFER * & currentNetBuffer,
    NxBounceBufferPool & BouncePool
) const
{
    auto pr = NetRingCollectionGetPacketRing(m_rings);
    auto const endIndex = pr->EndIndex;

    while (currentNbl && pr->EndIndex != ((pr->OSReserved0 - 1) & pr->ElementIndexMask))
    {
        if (! currentNetBuffer)
        {
            currentNetBuffer = currentNbl->FirstNetBuffer;
        }

        auto currentPacket = NetRingGetPacketAtIndex(pr, pr->EndIndex);

        switch (TranslateNetBufferToNetPacket(*currentNetBuffer, currentPacket))
        {
        case NxNblTranslationStatus::BounceRequired:
            // The buffers in the NET_BUFFER's MDL chain cannot be transmitted as is. As such we need
            // to bounce the packet

            if(!BouncePool.BounceNetBuffer(*currentNetBuffer, *currentPacket))
            {
                if (currentPacket->Ignore)
                {
                    // If it was not possible to bounce the NET_BUFFER and the
                    // current packet was marked to be ignored we should *not*
                    // try to translate it again.
                    m_stats.Packet.CannotTranslate += 1;
                    m_genStats.Increment(NxStatisticsCounters::NumberOfErrors);
                    break;
                }
                else
                {
                    // It was not possible to bounce the NET_BUFFER because we're
                    // out of some resource. Try again later.
                    m_stats.Packet.BounceFailure += 1;
                    return endIndex != pr->EndIndex;
                }
            }

            m_stats.Packet.BounceSuccess += 1;
            __fallthrough;

        case NxNblTranslationStatus::Success:
            currentPacket->Layout = NxGetPacketLayout(m_mediaType, m_rings, m_extensions.Extension.VirtualAddress, currentPacket, m_datapathCapabilities.TxPayloadBackfill);

            m_metadataTranslator.TranslatePacketChecksum(*currentNbl, pr->EndIndex);
            m_metadataTranslator.TranslatePacketLso(*currentNbl, pr->EndIndex);
            m_metadataTranslator.TranslatePacketWifiExemptionAction(*currentNbl, pr->EndIndex);

            m_genStats.Increment(NxStatisticsCounters::NumberOfPackets);
            m_genStats.IncrementBy(NxStatisticsCounters::BytesOfData, (ULONG64)GetPacketBytes(m_rings, currentPacket));
            break;

        case NxNblTranslationStatus::InsufficientResources:
            // There are not enough resources at the moment to translate the NET_BUFFER,
            // stop processing here. Once there are enough resources available this will
            // make forward progress
            return endIndex != pr->EndIndex;

        case NxNblTranslationStatus::CannotTranslate:
            // For some reason we won't ever be able to translate the NET_BUFFER,
            // mark the corresponding NET_PACKET to be dropped.
            currentPacket->Ignore = true;
            currentPacket->FragmentCount = 0;
            m_stats.Packet.CannotTranslate += 1;
            m_genStats.Increment(NxStatisticsCounters::NumberOfErrors);
            break;
        }

        currentNetBuffer = currentNetBuffer->Next;

        if (! currentNetBuffer)
        {
            // This is the final NB in the NBL, so let's bundle the NBL
            // up with the NET_PACKET.  We'll find it later when completing packets.

            auto &currentPacketExtension = m_contextBuffer.GetContext<PacketContext>(pr->EndIndex);
            currentPacketExtension.NetBufferListToComplete = currentNbl;

            // Now let's advance to the next NBL.
            currentNbl = currentNbl->Next;
        }

        pr->EndIndex = NetRingIncrementIndex(pr, pr->EndIndex);
    }

    return NetRingGetRangeCount(pr, endIndex, pr->EndIndex);
}

_Use_decl_annotations_
TxPacketCompletionStatus
NxNblTranslator::CompletePackets(
    NxBounceBufferPool & BouncePool
) const
{
    TxPacketCompletionStatus result;

    auto pr = NetRingCollectionGetPacketRing(m_rings);
    auto const osreserved0 = pr->OSReserved0;

    for (; pr->OSReserved0 != pr->BeginIndex;
        pr->OSReserved0 = NetRingIncrementIndex(pr, pr->OSReserved0))
    {
        auto packet = NetRingGetPacketAtIndex(pr, pr->OSReserved0);
        auto & extension = m_contextBuffer.GetContext<PacketContext>(pr->OSReserved0);

        if (m_dmaAdapter)
        {
            m_dmaAdapter->CleanupNetPacket(*packet);
        }

        // Free any bounce buffers allocated for this packet
        BouncePool.FreeBounceBuffers(*packet);

        if (auto completedNbl = extension.NetBufferListToComplete)
        {
            extension.NetBufferListToComplete = nullptr;

            completedNbl->Status = NDIS_STATUS_SUCCESS;
            completedNbl->Next = result.CompletedChain;
            result.CompletedChain = completedNbl;

            m_metadataTranslator.TranslatePacketLsoComplete(*completedNbl, pr->OSReserved0);

            result.NumCompletedNbls += 1;
        }

        RtlZeroMemory(packet, pr->ElementStride);
    }

    result.CompletedPackets = NetRingGetRangeCount(pr, osreserved0, pr->OSReserved0);

    return result;
}

