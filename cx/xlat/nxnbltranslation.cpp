// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxNblTranslation.tmh"

#include "NxNblTranslation.hpp"

#include "NxPacketLayout.hpp"
#include "NxChecksumInfo.hpp"
#include "NxLargeSend.hpp"

NxNblTranslator::NxNblTranslator(
    NxNblTranslationStats &Stats,
    const NET_DATAPATH_DESCRIPTOR & DatapathDescriptor,
    const NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES & DatapathCapabilities,
    const NxDmaAdapter *DmaAdapter,
    const NxContextBuffer & ContextBuffer,
    NDIS_MEDIUM MediaType
    ) :
    m_stats(Stats),
    m_datapathDescriptor(DatapathDescriptor),
    m_datapathCapabilities(DatapathCapabilities),
    m_contextBuffer(ContextBuffer),
    m_mediaType(MediaType),
    m_dmaAdapter(DmaAdapter)
{

}

bool
NxNblTranslator::RequiresDmaMapping(
    void
    ) const
{
    return m_datapathCapabilities.TxMemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED;
}

NetRbFragmentRange
NxNblTranslator::EmptyFragmentRange(
    void
    ) const
{
    return { *NET_DATAPATH_DESCRIPTOR_GET_FRAGMENT_RING_BUFFER(&m_datapathDescriptor), 0, 0 };
}

size_t
NxNblTranslator::MaximumNumberOfFragments(
    void
    ) const
{
    return NET_DATAPATH_DESCRIPTOR_GET_FRAGMENT_RING_BUFFER(&m_datapathDescriptor)->NumberOfElements - 1;
}

_Use_decl_annotations_
void
NxNblTranslator::TranslateNetBufferListOOBDataToNetPacketExtensions(
    NET_BUFFER_LIST const &netBufferList,
    NET_PACKET* netPacket
    ) const
{
    // For every in-use packet extensions for a NET_PACKET
    // translator (NET_PACKET owner) zeroes existing data and fill in new data

    // Checksum
    if (IsPacketChecksumEnabled())
    {
        NET_PACKET_CHECKSUM* checksumExt =
            NetPacketGetPacketChecksum(netPacket, m_netPacketChecksumOffset);
        RtlZeroMemory(checksumExt, NET_PACKET_EXTENSION_CHECKSUM_VERSION_1_SIZE);

        auto const &checksumInfo =
            *(NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO*)
            &netBufferList.NetBufferListInfo[TcpIpChecksumNetBufferListInfo];

#if DBG
        if (checksumInfo.Transmit.TcpChecksum && netPacket->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP)
        {
            NT_ASSERT(checksumInfo.Transmit.TcpHeaderOffset == (ULONG)(netPacket->Layout.Layer2HeaderLength + netPacket->Layout.Layer3HeaderLength));
        }
#endif

        *checksumExt = NxTranslateTxPacketChecksum(*netPacket, checksumInfo);
    }

    if (IsPacketLargeSendSegmentationEnabled())
    {
        NET_PACKET_LARGE_SEND_SEGMENTATION* lsoExt =
            NetPacketGetPacketLargeSendSegmentation(netPacket, m_netPacketLsoOffset);
        RtlZeroMemory(lsoExt, NET_PACKET_EXTENSION_LSO_VERSION_1_SIZE);

        if (netPacket->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP)
        {
            auto const &lsoInfo =
                *(NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO*)
                &netBufferList.NetBufferListInfo[TcpLargeSendNetBufferListInfo];

            *lsoExt = NxTranslateTxPacketLargeSendSegmentation(*netPacket, lsoInfo);
        }
    }
}

_Use_decl_annotations_
NxNblTranslationStatus
NxNblTranslator::TranslateNetBufferToNetPacket(
    NET_BUFFER & netBuffer,
    NET_PACKET* netPacket
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

    auto& fragmentRing = *NET_DATAPATH_DESCRIPTOR_GET_FRAGMENT_RING_BUFFER(&m_datapathDescriptor);
    auto const availableFragments = NetRbFragmentRange::OsRange(fragmentRing);

    auto const result = RequiresDmaMapping() ?
        TranslateMdlChainToDmaMappedFragmentRange(*mdl, mdlOffset, bytesToCopy, *netPacket, availableFragments) :
        TranslateMdlChainToFragmentRangeKvmOnly(*mdl, mdlOffset, bytesToCopy, availableFragments);

    if (result.Status == NxNblTranslationStatus::Success)
    {
        if (result.FragmentChain.Count() > 0)
        {
            // Commit the fragment chain to the packet
            netPacket->FragmentCount = static_cast<UINT16>(result.FragmentChain.Count());
            netPacket->FragmentOffset = result.FragmentChain.begin().GetIndex();
            fragmentRing.EndIndex = result.FragmentChain.end().GetIndex();

            if (backfill > 0)
            {
                // If the client driver requested any amount of backfill we need to take
                // that space out of the first fragment's valid length
                auto & firstFragment = *result.FragmentChain.begin();

                NT_ASSERT(firstFragment.Offset + backfill <= firstFragment.Capacity);

                firstFragment.Offset += backfill;
                firstFragment.ValidLength -= backfill;
            }
        }
    }

    return result.Status;
}

