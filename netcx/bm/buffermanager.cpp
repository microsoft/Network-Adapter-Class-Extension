// Copyright (C) Microsoft Corporation. All rights reserved.

#include "bmprecomp.hpp"

#include "BufferPool.hpp"
#include "BufferManager.hpp"
#include "BufferManager.tmh"

namespace NetCx {

namespace Core {

class PAGED NonPagePoolBufferVector : public IBufferVector
{
    friend class NonPagePoolAllocator;

public:

    NonPagePoolBufferVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset)
    :   IBufferVector(BufferCount, BufferSize, BufferOffset) {}

    ~NonPagePoolBufferVector()
    {
        for (size_t i = 0; i < m_BufferCount; i++)
        {
            if (m_NonPagePoolBufferVector[i])
            {
                ExFreePool(m_NonPagePoolBufferVector[i]);
                m_NonPagePoolBufferVector[i] = nullptr;
            }
        }
    }

    void
    GetEntryByIndex(
        _In_ size_t Index,
        _Out_ NET_DATA_HEADER* NetDataHeader
    ) const override
    {
        NetDataHeader->VirtualAddress = m_NonPagePoolBufferVector[Index];
        NetDataHeader->LogicalAddress = 0ULL;
    }

private:

    Rtl::KArray<PVOID> m_NonPagePoolBufferVector;
};

class PAGED NonPagePoolAllocator : public IBufferVectorAllocator
{
public:

    IBufferVector*
    AllocateVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset,
        _In_ NODE_REQUIREMENT)
    {
        wistd::unique_ptr<NonPagePoolBufferVector> bufferVector =
            wil::make_unique_nothrow<NonPagePoolBufferVector>(BufferCount, BufferSize, BufferOffset);

        if (!bufferVector)
        {
            return nullptr;
        }

        if (!bufferVector->m_NonPagePoolBufferVector.resize(BufferCount))
        {
            return nullptr;
        }

        for (size_t i = 0; i < BufferCount; i++)
        {
            bufferVector->m_NonPagePoolBufferVector[i] =
                ExAllocatePoolWithTag(NonPagedPoolNx, BufferSize, BUFFER_MANAGER_POOL_TAG);

            if (bufferVector->m_NonPagePoolBufferVector[i] == nullptr)
            {
                return nullptr;
            }
        }

        return bufferVector.release();
    }
};

class PAGED CommonBufferVector : public IBufferVector
{
    friend class DmaAllocator;

public:

    CommonBufferVector(_In_ PDMA_ADAPTER DmaAdapter,
                       _In_ size_t BufferCount,
                       _In_ size_t BufferSize,
                       _In_ size_t BufferOffset)
    :   IBufferVector(BufferCount, BufferSize, BufferOffset),
        m_DmaAdapter(DmaAdapter) {}

    ~CommonBufferVector()
    {
        // Free the common buffer vector if one was allocated
        if (m_CommonBufferVector != nullptr)
        {
            m_DmaAdapter->DmaOperations->FreeCommonBufferVector(m_DmaAdapter, m_CommonBufferVector);
            m_CommonBufferVector = nullptr;
        }
    }

    void
    GetEntryByIndex(
        _In_ size_t Index,
        _Out_ NET_DATA_HEADER* NetDataHeader
    ) const override
    {
        m_DmaAdapter->DmaOperations->GetCommonBufferFromVectorByIndex(
            m_DmaAdapter,
            m_CommonBufferVector,
            (ULONG) Index,
            &NetDataHeader->VirtualAddress,
            (PPHYSICAL_ADDRESS) &NetDataHeader->LogicalAddress);
    }

private:

    PDMA_ADAPTER                m_DmaAdapter;
    PDMA_COMMON_BUFFER_VECTOR   m_CommonBufferVector = nullptr;
    size_t                      m_BufferOffset = 0;
};

class PAGED DmaAllocator : public IBufferVectorAllocator
{
public:

    DmaAllocator(_In_ NET_CLIENT_MEMORY_CONSTRAINTS::DMA& Dma) :
        m_DmaAdapter(reinterpret_cast<PDMA_ADAPTER>(Dma.DmaAdapter)),
        m_MaximumPhysicalAddress(Dma.MaximumPhysicalAddress),
        m_PreferredNode(Dma.PreferredNode)
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

