// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/

#include "bmprecomp.hpp"
#include "BufferManager.hpp"
#include "BufferPool.hpp"
#include "BufferPool.tmh"

class RtlMdl
{
public:

    static RtlMdl* CreateMdl(_In_ PVOID VirtualAddress,
                          _In_ size_t Length,
                          _In_ bool toBuildMdl)
    {
        RtlMdl* mdl = new (std::nothrow, BUFFER_MANAGER_POOL_TAG) RtlMdl();

        if (!NT_SUCCESS(mdl->Allocate(VirtualAddress, Length, toBuildMdl)))
        {
            delete mdl;
            mdl = nullptr;
        }

        return mdl;
    }

    ~RtlMdl()
    {
        if (m_Mdl)
        {
            IoFreeMdl(m_Mdl);
            m_Mdl = nullptr;
        }

        m_PhysicalPageCount = 0;
        m_AllocatedMdlSize = 0;
    }

    size_t GetByteCount() { return MmGetMdlByteCount(m_Mdl); }
    size_t GetPhysicalPageCount() { return m_PhysicalPageCount; }
    size_t GetByteOffset() { return MmGetMdlByteOffset(m_Mdl); }
    PPFN_NUMBER GetPhysicalPages() { return MmGetMdlPfnArray(m_Mdl); }
    PMDL GetMdl() { return m_Mdl; }

private:

    RtlMdl() {}

    NTSTATUS
    Allocate(_In_ PVOID VirtualAddress,
             _In_ size_t Length,
             _In_ bool toBuildMdl)
    {
        ULONG ulength;
        CX_RETURN_IF_NOT_NT_SUCCESS(RtlSizeTToULong(Length, &ulength));
        m_Mdl = IoAllocateMdl(VirtualAddress, ulength, FALSE, FALSE, nullptr);
        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_Mdl);

        m_AllocatedMdlSize = MmSizeOfMdl(VirtualAddress, Length);
        m_PhysicalPageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, Length);

        if (toBuildMdl)
        {
            MmBuildMdlForNonPagedPool(m_Mdl);
        }

        return STATUS_SUCCESS;
    }

    size_t m_PhysicalPageCount = 0;
    size_t m_AllocatedMdlSize = 0;
    PMDL m_Mdl = nullptr;
};

NxBufferPool::NxBufferPool()
{}

NxBufferPool::~NxBufferPool()
{
    if (m_StitchedMdl)
    {
        MmUnmapReservedMapping(m_BaseVirtualAddress,
                               BUFFER_MANAGER_POOL_TAG,
                               m_StitchedMdl->GetMdl());

        MmFreeMappingAddress(m_BaseVirtualAddress,
                             BUFFER_MANAGER_POOL_TAG);
    }

    NT_ASSERT(m_NumBuffersInUse == 0);
}

NTSTATUS
NxBufferPool::Initialize(
    _In_ size_t  PoolSize,
    _In_ size_t AllocateSize,
    _In_ size_t AlignmentOffset,
    _In_ size_t Alignment,
    _Out_ size_t* TotalSizeRequested,
    _Out_ size_t* MinimumChunkSize)
{
    NT_ASSERT(Alignment <= PAGE_SIZE);

    //store the alignment value
    m_AlignmentOffset = AlignmentOffset;
    m_Alignment = Alignment;
    //store total pool size
    m_RequestedPoolSize = PoolSize;

    //1. calculate the total size of one buffer element (including the alignment)
    m_StrideSize = ALIGN_UP_BY(AllocateSize, Alignment);
    CX_RETURN_NTSTATUS_IF(STATUS_INTEGER_OVERFLOW, m_StrideSize < AllocateSize);


    //2. calculate the total size of memory needed to populate the entire pool
    //Buffer Manage only allows alignment value < PAGE_SIZE and grantuates the memory chunck it
    //allocated is always page algined
    size_t requstedTotalSize = 0;

    //2.a calculate the necessary memory size to store all buffers
    CX_RETURN_IF_NOT_NT_SUCCESS(RtlSizeTMult(m_RequestedPoolSize, m_StrideSize, &requstedTotalSize));


    //2.b calculate the offset needed at the starting of the memory chunk for alignment
    size_t chunkOffsetAligned;
    chunkOffsetAligned = ALIGN_UP_BY(m_AlignmentOffset, m_Alignment);
    CX_RETURN_NTSTATUS_IF(STATUS_INTEGER_OVERFLOW, chunkOffsetAligned < m_AlignmentOffset);
    m_ChunkOffset = chunkOffsetAligned - m_AlignmentOffset;

    //2.c calculate the total size
    CX_RETURN_IF_NOT_NT_SUCCESS(RtlSizeTAdd(requstedTotalSize, m_ChunkOffset, &requstedTotalSize));
    //round-up to multiple of pages
    m_RequestedTotalSize = ALIGN_UP_BY(requstedTotalSize, PAGE_SIZE);
    CX_RETURN_NTSTATUS_IF(STATUS_INTEGER_OVERFLOW, m_RequestedTotalSize < requstedTotalSize);
    *TotalSizeRequested = m_RequestedTotalSize;

    //a memory chunk cannot be smaller than the size required for a single buffer + offset needed
    //for alignment
    *MinimumChunkSize = m_StrideSize + m_ChunkOffset;

    return STATUS_SUCCESS;
}