inline
bool
IsAddressAligned(
    _In_ void *Address,
    _In_ size_t Alignment
    )
{
    return (reinterpret_cast<ULONG_PTR>(Address) & (Alignment - 1)) == 0;
}

_Use_decl_annotations_
bool
NxNblTranslator::ShouldBounceFragment(
    NET_PACKET_FRAGMENT const &Fragment
    ) const
{
    auto const alignment = m_datapathCapabilities.TxMemoryConstraints.AlignmentRequirement;
    auto const maxPhysicalAddress = m_datapathCapabilities.TxMemoryConstraints.Dma.MaximumPhysicalAddress;

    if (!IsAddressAligned(Fragment.VirtualAddress, alignment))
    {
        m_stats.Packet.UnalignedBuffer += 1;
        return true;
    }

    bool checkMaxPhysicalAddress = RequiresDmaMapping() && maxPhysicalAddress.QuadPart != 0;
    if (checkMaxPhysicalAddress && Fragment.Mapping.DmaLogicalAddress.QuadPart > maxPhysicalAddress.QuadPart)
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
    size_t BytesToCopy,
    NetRbFragmentRange const &AvailableFragments
    ) const
{
    auto it = AvailableFragments.begin();

    //
    // While there is remaining data to be copied, and we have MDLs to walk...
    //
    auto mdl = &Mdl;
    for (auto remain = BytesToCopy; remain > 0; mdl = mdl->Next)
    {
        auto numberOfFragments = NetRbFragmentRange(AvailableFragments.begin(), it).Count();

        if (numberOfFragments > m_datapathCapabilities.MaximumNumberOfTxFragments)
        {
            return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
        }

        if (it == AvailableFragments.end())
        {
            // We need to know how many fragments this packet has
            // because we might never have enough fragments to
            // translate it
            while (mdl)
            {
                numberOfFragments++;
                mdl = mdl->Next;
            }

            if (numberOfFragments > MaximumNumberOfFragments())
            {
                return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
            }

            return { NxNblTranslationStatus::InsufficientResources, EmptyFragmentRange() };
        }

        //
        // Skip zero length MDLs.
        //

        size_t const mdlByteCount = MmGetMdlByteCount(mdl);
        if (mdlByteCount == 0)
        {
            continue;
        }

        //
        // Get the current fragment, increment the iterator
        //

        auto& currentFragment = *(it++);
        RtlZeroMemory(&currentFragment, NetPacketFragmentGetSize());

        //
        // Compute the amount to transfer this time.
        //

        ASSERT(MdlOffset < mdlByteCount);
        size_t const copySize = min(remain, mdlByteCount - MdlOffset);

        //
        // Perform the transfer
        //
        currentFragment.Offset = MdlOffset;

        // NDIS6 requires MDLs to be mapped to system address space, so
        // retrieving SystemAddress should not fail.
        currentFragment.VirtualAddress = MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute);
        NT_ASSERT(currentFragment.VirtualAddress);

        if (ShouldBounceFragment(currentFragment))
        {
            return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
        }

        currentFragment.Mapping.Mdl = mdl;

        currentFragment.Capacity = mdlByteCount;
        currentFragment.ValidLength = copySize;

        MdlOffset = 0;
        remain -= copySize;
    }

    return { NxNblTranslationStatus::Success, NetRbFragmentRange(AvailableFragments.begin(), it) };
}

