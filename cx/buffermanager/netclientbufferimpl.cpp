// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/


#include "BmPrecomp.hpp"
#include "BufferManager.hpp"
#include "BufferPool.hpp"
#include "KPtr.h"

#include "NetClientBufferImpl.tmh"

EXTERN_C_START

//delete buffer pool also frees the memory chunks
PAGEDX
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
NetClientDestroyBufferPool(
    _In_ NET_CLIENT_BUFFER_POOL BufferPool
    )
{
    PAGED_CODE();

    NxBufferPool* pool = reinterpret_cast<NxBufferPool *> (BufferPool);
    delete pool;
}

NONPAGEDX
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ULONG
NetClientAllocateBuffers(
    _In_ NET_CLIENT_BUFFER_POOL BufferPool,
    _Inout_updates_(NumBuffers) NET_PACKET_FRAGMENT Buffers[],
    _In_ ULONG NumBuffers)
{
    NxBufferPool* pool = reinterpret_cast<NxBufferPool *> (BufferPool);
    ULONG allocatedCount = 0;

    //CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
    //                      pool->AvailableBuffersCount() < NumBuffers);

    for (UINT32 i = 0; i < NumBuffers; i++)
    {
        size_t allocatedSize, offset;

        NTSTATUS status = pool->Allocate(&Buffers[i].VirtualAddress,
                                         &Buffers[i].Mapping.DmaLogicalAddress,
                                         &offset,
                                         &allocatedSize);

        if (!NT_SUCCESS(status))
        {
            break;
        }

        Buffers[i].Offset = offset;
        Buffers[i].Capacity = allocatedSize;

        allocatedCount++;
    }

    return allocatedCount;
}


NONPAGEDX
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
NetClientFreeBuffers(
    _In_ NET_CLIENT_BUFFER_POOL BufferPool,
    _Inout_updates_(NumBuffers) PVOID * Buffers,
    _In_ ULONG NumBuffers)
{
    NxBufferPool* pool = reinterpret_cast<NxBufferPool *> (BufferPool);

    for (UINT32 i = 0; i < NumBuffers; i++)
    {
        pool->Free(Buffers[i]);
        Buffers[i] = nullptr;
    }
}

static const NET_CLIENT_BUFFER_POOL_DISPATCH PoolDispatch =
{
    sizeof(NET_CLIENT_BUFFER_POOL_DISPATCH),
    &NetClientDestroyBufferPool,
    &NetClientAllocateBuffers,
    &NetClientFreeBuffers,
};

PAGEDX
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
NetClientCreateBufferPool(
    _In_ NET_CLIENT_BUFFER_POOL_CONFIG * BufferPoolConfig,
    _Out_ NET_CLIENT_BUFFER_POOL * BufferPool,
    _Out_ NET_CLIENT_BUFFER_POOL_DISPATCH const ** BufferPoolDispatch
    )
{
    PAGED_CODE();

    CX_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED,
                          BufferPoolConfig->Flag & NET_CLIENT_BUFFER_POOL_FLAGS_SERIALIZATION);

    CX_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER,
                          BufferPoolConfig->BufferAlignment > PAGE_SIZE);

    CX_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER,
                          !IS_POWER_OF_TWO(BufferPoolConfig->BufferAlignment));

    wistd::unique_ptr<NxBufferManager> bufferManager = wil::make_unique_nothrow<NxBufferManager>();

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !bufferManager);

    CX_RETURN_IF_NOT_NT_SUCCESS(bufferManager->AddMemoryConstraints(BufferPoolConfig->MemoryConstraints));

    CX_RETURN_IF_NOT_NT_SUCCESS(bufferManager->InitializeMemoryChunkAllocator());

    KPtr<NxBufferPool> pool;
    pool.reset(new (std::nothrow) NxBufferPool());
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !pool.get());

    size_t requestedTotalSize = 0;
    size_t minimumChunkSize = 0;
    size_t combinedAlignmentRequirement =
        max(BufferPoolConfig->MemoryConstraints->AlignmentRequirement, BufferPoolConfig->BufferAlignment);

    CX_RETURN_IF_NOT_NT_SUCCESS(pool->Initialize(BufferPoolConfig->BufferCount,
                                                 BufferPoolConfig->BufferSize,
                                                 BufferPoolConfig->BufferAlignmentOffset,
                                                 combinedAlignmentRequirement,
                                                 &requestedTotalSize,
                                                 &minimumChunkSize));

    Rtl::KArray<wistd::unique_ptr<INxMemoryChunk>> memoryChunks;
    CX_RETURN_IF_NOT_NT_SUCCESS(bufferManager->AllocateMemoryChunks(requestedTotalSize,
                                                                    minimumChunkSize,
                                                                    BufferPoolConfig->PreferredNode,
                                                                    memoryChunks));

    CX_RETURN_IF_NOT_NT_SUCCESS(pool->AddMemoryChunks(memoryChunks));

    //detach smart ptr
    *BufferPool = reinterpret_cast<NET_CLIENT_BUFFER_POOL>(pool.release());
    *BufferPoolDispatch = &PoolDispatch;

    return STATUS_SUCCESS;
}

EXTERN_C_END

