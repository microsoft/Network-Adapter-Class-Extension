// Copyright (C) Microsoft Corporation. All rights reserved.
#include "bmprecomp.hpp"

#include "CommonBufferVector.hpp"
#include "DmaAllocator.hpp"
#include "DmaAllocator.tmh"

_Use_decl_annotations_
DmaAllocator::DmaAllocator(
    NET_CLIENT_MEMORY_CONSTRAINTS::DMA & Dma
)
    : m_DmaAdapter(reinterpret_cast<DMA_ADAPTER *>(Dma.DmaAdapter))
    , m_MaximumPhysicalAddress(Dma.MaximumPhysicalAddress)
    , m_PreferredNode(Dma.PreferredNode)
{
    if (Dma.CacheEnabled == NET_CLIENT_TRI_STATE_DEFAULT)
    {
#if _AMD64_ || _X86_
        m_CacheEnabled = true;
#else
        m_CacheEnabled = false;
#endif
    }
    else
    {
        m_CacheEnabled = Dma.CacheEnabled;
    }
}

_Use_decl_annotations_
IBufferVector *
DmaAllocator::AllocateVector(
    size_t BufferCount,
    size_t BufferSize,
    size_t BufferOffset,
    NODE_REQUIREMENT PreferredNode
)
{
    NODE_REQUIREMENT node = PreferredNode == MM_ANY_NODE_OK
        ? m_PreferredNode
        : PreferredNode;

    auto bufferVector = wil::make_unique_nothrow<CommonBufferVector>(
        m_DmaAdapter,
        BufferCount,
        BufferSize,
        BufferOffset);

    if (!bufferVector)
    {
        return nullptr;
    }

    // Currently AllocateCommonBufferVector takes in MI_NODE_NUMBER_ZERO_BASED
    // as NUMA node parameter, so we have to convert MM_ANY_NODE_OK to an actual
    // node id ourself - pick node 0
    if (node == MM_ANY_NODE_OK)
    {
        node = 0;
    }

    auto const status = m_DmaAdapter->DmaOperations->AllocateCommonBufferVector(
        m_DmaAdapter,
        PHYSICAL_ADDRESS{},
        m_MaximumPhysicalAddress.QuadPart != 0 ? m_MaximumPhysicalAddress : PHYSICAL_ADDRESS{ 0xffffffff, -1 },
        m_CacheEnabled ? MmCached : MmNonCached,
        node,
        0,
        static_cast<ULONG>(BufferCount),
        static_cast<ULONG>(BufferSize),
        &bufferVector->m_CommonBufferVector);

    if (status != STATUS_SUCCESS)
    {
        return nullptr;
    }

    return bufferVector.release();
}