MdlTranlationResult
NxNblTranslator::TranslateMdlChainToDmaMappedFragmentRange(
    MDL &Mdl,
    size_t MdlOffset,
    size_t BytesToCopy,
    NET_PACKET const &Packet,
    NetRbFragmentRange const &AvailableFragments
    ) const
{
    if (m_dmaAdapter->AlwaysBounce())
    {
        return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
    }

    if (m_dmaAdapter->BypassHal())
    {
        return TranslateMdlChainToDmaMappedFragmentRangeBypassHal(
            Mdl,
            MdlOffset,
            BytesToCopy,
            AvailableFragments);
    }
    else
    {
        auto dmaTransfer = m_dmaAdapter->InitializeDmaTransfer(Packet);

        if (!dmaTransfer)
        {
            return { NxNblTranslationStatus::InsufficientResources, EmptyFragmentRange() };
        }

        return TranslateMdlChainToDmaMappedFragmentRangeUseHal(
            Mdl,
            MdlOffset,
            BytesToCopy,
            dmaTransfer,
            AvailableFragments);
    }
}

static
bool
CanTranslateSglToNetPacket(
    _In_ SCATTER_GATHER_LIST const &Sgl,
    _In_ MDL const &Mdl
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
    MDL &MappedMdl,
    size_t BytesToCopy,
    size_t MdlOffset,
    NxScatterGatherList const &Sgl,
    NetRbFragmentRange const &AvailableFragments
    ) const
{
    if (Sgl->NumberOfElements > m_datapathCapabilities.MaximumNumberOfTxFragments)
    {
        return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
    }

    auto it = AvailableFragments.begin();

    MDL *currentMdl = &MappedMdl;
    size_t currentMdlBytesToCopy = MmGetMdlByteCount(currentMdl) - MdlOffset;
    size_t currentMdlOffset = MdlOffset;
    size_t remain = BytesToCopy;

    for (auto i = 0u; i < Sgl->NumberOfElements; i++)
    {
        if (it == AvailableFragments.end())
        {
            auto numberOfFragments = NetRbFragmentRange(AvailableFragments.begin(), it).Count();
            numberOfFragments += Sgl->NumberOfElements - i;

            // We might never have enough fragments to translate this packet. Bounce the buffers.
            if (numberOfFragments > MaximumNumberOfFragments())
            {
                return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
            }

            return { NxNblTranslationStatus::InsufficientResources, EmptyFragmentRange() };
        }

        auto& currentFragment = *(it++);
        RtlZeroMemory(&currentFragment, NetPacketFragmentGetSize());

        SCATTER_GATHER_ELEMENT const &sge = Sgl->Elements[i];

        currentFragment.Capacity = sge.Length;
        currentFragment.ValidLength = sge.Length;
        currentFragment.Offset = 0;
        currentFragment.Mapping.DmaLogicalAddress = sge.Address;

        void *kvm = MmGetSystemAddressForMdlSafe(currentMdl, LowPagePriority | MdlMappingNoExecute);

        NT_ASSERT(kvm != nullptr);

        // The fragment offset has to be zero because Scatter/Gather elements do not have an offset, so we need to adjust the virtual address
        // as appropriate
        currentFragment.VirtualAddress = static_cast<UCHAR *>(kvm) + currentMdlOffset;

        if (ShouldBounceFragment(currentFragment))
        {
            return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
        }

        currentMdlBytesToCopy -= static_cast<size_t>(currentFragment.ValidLength);
        remain -= static_cast<size_t>(currentFragment.ValidLength);
        currentMdlOffset += static_cast<size_t>(currentFragment.ValidLength);

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
        return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
    }

    return { NxNblTranslationStatus::Success, NetRbFragmentRange(AvailableFragments.begin(), it) };
}

