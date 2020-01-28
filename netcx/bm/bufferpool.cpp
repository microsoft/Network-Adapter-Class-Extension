// Copyright (C) Microsoft Corporation. All rights reserved.

#include "bmprecomp.hpp"
#include "BufferPool.hpp"
#include "BufferPool.tmh"

namespace NetCx {

namespace Core {

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
BufferPool::Allocate(_Out_ size_t* BufferIndex)
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

} //namespace Core
} //namespace NetCx

