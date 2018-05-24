// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxBounceBufferPool.tmh"

#include "NxBounceBufferPool.hpp"

NxBounceBufferPool::~NxBounceBufferPool(
    void
    )
{
    if (m_bufferPool)
    {
        m_bufferPoolDispatch->NetClientDestroyBufferPool(m_bufferPool);
        m_bufferPool = nullptr;
    }
}

_Use_decl_annotations_
NTSTATUS
NxBounceBufferPool::Initialize(
    NET_CLIENT_DISPATCH const &ClientDispatch,
    NET_DATAPATH_DESCRIPTOR const *Descriptor,
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES &DatapathCapabilities,
    size_t NumberOfBuffers
    )
{
    m_descriptor = Descriptor;
    m_bufferSize = DatapathCapabilities.MaximumTxFragmentSize;

    NET_CLIENT_BUFFER_POOL_CONFIG bufferPoolConfig = {
        &DatapathCapabilities.TxMemoryConstraints,
        NumberOfBuffers,
        m_bufferSize,
        0,
        0,
        MM_ANY_NODE_OK,
        NET_CLIENT_BUFFER_POOL_FLAGS_NONE
    };

    CX_RETURN_IF_NOT_NT_SUCCESS(
        ClientDispatch.NetClientCreateBufferPool(&bufferPoolConfig,
            &m_bufferPool,
            &m_bufferPoolDispatch));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
bool
NxBounceBufferPool::BounceNetBuffer(
    NET_BUFFER const &NetBuffer,
    NET_PACKET &NetPacket
    )
/*

Description:

    This routine tries to allocate a buffer from the buffer pool and bounce
    the payload described by NetBuffer into *one* NET_PACKET_FRAGMENT.

Return value:

    true - Bounce operation was successful. NetPacket has one fragment.
    false - Bounce operation was unsuccessful. NetPacket has no fragments.

Remarks:

    If this routine returns false and NetPacket.IgnoreThisPacket is TRUE the
    caller should not try to bounce the buffer again.
*/
{
    auto& fragmentRing = *NET_DATAPATH_DESCRIPTOR_GET_FRAGMENT_RING_BUFFER(m_descriptor);
    auto availableFragments = NetRbFragmentRange::OsRange(fragmentRing);

    if (availableFragments.Count() == 0)
    {
        return false;
    }

    auto& fragment = *availableFragments.begin();
    RtlZeroMemory(&fragment, NetPacketFragmentGetSize());

    auto allocatedCount = m_bufferPoolDispatch->NetClientAllocateBuffers(
        m_bufferPool,
        &fragment,
        1);

    if (allocatedCount != 1)
    {
        return false;
    }

    fragment.LastFragmentOfFrame = TRUE;
    fragment.OsReserved_Bounced = TRUE;

    PMDL mdl = NET_BUFFER_CURRENT_MDL(&NetBuffer);
    size_t mdlOffset = NET_BUFFER_CURRENT_MDL_OFFSET(&NetBuffer);
    auto bytesToCopy = NET_BUFFER_DATA_LENGTH(&NetBuffer);

    if (bytesToCopy == 0 || bytesToCopy > m_bufferSize)
    {
        NetPacket.IgnoreThisPacket = TRUE;
        NetPacket.FragmentValid = FALSE;
        return false;
    }

    size_t currentFragmentOffset = fragment.Offset;
    auto baseFragmentVa = static_cast<UCHAR *>(fragment.VirtualAddress);
    for (size_t remain = bytesToCopy; remain > 0; mdl = mdl->Next)
    {
        size_t const mdlByteCount = MmGetMdlByteCount(mdl);
        if (mdlByteCount == 0)
        {
            continue;
        }

        NT_ASSERT(mdlByteCount > mdlOffset);

        size_t const copySize = min(remain, mdlByteCount - mdlOffset);

        void *sourceBuffer = static_cast<UCHAR *>(MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute)) + mdlOffset;
        void *destinationBuffer = baseFragmentVa + currentFragmentOffset;

        // If we make the parsing code optional or parse the packets in
        // batches we might benefit from using RtlCopyMemoryNonTemporal
        RtlCopyMemory(
            destinationBuffer,
            sourceBuffer,
            copySize);

        fragment.ValidLength += copySize;

        mdlOffset = 0;
        remain -= copySize;
        currentFragmentOffset += copySize;
    }

    if (fragment.ValidLength != bytesToCopy)
    {
        NetPacket.IgnoreThisPacket = TRUE;
        NetPacket.FragmentValid = FALSE;
        return false;
    }

    // Attach the fragment chain to the packet
    NetPacket.FragmentValid = TRUE;
    NetPacket.FragmentOffset = availableFragments.begin().GetIndex();
    fragmentRing.EndIndex = NetRingBufferIncrementIndex(&fragmentRing, fragmentRing.EndIndex);

    return true;
}

_Use_decl_annotations_
void
NxBounceBufferPool::FreeBounceBuffers(
    NET_PACKET &NetPacket
    )
{
    if (NetPacket.IgnoreThisPacket)
    {
        return;
    }

    for (auto i = 0; ; i++)
    {
        auto fragment = NET_PACKET_GET_FRAGMENT(&NetPacket, m_descriptor, i);

        if (fragment->OsReserved_Bounced)
        {
            m_bufferPoolDispatch->NetClientFreeBuffers(
                m_bufferPool,
                &fragment->VirtualAddress,
                1);
        }

        if (fragment->LastFragmentOfFrame)
        {
            break;
        }
    }
}
