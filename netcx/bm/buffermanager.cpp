// Copyright (C) Microsoft Corporation. All rights reserved.

#include "bmprecomp.hpp"

#include "BufferPool.hpp"
#include "BufferManager.hpp"
#include "DmaAllocator.hpp"
#include "NonPagedAllocator.hpp"
#include "BufferManager.tmh"

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
            m_BufferVectorAllocator = wil::make_unique_nothrow<NonPagedAllocator>();
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
        actualBufferOffset = RTL_NUM_ALIGN_UP(BufferAlignmentOffset, BufferAlignment);
        CX_RETURN_NTSTATUS_IF(STATUS_INTEGER_OVERFLOW, actualBufferOffset < BufferAlignmentOffset);

        BufferSize += actualBufferOffset - BufferAlignmentOffset;
    }

    //make sure the allocation is the multiple of PAGE_SIZE
    actualBufferSize = RTL_NUM_ALIGN_UP(BufferSize, PAGE_SIZE);
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