    IBufferVector*
    AllocateVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset,
        _In_ NODE_REQUIREMENT PreferredNode)
    {
        NODE_REQUIREMENT node = (PreferredNode == MM_ANY_NODE_OK) ? m_PreferredNode : PreferredNode;

        wistd::unique_ptr<CommonBufferVector> bufferVector =
            wil::make_unique_nothrow<CommonBufferVector>(m_DmaAdapter, BufferCount, BufferSize, BufferOffset);

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

        NTSTATUS status =
            m_DmaAdapter->DmaOperations->AllocateCommonBufferVector(
                m_DmaAdapter,
                PHYSICAL_ADDRESS{ 0 },
                m_MaximumPhysicalAddress.QuadPart != 0 ? m_MaximumPhysicalAddress : PHYSICAL_ADDRESS{ 0xffffffff, -1 },
                m_CacheEnabled ? MmCached : MmNonCached,
                node,
                0,
                (ULONG) BufferCount,
                (ULONG) BufferSize,
                &bufferVector->m_CommonBufferVector);

        if (NT_SUCCESS(status))
        {
            return bufferVector.release();
        }
        else
        {
            return nullptr;
        }
    }

private:

    PDMA_ADAPTER        m_DmaAdapter;
    PHYSICAL_ADDRESS    m_MaximumPhysicalAddress;
    bool                m_CacheEnabled;
    NODE_REQUIREMENT    m_PreferredNode;
};

class PAGED NxDmaDomainAllocator : public IBufferVectorAllocator
{

};

NTSTATUS
BufferManager::AddMemoryConstraints(
    _In_ NET_CLIENT_MEMORY_CONSTRAINTS * MemoryConstraints
)
{
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !m_MemoryConstraints.append(*MemoryConstraints));

    return STATUS_SUCCESS;
}

NTSTATUS
BufferManager::InitializeBufferVectorAllocator()
{
    CX_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED,
                          m_MemoryConstraints.count() > 1);

    switch (m_MemoryConstraints[0].MappingRequirement)
    {
        case NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED:
        {
            m_BufferVectorAllocator = wil::make_unique_nothrow<DmaAllocator>(m_MemoryConstraints[0].Dma);
            break;
        }
        case NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_NONE:
        {
            m_BufferVectorAllocator = wil::make_unique_nothrow<NonPagePoolAllocator>();
            break;
        }
        default:
        {

        }
    }

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_BufferVectorAllocator.get());

    return STATUS_SUCCESS;
}

NTSTATUS
BufferManager::AllocateBufferVector(
    _In_ size_t BufferCount,
    _In_ size_t BufferSize,
    _In_ size_t BufferAlignment,
    _In_ size_t BufferAlignmentOffset,
    _In_ NODE_REQUIREMENT PreferredNode,
    _Out_ IBufferVector** BufferVector)
{
    NT_FRE_ASSERT(BufferAlignment <= PAGE_SIZE);
    NT_FRE_ASSERT(RTL_IS_POWER_OF_TWO(BufferCount));

    size_t actualBufferSize = BufferSize;
    size_t actualBufferOffset = 0;

    //
    //buffer from NetAdapter's buffer manager is always PAGE_SIZE aligned, and its
    //size is an integral multiple of PAGE_SIZE bytes, regardless of the requested
    //size
    //

    //
    //calculate any needed space at front of the buffer to satify the alignment
    //offset requirement, as well as the offset after the backfill space
    //
    if (BufferAlignmentOffset > 0)
    {
        actualBufferOffset = ALIGN_UP_BY(BufferAlignmentOffset, BufferAlignment);
        CX_RETURN_NTSTATUS_IF(STATUS_INTEGER_OVERFLOW, actualBufferOffset < BufferAlignmentOffset);

        BufferSize += actualBufferOffset - BufferAlignmentOffset;
    }

    //make sure the allocation is the multiple of PAGE_SIZE
    actualBufferSize = ALIGN_UP_BY(BufferSize, PAGE_SIZE);
    CX_RETURN_NTSTATUS_IF(STATUS_INTEGER_OVERFLOW, actualBufferSize < BufferSize);

    IBufferVector* bufferVector =
        m_BufferVectorAllocator->AllocateVector(BufferCount,
                                                actualBufferSize,
                                                actualBufferOffset,
                                                PreferredNode);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, bufferVector == nullptr);

    *BufferVector = bufferVector;

    return STATUS_SUCCESS;
}

} //namespace Core
} //namespace NetCx