_Use_decl_annotations_
bool
NxNblTranslator::MapMdlChainToSystemVA(
    MDL &Mdl
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
    MDL &Mdl,
    size_t MdlOffset,
    size_t BytesToCopy,
    NxDmaTransfer const &DmaTransfer,
    NetRbFragmentRange const &AvailableFragments
    ) const
{
    NxScatterGatherList sgl { *m_dmaAdapter };

    // Build the scatter/gather list using HAL
    NTSTATUS buildSglStatus = m_dmaAdapter->BuildScatterGatherListEx(
        DmaTransfer,
        &Mdl,
        MdlOffset,
        BytesToCopy,
        sgl.releaseAndGetAddressOf());

    if (buildSglStatus == STATUS_INSUFFICIENT_RESOURCES)
    {
        // DMA engine might be out of map registers or some other resource, let's try again later
        m_stats.DMA.InsufficientResouces += 1;
        return { NxNblTranslationStatus::InsufficientResources, EmptyFragmentRange() };
    }
    else if (buildSglStatus == STATUS_BUFFER_TOO_SMALL)
    {
        // Our pre-allocated SGL did not have enough space to hold the Scatter/Gather list, bounce the packet
        m_stats.DMA.BufferTooSmall += 1;
        return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
    }
    else if (buildSglStatus != STATUS_SUCCESS)
    {
        // If we can't map this packet using HAL APIs let's try to bounce it
        m_stats.DMA.OtherErrors += 1;
        return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
    }

    auto const mappedMdl = m_dmaAdapter->GetMappedMdl(
        sgl.get(),
        &Mdl);

    if (mappedMdl == nullptr)
    {
        // We need the mapped MDL chain to be able to translate the Scatter/Gather list
        // to a chain of NET_PACKET_FRAGMENTs, if we don't have it we need to bounce
        return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
    }

    if (!CanTranslateSglToNetPacket(*sgl, *mappedMdl))
    {
        // If this is the case we can't calculate the VAs of each physical fragment. This should be really rare,
        // so instead of trying to create MDLs and map each physical fragment into VA space just bounce the buffers
        m_stats.DMA.CannotMapSglToFragments += 1;
        return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
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
            return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
        }
    }

    auto result = TranslateScatterGatherListToFragmentRange(
        *mappedMdl,
        BytesToCopy,
        MdlOffset,
        sgl,
        AvailableFragments);

    if (result.Status == NxNblTranslationStatus::Success)
    {
        // Release the ownership of the SGL and save the pointer to it in
        // the packet's DMA context. When the packet is completed we will
        // call PutScatterGatherList
        dmaContext.ScatterGatherList = sgl.release();
    }

    return result;
}

static
size_t
CountNumberOfPages(
    _In_ MDL &MdlChain,
    _In_ size_t MdlOffset,
    _In_ size_t ByteCount
    )
{
    size_t numberOfPages = 0;
    auto mdl = &MdlChain;

    for (size_t remain = ByteCount; mdl && (remain > 0); mdl = mdl->Next)
    {
        // Skip zero length MDLs.
        auto const mdlByteCount = MmGetMdlByteCount(mdl);
        if (mdlByteCount == 0)
        {
            continue;
        }

        auto const copySize = min(remain, mdlByteCount - MdlOffset);

        ULONG_PTR vaStart = reinterpret_cast<ULONG_PTR>(MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute)) + MdlOffset;
        ULONG_PTR vaEnd = vaStart + static_cast<ULONG>(copySize);

        if (vaStart == vaEnd)
        {
            continue;
        }

        numberOfPages += ADDRESS_AND_SIZE_TO_SPAN_PAGES(vaStart, copySize);
        remain -= copySize;
        MdlOffset = 0;
    }

    return numberOfPages;
}

_Use_decl_annotations_
MdlTranlationResult
NxNblTranslator::TranslateMdlChainToDmaMappedFragmentRangeBypassHal(
    MDL &Mdl,
    size_t MdlOffset,
    size_t BytesToCopy,
    NetRbFragmentRange const &AvailableFragments
    ) const
{
    auto it = AvailableFragments.begin();

    //
    // While there is remaining data to be copied, and we have MDLs to walk...
    //

    auto mdl = &Mdl;
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
            size_t numberOfFragments = NetRbFragmentRange(AvailableFragments.begin(), it).Count();

            if (numberOfFragments > m_datapathCapabilities.MaximumNumberOfTxFragments)
            {
                return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
            }

            if (it == AvailableFragments.end())
            {
                size_t const currentMdlOffset = va - vaStart + MdlOffset;

                // The total number of fragments is whatever we translated so far plus the number of
                // remaining pages in the mdl chain
                numberOfFragments += CountNumberOfPages(
                    *mdl,
                    currentMdlOffset,
                    remain);

                if (numberOfFragments > MaximumNumberOfFragments())
                {
                    return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
                }

                return { NxNblTranslationStatus::InsufficientResources, EmptyFragmentRange() };
            }

            // Get the current fragment, increment the iterator
            auto& currentFragment = *(it++);
            RtlZeroMemory(&currentFragment, NetPacketFragmentGetSize());

            // Performance can be optimized by coalescing adjacent fragments

            currentFragment.Mapping.DmaLogicalAddress = MmGetPhysicalAddress(reinterpret_cast<void *>(va));
            currentFragment.VirtualAddress = reinterpret_cast<void *>(va);

            if (ShouldBounceFragment(currentFragment))
            {
                return { NxNblTranslationStatus::BounceRequired, EmptyFragmentRange() };
            }

            auto fragmentLength = PAGE_ALIGN(va) != PAGE_ALIGN(vaEnd) ?
                PAGE_SIZE - BYTE_OFFSET(va) :
                (ULONG)(vaEnd - va);

            currentFragment.ValidLength = fragmentLength;
            currentFragment.Offset = 0;
            currentFragment.Capacity = currentFragment.ValidLength;

            remain -= fragmentLength;
        }

        MdlOffset = 0;
    }

    return { NxNblTranslationStatus::Success, NetRbFragmentRange(AvailableFragments.begin(), it) };
}

