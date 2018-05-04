// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/

#include "bmprecomp.hpp"

#include "BufferManager.hpp"
#include "BufferManager.tmh"
#include "KRegKey.h"

#define NUM_MEMORY_CHUNK_TEST_REG_PATH L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\NDIS\\Parameters" 
#define NUM_MEMORY_CHUNK_TEST_REG_VALUE L"NumMemoryChunk"

namespace {

    VOID
    LoadValueFromTestHookIfPresent(
        _Out_ size_t* numMemoryChunks)
    {
        KRegKey key;
        NTSTATUS status = key.Open(KEY_QUERY_VALUE, NUM_MEMORY_CHUNK_TEST_REG_PATH);

        if (NT_SUCCESS(status))
        {
            ULONG value;
            status = key.QueryValueUlong(NUM_MEMORY_CHUNK_TEST_REG_VALUE, &value);

            if (NT_SUCCESS(status))
            {
                *numMemoryChunks = value;
            }
        }
    }
}

class PAGED NxPoolMemoryChunk : public INxMemoryChunk
{
    friend class NxNonPagePoolAllocator;

public:

    NxPoolMemoryChunk() {}

    ~NxPoolMemoryChunk()
    {
        if (m_VirtualAddress)
        {
            ExFreePool(m_VirtualAddress);
            m_VirtualAddress = nullptr;
        }
    }
    size_t GetLength() const override
    {
        return m_Length;
    }

    PVOID GetVirtualAddress() const override
    {
        return m_VirtualAddress;
    }

    PHYSICAL_ADDRESS GetLogicalAddress() const override
    {
        return { 0, 0 };
    }

private:

    PVOID   m_VirtualAddress = nullptr;
    size_t  m_Length = 0;
};

#ifdef _KERNEL_MODE

class PAGED NxCommonBufferMemoryChunk : public INxMemoryChunk
{
    friend class NxDmaAllocator;

public:

    NxCommonBufferMemoryChunk(_In_ PDMA_ADAPTER DmaAdapter)
        : m_DmaAdapter(DmaAdapter) {}

    ~NxCommonBufferMemoryChunk()
    {
        // Free the common buffer if one was allocated
        if (m_VirtualAddress != nullptr)
        {
            NT_ASSERT(m_LogicalAddress.QuadPart != 0);

            m_DmaAdapter->DmaOperations->FreeCommonBuffer(
                m_DmaAdapter,
                m_AllocatedLength,
                m_LogicalAddress,
                m_VirtualAddress,
                m_CacheEnabled);

            m_VirtualAddress = nullptr;
            m_LogicalAddress.QuadPart = 0;
        }
    }

    size_t GetLength() const override
    {
        return m_Length;
    }

    PVOID GetVirtualAddress() const override
    {
        return m_VirtualAddress;
    }

    PHYSICAL_ADDRESS GetLogicalAddress() const override
    {
        return m_LogicalAddress;
    }

private:

    PVOID               m_VirtualAddress = nullptr;
    PHYSICAL_ADDRESS    m_LogicalAddress = { 0 };
    size_t              m_Length = 0;
    ULONG               m_AllocatedLength = 0;
    bool                m_CacheEnabled;
    PDMA_ADAPTER        m_DmaAdapter;
};

class PAGED NxDmaAllocator : public INxMemoryChunkAllocator
{
public:

    NxDmaAllocator(_In_ NET_CLIENT_MEMORY_CONSTRAINTS::DMA& Dma)
        : m_DmaAdapter(reinterpret_cast<PDMA_ADAPTER>(Dma.DmaAdapter)),
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

    INxMemoryChunk* 
    AllocateMemoryChunk(
        _In_ size_t Size,
        _In_ NODE_REQUIREMENT PreferredNode)
    {
        NODE_REQUIREMENT node = (PreferredNode == MM_ANY_NODE_OK) ? m_PreferredNode : PreferredNode;
        size_t allocationSize = 0;

        wistd::unique_ptr<NxCommonBufferMemoryChunk> memoryChunk =
            wil::make_unique_nothrow<NxCommonBufferMemoryChunk>(m_DmaAdapter);

        if (!memoryChunk.get())
        {
            return nullptr;
        }

        allocationSize = ALIGN_UP_BY(Size, PAGE_SIZE);

        if (allocationSize < Size)
        {
            return nullptr;
        }

        if (!NT_SUCCESS(RtlSizeTToULong(allocationSize, &memoryChunk->m_AllocatedLength)))
        {
            return nullptr;
        }











        memoryChunk->m_VirtualAddress =
            m_DmaAdapter->DmaOperations->AllocateCommonBufferEx(
                m_DmaAdapter,
                m_MaximumPhysicalAddress.QuadPart != 0 ? &m_MaximumPhysicalAddress : nullptr,
                memoryChunk->m_AllocatedLength,
                &memoryChunk->m_LogicalAddress,
                m_CacheEnabled,
                node);

        if (memoryChunk->m_VirtualAddress == nullptr)
        {
            return nullptr;
        }
        else
        {
            memoryChunk->m_CacheEnabled = m_CacheEnabled;
            memoryChunk->m_Length = Size;
            return memoryChunk.release();
        }
    }

private:

