// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxNblTranslation.tmh"

#include "NxNblTranslation.hpp"

#include "NxPacketLayout.hpp"
#include "NxChecksumInfo.hpp"
#include "NxLargeSend.hpp"

NxNblTranslator::NxNblTranslator(
    const NxContextBuffer & ContextBuffer,
    NDIS_MEDIUM MediaType
    ) : 
    m_contextBuffer(ContextBuffer),
    m_mediaType(MediaType)
{

}

_Use_decl_annotations_
ULONG
NxNblTranslator::CountNumberOfFragments(
    NET_BUFFER const & nb
    )
/*

Description:
    
    Given a NET_BUFFER, count how many NET_PACKET_FRAGMENTs
    are needed to describe the frame.






*/
{
    ULONG count = 0;
    PMDL mdl = NET_BUFFER_CURRENT_MDL(&nb);
    size_t mdlOffset = NET_BUFFER_CURRENT_MDL_OFFSET(&nb);
    auto bytesToCopy = NET_BUFFER_DATA_LENGTH(&nb);

    for (size_t remain = bytesToCopy; mdl && (remain > 0); mdl = mdl->Next)
    {
        //
        // Skip zero length MDLs.
        //

        size_t const mdlByteCount = MmGetMdlByteCount(mdl);
        if (mdlByteCount == 0)
        {
            continue;
        }

        ASSERT(mdlOffset < mdlByteCount);

        remain -= min(remain, mdlByteCount - mdlOffset);
        mdlOffset = 0;

        count++;
    }

    return count;
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
    NET_BUFFER const & netBuffer,
    NET_BUFFER_LIST const &netBufferList,
    NET_DATAPATH_DESCRIPTOR const* descriptor,
    NET_PACKET* netPacket
    ) const
{
    PMDL mdl = NET_BUFFER_CURRENT_MDL(&netBuffer);
    size_t mdlOffset = NET_BUFFER_CURRENT_MDL_OFFSET(&netBuffer);
    auto bytesToCopy = NET_BUFFER_DATA_LENGTH(&netBuffer);

    if (bytesToCopy == 0)
    {
        netPacket->IgnoreThisPacket = TRUE;
        netPacket->FragmentValid = FALSE;
        return NxNblTranslationStatus::Success;
    }

    auto& fragmentRing = *NET_DATAPATH_DESCRIPTOR_GET_FRAGMENT_RING_BUFFER(descriptor);
    auto availableFragments = NetRbFragmentRange::OsRange(fragmentRing);
    auto it = availableFragments.begin();

    //
    // While there is remaining data to be copied, and we have MDLs to walk...
    //

    for (size_t remain = bytesToCopy; mdl && (remain > 0); mdl = mdl->Next)
    {
        if (it == availableFragments.end())
        {
            auto numberOfFragments = NetRbFragmentRange(availableFragments.begin(), it).Count();

            // We need to know how many fragments this packet has
            // because we might never have enough fragments to
            // translate it
            while (mdl)
            {
                numberOfFragments++;
                mdl = mdl->Next;
            }

            if (numberOfFragments > (fragmentRing.NumberOfElements - 1))
                return NxNblTranslationStatus::CannotTranslate;

            return NxNblTranslationStatus::InsufficientFragments;
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

        ASSERT(mdlOffset < mdlByteCount);
        size_t const copySize = min(remain, mdlByteCount - mdlOffset);

        //
        // Perform the transfer
        //
        currentFragment.Offset = mdlOffset;

        // NDIS6 requires MDLs to be mapped to system address space, so
        // retrieving SystemAddress should not fail.
        currentFragment.VirtualAddress = MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute);
        NT_ASSERT(currentFragment.VirtualAddress);

        currentFragment.Mapping.Mdl = mdl;

        currentFragment.Capacity = mdlByteCount;
        currentFragment.ValidLength = copySize;

        mdlOffset = 0;
        remain -= copySize;

        currentFragment.LastFragmentOfFrame = (remain == 0);
    }

    // Attach the fragment chain to the packet
    netPacket->FragmentValid = TRUE;
    netPacket->FragmentOffset = availableFragments.begin().GetIndex();
    fragmentRing.EndIndex = it.GetIndex();

    netPacket->Layout = NxGetPacketLayout(m_mediaType, descriptor, netPacket);

    TranslateNetBufferListOOBDataToNetPacketExtensions(netBufferList, netPacket);

    return NxNblTranslationStatus::Success;
}

_Use_decl_annotations_
NetRbPacketIterator
NxNblTranslator::TranslateNbls(
    NET_BUFFER_LIST *&currentNbl,
    NET_BUFFER *&currentNetBuffer,
    NET_DATAPATH_DESCRIPTOR const* descriptor,
    NetRbPacketRange const &rb
    ) const
{
    for (auto currentPacket = rb.begin(); currentPacket != rb.end(); currentPacket++)
    {
        switch (TranslateNetBufferToNetPacket(*currentNetBuffer, *currentNbl, descriptor, &(*currentPacket)))
        {
        case NxNblTranslationStatus::InsufficientFragments:
            // There are not enough fragments to translate the NET_BUFFERs, stop
            // processing here. This will make forward progress only once packets
            // are completed and fragments are returned to the fragment pool
            return currentPacket;

        case NxNblTranslationStatus::CannotTranslate:
            // For some reason we won't ever be able to translate the NET_BUFFER,
            // mark the corresponding NET_PACKET to be dropped.
            currentPacket->IgnoreThisPacket = true;
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
    NET_DATAPATH_DESCRIPTOR const* descriptor,
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
                    UINT32 fragmentCount = NetPacketGetFragmentCount(descriptor, netPacket);
                    lsoTcpHeaderOffset = lsoInfo.LsoV1Transmit.TcpHeaderOffset;

                    for (UINT32 i = 0; i < fragmentCount; i++)
                    {
                        totalPacketSize += NET_PACKET_GET_FRAGMENT(netPacket, descriptor, i)->ValidLength;
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
    NET_DATAPATH_DESCRIPTOR const* descriptor,
    NetRbPacketRange const &rb
    ) const
{
    TxPacketCompletionStatus result{ rb.begin() };

    for (; result.CompletedTo != rb.end(); ++result.CompletedTo)
    {
        auto &extension = m_contextBuffer.GetPacketContext<PacketContext>(*result.CompletedTo);

        if (auto completedNbl = extension.NetBufferListToComplete)
        {
            extension.NetBufferListToComplete = nullptr;

            completedNbl->Status = NDIS_STATUS_SUCCESS;

            completedNbl->Next = result.CompletedChain;
            result.CompletedChain = completedNbl;

            TranslateNetPacketExtensionsCompletionToNetBufferList(
                descriptor,
                &(*result.CompletedTo),
                completedNbl);

            result.NumCompletedNbls += 1;
        }

        DetachFragmentsFromPacket(*result.CompletedTo, *descriptor);
    }

    NetRbPacketRange completed{ rb.begin(), result.CompletedTo };

    ReusePackets(descriptor, completed);

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
