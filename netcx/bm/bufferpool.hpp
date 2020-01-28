// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "KPtr.h"
#include "KBitmap.h"
#include "kStackstorage.h"

typedef struct _NET_DATA_REF
{
    UINT32          OsRefCount;
    BOOLEAN         OsOwned;
} NET_DATA_REF;

namespace NetCx {

namespace Core {

class PAGED IBufferVector :
    public PAGED_OBJECT<'vmxn'> // 'nxmv'
{
    friend class BufferPool;

public:

    IBufferVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferOffset,
        _In_ size_t BufferCapacity)
    :   m_BufferCount(BufferCount),
        m_BufferOffset(BufferOffset),
        m_BufferCapacity(BufferCapacity) {}

    virtual
    ~IBufferVector(
        void
    ) = default;

    virtual
    void
    GetEntryByIndex(
        _In_ size_t Index,
        _Out_ NET_DATA_HEADER* NetDataHeader
    ) const = 0;

protected:

    size_t m_BufferCount = 0;
    size_t m_BufferOffset = 0;
    size_t m_BufferCapacity = 0;
};

class PAGED BufferPool :
    public KStackPool<size_t> // 'nxbp'
{

public:

    ~BufferPool(
        void
    );

    NTSTATUS
    Fill(
        _In_ IBufferVector* BufferVector
    );

    NONPAGED
    NTSTATUS
    Allocate(
        _Out_ size_t* BufferIndex
    );

    NONPAGED
    VOID
    AddRef(
        _In_ size_t BufferIndex
    );

    NONPAGED
    VOID
    DeRef(
        _In_ size_t BufferIndex
    );

    NONPAGED
    VOID
    Free(
        _In_ size_t BufferIndex
    );

    VOID
    Preview(
        _In_ size_t BufferIndex,
        _Out_ NET_DATA_HEADER* NetDataHeader
    );

    NONPAGED
    VOID
    OwnedByOs(
        _In_ size_t BufferIndex
    );

    void
    Enumerate(
        _In_ ENUMERATE_CALLBACK Callback,
        _In_ void* Context
    );

    size_t
    GetOffset();

    size_t
    GetCapacity();

    size_t
    GetPoolSize();

private:

    size_t
        m_PoolSize = 0;

    wistd::unique_ptr<IBufferVector>
        m_BufferVector;

    Rtl::KArray<NET_DATA_REF, NonPagedPoolNxCacheAligned>
        m_BuffersRef;

    Rtl::KBitmap
        m_BuffersInUseFlag;
};

} //namespace Core
} //namespace NetCx