_Use_decl_annotations_
NetRbPacketIterator
NxNblTranslator::TranslateNbls(
    NET_BUFFER_LIST *&currentNbl,
    NET_BUFFER *&currentNetBuffer,
    NetRbPacketRange const &rb,
    NxBounceBufferPool &BouncePool
    ) const
{
    for (auto currentPacket = rb.begin(); currentPacket != rb.end(); currentPacket++)
    {
        switch (TranslateNetBufferToNetPacket(*currentNetBuffer, &(*currentPacket)))
        {
        case NxNblTranslationStatus::BounceRequired:
            // The buffers in the NET_BUFFER's MDL chain cannot be transmitted as is. As such we need
            // to bounce the packet

            if(!BouncePool.BounceNetBuffer(*currentNetBuffer, *currentPacket))
            {
                if (currentPacket->IgnoreThisPacket)
                {
                    // If it was not possible to bounce the NET_BUFFER and the
                    // current packet was marked to be ignored we should *not*
                    // try to translate it again.
                    m_stats.Packet.CannotTranslate += 1;
                    break;
                }
                else
                {
                    // It was not possible to bounce the NET_BUFFER because we're
                    // out of some resource. Try again later.
                    m_stats.Packet.BounceFailure += 1;
                    return currentPacket;
                }
            }

            m_stats.Packet.BounceSuccess += 1;
            __fallthrough;

        case NxNblTranslationStatus::Success:
            currentPacket->Layout = NxGetPacketLayout(m_mediaType, &m_datapathDescriptor, &(*currentPacket));
            TranslateNetBufferListOOBDataToNetPacketExtensions(*currentNbl, &(*currentPacket));
            break;
        case NxNblTranslationStatus::InsufficientResources:
            // There are not enough resources at the moment to translate the NET_BUFFER,
            // stop processing here. Once there are enough resources available this will
            // make forward progress
            return currentPacket;

        case NxNblTranslationStatus::CannotTranslate:
            // For some reason we won't ever be able to translate the NET_BUFFER,
            // mark the corresponding NET_PACKET to be dropped.
            currentPacket->IgnoreThisPacket = true;
            currentPacket->FragmentCount = 0;
            m_stats.Packet.CannotTranslate += 1;
            break;
        }

        if (currentNetBuffer->Next)
        {
            currentNetBuffer = currentNetBuffer->Next;
        }
        else
        {
            // This is the final NB in the NBL, so let's bundle the NBL
            // up with the NET_PACKET.  We'll find it later when completing packets.

            auto &currentPacketExtension = m_contextBuffer.GetPacketContext<PacketContext>(*currentPacket);
            currentPacketExtension.NetBufferListToComplete = currentNbl;

            // Now let's advance to the next NBL.
            currentNbl = currentNbl->Next;

            // If this was the last NBL, we're done for now.  Remember which packet is next.
            if (!currentNbl)
                return ++currentPacket;

            currentNetBuffer = currentNbl->FirstNetBuffer;

#if defined(DBG) && defined(_KERNEL_MODE)
            // We're trying to avoid writing to the NBL now, but that means it will
            // temporarily hold a stale pointer.  Scribble a bogus value here to
            // ensure nobody tries to dereference it until the NBL is completed and
            // a correct value is written here.
            currentPacketExtension.NetBufferListToComplete->Next = (NET_BUFFER_LIST*)MM_BAD_POINTER;
#endif
        }
    }

    return rb.end();
}