    PDMA_ADAPTER        m_DmaAdapter;
    PHYSICAL_ADDRESS    m_MaximumPhysicalAddress;
    bool                m_CacheEnabled;
    NODE_REQUIREMENT    m_PreferredNode;
};

#endif //_KERNEL_MODE

class PAGED NxNonPagePoolAllocator : public INxMemoryChunkAllocator
{
public:

    INxMemoryChunk* 
    AllocateMemoryChunk(
        _In_ size_t Size,
        _In_ NODE_REQUIREMENT PreferredNode)
    {
        size_t allocateSize = 0;
        UNREFERENCED_PARAMETER(PreferredNode);

        NT_ASSERT(Size != 0);

        allocateSize = ALIGN_UP_BY(Size, PAGE_SIZE);
        if (allocateSize < Size)
        {
            return nullptr;
        }

        wistd::unique_ptr<NxPoolMemoryChunk> memoryChunk =
            wil::make_unique_nothrow<NxPoolMemoryChunk>();

        memoryChunk->m_VirtualAddress = ExAllocatePoolWithTag(NonPagedPoolNx, allocateSize, BUFFER_MANAGER_POOL_TAG);

        if (memoryChunk->m_VirtualAddress == nullptr)
        {
            return nullptr;
        }
        else
        {
            memoryChunk->m_Length = Size;
            return memoryChunk.release();
        }
    }
};

class PAGED NxDmaDomainAllocator : public INxMemoryChunkAllocator
{

};

NxBufferManager::NxBufferManager()
{}

NxBufferManager::~NxBufferManager()
{}

NTSTATUS
NxBufferManager::AddMemoryConstraints(
    _In_ NET_CLIENT_MEMORY_CONSTRAINTS * MemoryConstraints
    )
{
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !m_MemoryConstraints.append(*MemoryConstraints));

    return STATUS_SUCCESS;
}

NTSTATUS
NxBufferManager::InitializeMemoryChunkAllocator()
{
    CX_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED,
                          m_MemoryConstraints.count() > 1);

    switch (m_MemoryConstraints[0].MappingRequirement)
    {
        case NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED:
        {
#ifdef _KERNEL_MODE
            m_MemoryChunkAllocator.reset(new (std::nothrow) NxDmaAllocator(m_MemoryConstraints[0].Dma));
#endif //_KERNEL_MODE
            break;
        }
        case NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_NONE:
        {
            m_MemoryChunkAllocator.reset(new (std::nothrow) NxNonPagePoolAllocator());
            break;
        }
        default:
        {

        }
    }

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_MemoryChunkAllocator.get());

    return STATUS_SUCCESS;
}

NTSTATUS
NxBufferManager::AllocateMemoryChunks(
    _In_ size_t TotalRequestedSize,
    _In_ size_t MinimumChunkSize,
    _In_ NODE_REQUIREMENT PreferredNode,
    _Inout_ Rtl::KArray<wistd::unique_ptr<INxMemoryChunk>>& MemoryChunks)
{
    // Try to allocate 1 big allocation. If that fails, continue to split the size of the
    // requested allocations in half until the allocation requests can be satisfied.
    //
    // This won't allocate less memory, but it will split the allocation up so that if room
    // for the initial allocation can't be found, we can get a bunch of smaller allocations
    // instead which are easier to find room for
    size_t numMemoryChunks = 1;

    LoadValueFromTestHookIfPresent(&numMemoryChunks);

    size_t chunkSize = TotalRequestedSize / numMemoryChunks;

    while (MinimumChunkSize < chunkSize)
    {
        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            !MemoryChunks.reserve(numMemoryChunks));

        for (size_t i = 0; i < numMemoryChunks; i++)
        {
            // memory has already been pre-allocated - there's
            // no reason this append call should fail.
            NT_FRE_ASSERT(MemoryChunks.append(wistd::unique_ptr<INxMemoryChunk>(
                m_MemoryChunkAllocator->AllocateMemoryChunk(chunkSize, PreferredNode))));

            if (!MemoryChunks[i])
            {
                //cannot find a large enough memory chunk
                //start all over again with x2 memory chunks
                MemoryChunks.clear();
                numMemoryChunks <<= 1;
                chunkSize >>= 1;
                break;
            }
        }

        if (MemoryChunks.count() == numMemoryChunks)
        {
            break;
        }
    }

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        numMemoryChunks != MemoryChunks.count());

    return STATUS_SUCCESS;
}

