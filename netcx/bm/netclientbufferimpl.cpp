// Copyright (C) Microsoft Corporation. All rights reserved.

#include "BmPrecomp.hpp"
#include "BufferPool.hpp"
#include "BufferManager.hpp"
#include "KPtr.h"

#include <net/fragment.h>

#include "NetClientBufferImpl.tmh"

EXTERN_C_START

//delete buffer pool also frees the memory chunks
PAGEDX
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
NetClientDestroyBufferPool(
    _In_ NET_CLIENT_BUFFER_POOL Pool
)
{
    PAGED_CODE();

    BufferPool* pool
        = reinterpret_cast<BufferPool *> (Pool);

    delete pool;
}

NONPAGEDX
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
NetClientEnumerateBuffers(
    _In_ NET_CLIENT_BUFFER_POOL Pool,
    _In_ ENUMERATE_CALLBACK Callback,
    _In_ PVOID Context
)
{
    auto pool = reinterpret_cast<BufferPool*>(Pool);
    pool->Enumerate(Callback, Context);
}

NONPAGEDX
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NTSTATUS
NetClientAllocateBuffer(
    _In_ NET_CLIENT_BUFFER_POOL Pool,
    _Out_ SIZE_T* Index,
    _Out_ NET_DATA_HEADER* NetDataHeader,
    _Out_ SIZE_T * NetDataOffset
)
{
    SIZE_T index = 0;
    size_t offset = 0;

    auto pool = reinterpret_cast<BufferPool *>(Pool);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        pool->Allocate(&index));

    pool->AddRef(index);

    pool->OwnedByOs(index);
    
    pool->Preview(index,
                  NetDataHeader);

    *Index = index;
    *NetDataOffset = offset;

    return STATUS_SUCCESS;
}


NONPAGEDX
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
NetClientFreeBuffers(
    _In_ NET_CLIENT_BUFFER_POOL Pool,
    _Inout_updates_(NumBuffers) SIZE_T * Buffers,
    _In_ ULONG NumBuffers)
{
    BufferPool* pool
        = reinterpret_cast<BufferPool *> (Pool);

    for (UINT32 i = 0; i < NumBuffers; i++)
    {
        pool->DeRef(Buffers[i]);
    }
}

static const NET_CLIENT_BUFFER_POOL_DISPATCH PoolDispatch =
{
    sizeof(NET_CLIENT_BUFFER_POOL_DISPATCH),
    &NetClientDestroyBufferPool,
    &NetClientEnumerateBuffers,
    &NetClientAllocateBuffer,
    &NetClientFreeBuffers
};

PAGEDX
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
NetClientCreateBufferPool(
    _In_ NET_CLIENT_BUFFER_POOL_CONFIG* BufferPoolConfig,
    _Out_ NET_CLIENT_BUFFER_POOL* Pool,
    _Out_ NET_CLIENT_BUFFER_POOL_DISPATCH const** BufferPoolDispatch
)
{
    PAGED_CODE();

    CX_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED,
                          BufferPoolConfig->Flag & NET_CLIENT_BUFFER_POOL_FLAGS_SERIALIZATION);

    CX_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER,
                          BufferPoolConfig->BufferAlignment > PAGE_SIZE);

    if (BufferPoolConfig->BufferAlignment > 0)
    {
        CX_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER,
                            !RTL_IS_POWER_OF_TWO(BufferPoolConfig->BufferAlignment));
    }

    wistd::unique_ptr<BufferManager>
        bufferManager = wil::make_unique_nothrow<BufferManager>();

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !bufferManager);

    wistd::unique_ptr<BufferPool> pool
        = wil::make_unique_nothrow<BufferPool>();

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !pool);

    CX_RETURN_IF_NOT_NT_SUCCESS(bufferManager->AddMemoryConstraints(BufferPoolConfig->MemoryConstraints));

    CX_RETURN_IF_NOT_NT_SUCCESS(bufferManager->InitializeBufferVectorAllocator());

    size_t combinedAlignmentRequirement = max(BufferPoolConfig->MemoryConstraints->AlignmentRequirement,
                                              BufferPoolConfig->BufferAlignment);

    IBufferVector* bufferVector = nullptr;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        bufferManager->AllocateBufferVector(
            BufferPoolConfig->BufferCount,
            BufferPoolConfig->BufferSize,
            combinedAlignmentRequirement,
            BufferPoolConfig->BufferAlignmentOffset,
            BufferPoolConfig->PreferredNode,
            &bufferVector));

    CX_RETURN_IF_NOT_NT_SUCCESS(pool->Fill(bufferVector));

    //detach smart ptr
    *Pool = reinterpret_cast<NET_CLIENT_BUFFER_POOL>(pool.release());
    *BufferPoolDispatch = &PoolDispatch;

    return STATUS_SUCCESS;
}

EXTERN_C_END