NONPAGED
size_t
NxBufferPool::AvailableBuffersCount()
{
    return m_PopulatedPoolSize - m_NumBuffersInUse;
}

NTSTATUS
NxBufferPool::StitchMemoryChunks()
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_MemoryChunkBaseAddresses.reserve(m_NumMemoryChunks));

    CX_RETURN_IF_NOT_NT_SUCCESS(RtlSizeTMult(m_MemoryChunkSize,
                                             m_NumMemoryChunks,
                                             &m_ContiguousVirtualLength));

    if (m_NumMemoryChunks == 1)
    {
        NxChunkBaseAddress baseAddress = {
            m_MemoryChunks[0]->GetVirtualAddress(),
            m_MemoryChunks[0]->GetLogicalAddress()
        };

        NT_FRE_ASSERT(m_MemoryChunkBaseAddresses.append(baseAddress));

        m_BaseVirtualAddress = m_MemoryChunks[0]->GetVirtualAddress();
    }
    else
    {
        m_BaseVirtualAddress = MmAllocateMappingAddress(m_ContiguousVirtualLength,
                                                        BUFFER_MANAGER_POOL_TAG);

        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                              !m_BaseVirtualAddress);

        m_StitchedMdl.reset(RtlMdl::CreateMdl(m_BaseVirtualAddress, m_ContiguousVirtualLength, false));
        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                              !m_StitchedMdl);

        size_t stitchedMdlPhysicalPageIndex = 0;

        for (size_t i = 0; i < m_NumMemoryChunks; i++)
        {
            NxChunkBaseAddress baseAddress = {
                (PVOID) ((ULONG_PTR) m_BaseVirtualAddress + i * m_MemoryChunkSize),
                m_MemoryChunks[i]->GetLogicalAddress()
            };

            NT_FRE_ASSERT(m_MemoryChunkBaseAddresses.append(baseAddress));

            wistd::unique_ptr<RtlMdl> chunkMdl;
            chunkMdl.reset(RtlMdl::CreateMdl(m_MemoryChunks[i]->GetVirtualAddress(),
                                             m_MemoryChunkSize,
                                             true));
            CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                                  !chunkMdl);

            NT_FRE_ASSERT(m_StitchedMdl->GetPhysicalPageCount() ==
                          chunkMdl->GetPhysicalPageCount() * m_NumMemoryChunks);

            RtlCopyMemory(&m_StitchedMdl->GetPhysicalPages()[stitchedMdlPhysicalPageIndex],
                          chunkMdl->GetPhysicalPages(),
                          chunkMdl->GetPhysicalPageCount() * sizeof(PFN_NUMBER));

            stitchedMdlPhysicalPageIndex += chunkMdl->GetPhysicalPageCount();
        }

        NT_FRE_ASSERT(stitchedMdlPhysicalPageIndex == m_StitchedMdl->GetPhysicalPageCount());

        PVOID mappedVa =
            MmMapLockedPagesWithReservedMapping(m_BaseVirtualAddress,
                                                BUFFER_MANAGER_POOL_TAG,
                                                m_StitchedMdl->GetMdl(),
                                                MmCached);

        NT_FRE_ASSERT(mappedVa == m_BaseVirtualAddress);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NxBufferPool::AddMemoryChunks(
    _Inout_ Rtl::KArray<wistd::unique_ptr<INxMemoryChunk>>& MemoryChunks)
{
    //
    // buffer manager guaruntees all memory chunks it allocated meets
    // the requirements, here to assert these conditions
    //
    NT_FRE_ASSERT(m_MemoryChunks.count() == 0);
    NT_FRE_ASSERT(MemoryChunks.count() > 0);

    const size_t numMemoryChunks = MemoryChunks.count();
    const size_t memoryChunkSize = MemoryChunks[0]->GetLength();

    for (size_t i = 0; i < MemoryChunks.count(); i++)
    {
        // all chunks must have the same size
        NT_FRE_ASSERT(MemoryChunks[i]->GetLength() == memoryChunkSize);
        NT_FRE_ASSERT(IS_ALIGNED((size_t) MemoryChunks[i]->GetVirtualAddress(), PAGE_SIZE));
    }

    NT_FRE_ASSERT(memoryChunkSize >= m_ChunkOffset + m_StrideSize);

    const size_t numBuffersPerChunk = (memoryChunkSize - m_ChunkOffset) / m_StrideSize;
    const size_t newPoolSize = numBuffersPerChunk * numMemoryChunks;

    NT_FRE_ASSERT(newPoolSize >= m_RequestedPoolSize);

    //
    // now transfer the ownership of those memory chunks to buffer pool
    //
    m_NumMemoryChunks = numMemoryChunks;
    m_MemoryChunkSize = memoryChunkSize;
    m_NumBuffersPerChunk = numBuffersPerChunk;

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !m_Buffers.reserve(newPoolSize));

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !m_BuffersInUseFlag.Initialize(newPoolSize));

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !m_MemoryChunks.reserve(m_NumMemoryChunks));

    m_MemoryChunks = wistd::move(MemoryChunks);

    CX_RETURN_IF_NOT_NT_SUCCESS(StitchMemoryChunks());

    FillBufferPool();

    NT_FRE_ASSERT(m_PopulatedPoolSize == newPoolSize);

    return STATUS_SUCCESS;
}

