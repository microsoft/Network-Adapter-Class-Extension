// Copyright (C) Microsoft Corporation. All rights reserved.

#include "bmprecomp.hpp"
#include "BufferPool.hpp"
#include "BufferPool.tmh"

BufferPool::~BufferPool()
{
    NT_FRE_ASSERT(IsFull());
}

NTSTATUS
BufferPool::Fill(
    _In_ IBufferVector* BufferVector
)
{    
    m_BufferVector.reset(BufferVector);

    m_PoolSize = m_BufferVector->m_BufferCount;

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !m_BuffersRef.resize(m_PoolSize));

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !m_BuffersInUseFlag.Initialize(m_PoolSize));

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !resize(m_PoolSize));

    for (size_t i = 0; i < m_PoolSize; i++)
    {
        PushToReturn(i);
        m_BuffersRef[i].OsOwned = false;
        m_BuffersRef[i].OsRefCount = 0;
    }

    return STATUS_SUCCESS;
}

NONPAGED
VOID
BufferPool::Preview(
    _In_ size_t BufferIndex,
    _Out_ NET_DATA_HEADER* NetDataHeader)
{
    NT_FRE_ASSERT(BufferIndex < m_PoolSize);

    m_BufferVector->GetEntryByIndex(BufferIndex, NetDataHeader);
}

void
BufferPool::Enumerate(
    _In_ ENUMERATE_CALLBACK Callback,
    _In_ void* Context
)
{
    for (size_t i = 0; i < m_PoolSize; i++)
    {
        NET_DATA_HEADER NetDataHeader;
        m_BufferVector->GetEntryByIndex(i, &NetDataHeader);

        Callback(Context, i, NetDataHeader.LogicalAddress, NetDataHeader.VirtualAddress);
    }
}

NONPAGED
NTSTATUS
BufferPool::Allocate(_Out_ SIZE_T * BufferIndex)
{
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, IsEmpty());

    size_t bufferIndex = PopToUse();

    NT_FRE_ASSERT(!m_BuffersInUseFlag.TestBit(bufferIndex));
    NT_FRE_ASSERT(!m_BuffersRef[bufferIndex].OsOwned);
    NT_FRE_ASSERT(m_BuffersRef[bufferIndex].OsRefCount == 0);

    m_BuffersInUseFlag.SetBit(bufferIndex);

    *BufferIndex = bufferIndex;

    return STATUS_SUCCESS;
}

NONPAGED
VOID
BufferPool::AddRef(_In_ size_t BufferIndex)
{
    NT_FRE_ASSERT(BufferIndex < m_PoolSize);
    // at one buffer must already been allocated, the pool cannot be full
    NT_FRE_ASSERT(!IsFull());
    NT_FRE_ASSERT(m_BuffersInUseFlag.TestBit(BufferIndex));

    m_BuffersRef[BufferIndex].OsRefCount++;
}

NONPAGED
VOID
BufferPool::DeRef(_In_ size_t BufferIndex)
{
    NT_FRE_ASSERT(BufferIndex < m_PoolSize);
    NT_FRE_ASSERT(!IsFull());
    NT_FRE_ASSERT(m_BuffersInUseFlag.TestBit(BufferIndex));

    --m_BuffersRef[BufferIndex].OsRefCount;

    if ((m_BuffersRef[BufferIndex].OsRefCount == 0) &&
        (m_BuffersRef[BufferIndex].OsOwned))
    {
        Free(BufferIndex);
    }
}

NONPAGED
VOID
BufferPool::Free(_In_ size_t BufferIndex)
{
    NT_FRE_ASSERT(BufferIndex < m_PoolSize);
    NT_FRE_ASSERT(!IsFull());
    NT_FRE_ASSERT(m_BuffersInUseFlag.TestBit(BufferIndex));
    NT_FRE_ASSERT(m_BuffersRef[BufferIndex].OsRefCount == 0);
    NT_FRE_ASSERT(m_BuffersRef[BufferIndex].OsOwned);

    PushToReturn(BufferIndex);
    m_BuffersInUseFlag.ClearBit(BufferIndex);
    m_BuffersRef[BufferIndex].OsOwned = false;
}

NONPAGED
VOID
BufferPool::OwnedByOs(
    _In_ size_t BufferIndex
)
{
    m_BuffersRef[BufferIndex].OsOwned = true;
}

inline
size_t
BufferPool::GetOffset()
{
    return m_BufferVector->m_BufferOffset;
}

inline
size_t
BufferPool::GetCapacity()
{
    return m_BufferVector->m_BufferCapacity;
}

inline
size_t
BufferPool::GetPoolSize()
{
    return m_PoolSize;
}

_Use_decl_annotations_
BufferPool2::BufferPool2(
    size_t BufferSize,
    size_t BufferAlignment
)
    : m_bufferSize(BufferSize)
    , m_bufferAlignment(BufferAlignment)
{
    InitializeListHead(&m_freeBufferList);
    InitializeListHead(&m_usedBufferList);

    NT_FRE_ASSERT(m_bufferAlignment > 0);
    NT_FRE_ASSERT((m_bufferAlignment & (m_bufferAlignment - 1)) == 0);
    NT_FRE_ASSERT(m_bufferAlignment <= PAGE_SIZE);
}

_Use_decl_annotations_
BufferPool2::~BufferPool2(
    void
)
{
    NT_FRE_ASSERTMSG(
        "Buffer leak detected",
        IsListEmpty(&m_usedBufferList));
}

_Use_decl_annotations_
NTSTATUS
BufferPool2::Initialize(
    IBufferVectorAllocator * Allocator,
    size_t InitialNumberOfBuffers
)
{
    // Ensure user buffer start address is correctly aligned
    size_t const headerSize = RTL_NUM_ALIGN_UP(sizeof(BufferHeader), m_bufferAlignment);

    // Always allocate a multiple of PAGE_SIZE, such that if we have to map this memory to user
    // mode address space we don't accidentally expose unrelated kernel data. This has the side
    // effect of making the largest possible alignment a client can request be PAGE_SIZE
    size_t const allocationSize = RTL_NUM_ALIGN_UP(headerSize + m_bufferSize, PAGE_SIZE);

    // Calculate how much space will be left unused at the end of the buffer
    size_t const bufferPaddingOffset = headerSize + m_bufferSize;
    size_t const bufferPaddingSize = allocationSize - bufferPaddingOffset;

    wistd::unique_ptr<IBufferVector> bufferVector
    {
        Allocator->AllocateVector(
            InitialNumberOfBuffers,
            allocationSize,
            0,
            MM_ANY_NODE_OK)
    };

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !bufferVector);

    for (size_t i = 0; i < InitialNumberOfBuffers; i++)
    {
        NET_DATA_HEADER header;
        bufferVector->GetEntryByIndex(i, &header);

        auto allocation = new (header.VirtualAddress) BufferHeader(
            &m_freeBufferList,
            &m_usedBufferList,
            header.LogicalAddress,
            m_bufferSize,
            headerSize,
            bufferPaddingSize);

        CX_RETURN_IF_NOT_NT_SUCCESS(allocation->Initialize());
    }

    m_bufferVector = wistd::move(bufferVector);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
BufferHeader *
BufferPool2::Allocate(
    void
)
{
    if (IsListEmpty(&m_freeBufferList))
    {
        return nullptr;
    }

    auto buffer = BufferHeader::FromListEntry(
        RemoveHeadList(&m_freeBufferList));

    buffer->MarkAsUsed();

    return buffer;
}