void
ReusePackets(
    _In_ NET_DATAPATH_DESCRIPTOR const* descriptor,
    _In_ NetRbPacketRange &packets
    )
{
    if (packets.Count() == 0)
        return;

    auto const beginIndex = packets.begin().GetIndex();
    auto const endIndex = packets.end().GetIndex();

    auto pRb = const_cast<NET_RING_BUFFER *>(&packets.RingBuffer());

    if (beginIndex < endIndex)
    {
        // We didn't wrap around this time, so we can do it in one go.
        NetPacketReuseMany(
            descriptor,
            packets.At(0),
            pRb->ElementStride,
            packets.Count());
    }
    else
    {
        // Wraparound: clean the first few packets, then the last few packets.

        if (endIndex > 0)
        {
            NetPacketReuseMany(
                descriptor,
                NetRingBufferGetPacketAtIndex(pRb, 0),
                pRb->ElementStride,
                endIndex);
        }

        NetPacketReuseMany(
            descriptor,
            NetRingBufferGetPacketAtIndex(pRb, beginIndex),
            pRb->ElementStride,
            (pRb->NumberOfElements - beginIndex));
    }
}

_Use_decl_annotations_
void
NxNblTranslator::TranslateNetPacketExtensionsCompletionToNetBufferList(
    const NET_PACKET *netPacket,
    PNET_BUFFER_LIST netBufferList
    ) const
{
    UINT64 totalPacketSize = 0;
    ULONG lsoTcpHeaderOffset = 0;

    if ((netPacket->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP) &&
        (IsPacketLargeSendSegmentationEnabled()))
    {
        // lso requires special markings upon completion.
        auto &lsoInfo =
            *reinterpret_cast<NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO*>(
                &netBufferList->NetBufferListInfo[TcpLargeSendNetBufferListInfo]);

        if (lsoInfo.Value != 0)
        {
            auto lsoType = lsoInfo.Transmit.Type;
            lsoInfo.Value = 0;

            switch (lsoType)
            {
            case NDIS_TCP_LARGE_SEND_OFFLOAD_V1_TYPE:
                {
                    lsoTcpHeaderOffset = lsoInfo.LsoV1Transmit.TcpHeaderOffset;

                    for (UINT32 i = 0; i < netPacket->FragmentCount; i++)
                    {
                        totalPacketSize += NET_PACKET_GET_FRAGMENT(netPacket, &m_datapathDescriptor, i)->ValidLength;
                    }

                    lsoInfo.LsoV1TransmitComplete.Type = lsoType;
                    lsoInfo.LsoV1TransmitComplete.TcpPayload = (ULONG)(totalPacketSize - lsoTcpHeaderOffset);
                }
                break;
            case NDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE:
                lsoInfo.LsoV2TransmitComplete.Type = lsoType;
                break;
            }
        }
    }
    else
    {
        auto const &lsoInfo =
            *reinterpret_cast<NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO*>(
                &netBufferList->NetBufferListInfo[TcpLargeSendNetBufferListInfo]);
        WIN_VERIFY(lsoInfo.Value == 0);
    }
}

_Use_decl_annotations_
TxPacketCompletionStatus
NxNblTranslator::CompletePackets(
    NetRbPacketRange const &rb,
    NxBounceBufferPool &BouncePool
    ) const
{
    TxPacketCompletionStatus result{ rb.begin() };

    for (; result.CompletedTo != rb.end(); ++result.CompletedTo)
    {
        auto &extension = m_contextBuffer.GetPacketContext<PacketContext>(*result.CompletedTo);

        // Release any DMA resources allocated for this packet
        if (m_dmaAdapter)
        {
            m_dmaAdapter->CleanupNetPacket(*result.CompletedTo);
        }

        // Free any bounce buffers allocated for this packet
        BouncePool.FreeBounceBuffers(*result.CompletedTo);

        if (auto completedNbl = extension.NetBufferListToComplete)
        {
            extension.NetBufferListToComplete = nullptr;

            completedNbl->Status = NDIS_STATUS_SUCCESS;

            completedNbl->Next = result.CompletedChain;
            result.CompletedChain = completedNbl;

            TranslateNetPacketExtensionsCompletionToNetBufferList(
                &(*result.CompletedTo),
                completedNbl);

            result.NumCompletedNbls += 1;
        }

        DetachFragmentsFromPacket(*result.CompletedTo, m_datapathDescriptor);
    }

    NetRbPacketRange completed{ rb.begin(), result.CompletedTo };

    ReusePackets(&m_datapathDescriptor, completed);

    return result;
}

bool
NxNblTranslator::IsPacketChecksumEnabled() const
{
    return m_netPacketChecksumOffset != NET_PACKET_EXTENSION_INVALID_OFFSET;
}

bool
NxNblTranslator::IsPacketLargeSendSegmentationEnabled() const
{
    return m_netPacketLsoOffset != NET_PACKET_EXTENSION_INVALID_OFFSET;
}