void
NxBufferPool::FillBufferPool()
{
    for (size_t chunkIndex = 0; chunkIndex < m_NumMemoryChunks; chunkIndex++)
    {
        for (size_t bufferIndex = 0; bufferIndex < m_NumBuffersPerChunk; bufferIndex++)
        {
            const size_t bufferOffset = m_ChunkOffset + bufferIndex * m_StrideSize;

            NxBufferDescriptor buffer =
            {
                (PVOID) (((ULONG_PTR) m_MemoryChunkBaseAddresses[chunkIndex].VirtualAddress) + bufferOffset),
                0ULL,
                chunkIndex,
                m_PopulatedPoolSize,
                nullptr
            };

            buffer.LogicalAddress =
                m_MemoryChunkBaseAddresses[chunkIndex].LogicalAddress + bufferOffset;

            m_Buffers.append(buffer);

            m_PopulatedPoolSize++;
        }
    }
}

NONPAGED
NTSTATUS
NxBufferPool::Allocate(_Out_ PVOID* VirtualAddress,
                       _Out_ LOGICAL_ADDRESS * LogicalAddress,
                       _Out_ size_t* Offset,
                       _Out_ size_t* AllocatedSize)
{
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          m_NumBuffersInUse >= m_PopulatedPoolSize);

    size_t bufferIndex = m_Buffers[m_NumBuffersInUse].BufferIndex;

    NT_FRE_ASSERT(!m_BuffersInUseFlag.TestBit(bufferIndex));
    m_BuffersInUseFlag.SetBit(bufferIndex);

    *VirtualAddress = m_Buffers[m_NumBuffersInUse].VirtualAddress;
    *LogicalAddress = m_Buffers[m_NumBuffersInUse].LogicalAddress;
    *Offset = m_AlignmentOffset;
    *AllocatedSize = m_StrideSize;

    m_NumBuffersInUse++;

    return STATUS_SUCCESS;
}

NONPAGED
VOID
NxBufferPool::Free(_In_ PVOID VirtualAddress)
{





    NT_FRE_ASSERT(m_NumBuffersInUse > 0);

    NT_FRE_ASSERT(VirtualAddress >= m_BaseVirtualAddress);

    size_t offsetFromBaseVa = ((size_t) VirtualAddress) - ((size_t) m_BaseVirtualAddress);

    NT_FRE_ASSERT(offsetFromBaseVa < m_ContiguousVirtualLength);

    auto &buffer = m_Buffers[--m_NumBuffersInUse];

    buffer.VirtualAddress = VirtualAddress;
    buffer.ChunkIndex = offsetFromBaseVa / m_MemoryChunkSize;

    size_t offsetFromChunk = offsetFromBaseVa - (buffer.ChunkIndex * m_MemoryChunkSize);

    buffer.LogicalAddress =
        m_MemoryChunkBaseAddresses[buffer.ChunkIndex].LogicalAddress + offsetFromChunk;

    buffer.BufferIndex =
        buffer.ChunkIndex * m_NumBuffersPerChunk +
        (offsetFromChunk - m_ChunkOffset) / m_StrideSize;


    NT_FRE_ASSERT(m_BuffersInUseFlag.TestBit(buffer.BufferIndex));
    m_BuffersInUseFlag.ClearBit(buffer.BufferIndex);
}

