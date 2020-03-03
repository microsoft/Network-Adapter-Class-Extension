// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxBounceBufferPool.tmh"

#include "NxBounceBufferPool.hpp"

#include <net/ring.h>
#include <net/fragment.h>
#include <net/logicaladdress_p.h>
#include <net/virtualaddress_p.h>

struct FragmentContext
{
    NET_CLIENT_BUFFER_POOL BufferPool;
    NET_CLIENT_BUFFER_POOL_DISPATCH const * BufferPoolDispatch;
};

NxBounceBufferPool::NxBounceBufferPool(
    NET_RING_COLLECTION const & Rings,
    NET_EXTENSION const & VirtualAddressExtension,
    NET_EXTENSION const & LogicalAddressExtension
)
    : m_virtualAddressExtension(VirtualAddressExtension)
    , m_logicalAddressExtension(LogicalAddressExtension)
    , m_rings(&Rings)
    , m_fragmentContext(Rings, NetRingTypeFragment)
{
}

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
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES &DatapathCapabilities,
    size_t NumberOfBuffers
)
{
    //
    // Current bouncing logic ensures that a bounced NET_PACKET always has a single NET_FRAGMENT
    // So we can include NET_PACKET backfill in the buffer size
    //
    m_txPayloadBackfill = DatapathCapabilities.TxPayloadBackfill;
    m_bufferSize = DatapathCapabilities.MaximumTxFragmentSize + m_txPayloadBackfill;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_fragmentContext.Initialize(sizeof(FragmentContext)),
        "Failed to initialize private fragment context.");

    //
    // Note: NxRingContext zeroes all the elements in the ring, so no need to initialize each
    // element here
    //

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
    the payload described by NetBuffer into *one* NET_FRAGMENT.

Return value:

    true - Bounce operation was successful. NetPacket has one fragment.
    false - Bounce operation was unsuccessful. NetPacket has no fragments.

Remarks:

    If this routine returns false and NetPacket.Ignore is TRUE the
    caller should not try to bounce the buffer again.
*/
{
    auto fr = NetRingCollectionGetFragmentRing(m_rings);
    auto const frOsBegin = fr->EndIndex;
    auto const frOsEnd = (fr->BeginIndex - 1) & fr->ElementIndexMask;

    if (frOsBegin == frOsEnd)
    {
        return false;
    }

    auto const bytesToCopy = NET_BUFFER_DATA_LENGTH(&NetBuffer);
    auto const packetSize = bytesToCopy + m_txPayloadBackfill;

    if (bytesToCopy == 0 || packetSize > m_bufferSize)
    {
        NetPacket.Ignore = TRUE;
        NetPacket.FragmentCount = 0;
        return false;
    }

    auto fragment = NetRingGetFragmentAtIndex(fr, frOsBegin);
    auto virtualAddress = NetExtensionGetFragmentVirtualAddress(
        &m_virtualAddressExtension, frOsBegin);
    auto logicalAddress = NetExtensionGetFragmentLogicalAddress(
        &m_logicalAddressExtension, frOsBegin);

    RtlZeroMemory(fragment, sizeof(NET_FRAGMENT));

    SIZE_T offset, capacity;
    if (STATUS_SUCCESS != m_bufferPoolDispatch->NetClientAllocateBuffer(
            m_bufferPool,
            &virtualAddress->VirtualAddress,
            &logicalAddress->LogicalAddress,
            &offset,
            &capacity))
    {
        return false;
    }

    fragment->Offset = offset;
    fragment->Capacity = capacity;
    fragment->ValidLength = m_txPayloadBackfill;

    PMDL mdl = NET_BUFFER_CURRENT_MDL(&NetBuffer);
    size_t mdlOffset = NET_BUFFER_CURRENT_MDL_OFFSET(&NetBuffer);

    size_t fragmentOffset = (size_t)fragment->Offset + m_txPayloadBackfill;
    auto baseFragmentVa = static_cast<unsigned char *>(virtualAddress->VirtualAddress);
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
        void *destinationBuffer = baseFragmentVa + fragmentOffset;

        // If we make the parsing code optional or parse the packets in
        // batches we might benefit from using RtlCopyMemoryNonTemporal
        RtlCopyMemory(
            destinationBuffer,
            sourceBuffer,
            copySize);

        fragment->ValidLength += copySize;

        mdlOffset = 0;
        remain -= copySize;
        fragmentOffset += copySize;
    }

    if (fragment->ValidLength != packetSize)
    {
        m_bufferPoolDispatch->NetClientFreeBuffers(m_bufferPool, &virtualAddress->VirtualAddress, 1);
        NetPacket.Ignore = TRUE;
        NetPacket.FragmentCount = 0;

        return false;
    }

    // Attach the fragment chain to the packet
    NetPacket.FragmentCount = 1;
    NetPacket.FragmentIndex = frOsBegin;
    fr->EndIndex = NetRingIncrementIndex(fr, fr->EndIndex);

    // Save the buffer pool information so that we can return the bounce buffer on completion
    auto & fragmentContext = m_fragmentContext.GetContext<FragmentContext>(frOsBegin);
    fragmentContext.BufferPool = m_bufferPool;
    fragmentContext.BufferPoolDispatch = m_bufferPoolDispatch;

    return true;
}

_Use_decl_annotations_
void
NxBounceBufferPool::FreeBounceBuffers(
    NET_PACKET &NetPacket
)
{
    if (NetPacket.Ignore)
    {
        return;
    }

    auto fr = NetRingCollectionGetFragmentRing(m_rings);
    for (UINT32 i = 0; i < NetPacket.FragmentCount; i++)
    {
        auto const index = (NetPacket.FragmentIndex + i) & fr->ElementIndexMask;
        auto & fragmentContext = m_fragmentContext.GetContext<FragmentContext>(index);
        auto virtualAddress = NetExtensionGetFragmentVirtualAddress(
            &m_virtualAddressExtension, index);

        if (fragmentContext.BufferPool != nullptr)
        {
            fragmentContext.BufferPoolDispatch->NetClientFreeBuffers(
                fragmentContext.BufferPool,
                &virtualAddress->VirtualAddress,
                1);

            fragmentContext = {};
        }
    }
}

