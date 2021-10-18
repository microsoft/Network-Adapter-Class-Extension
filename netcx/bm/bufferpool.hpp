// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "KPtr.h"
#include "KBitmap.h"
#include "kStackstorage.h"
#include "BufferHeader.hpp"
#include "IBufferVector.hpp"
#include "IBufferVectorAllocator.hpp"

typedef struct _NET_DATA_REF
{
    UINT32          OsRefCount;
    BOOLEAN         OsOwned;
} NET_DATA_REF;

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
        _Out_ SIZE_T * BufferIndex
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

    NONPAGED
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

class BufferPool2 : public NONPAGED_OBJECT<'2lPB'>
{
#ifdef TEST_HARNESS_CLASS_NAME
    friend class
        TEST_HARNESS_CLASS_NAME;
#endif

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    BufferPool2(
        _In_ size_t BufferSize,
        _In_ size_t BufferAlignment
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ~BufferPool2(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        _In_ IBufferVectorAllocator * Allocator,
        _In_ size_t InitialNumberOfBuffers
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    BufferHeader *
    Allocate(
        void
    );

private:

    wistd::unique_ptr<IBufferVector>
        m_bufferVector;

    size_t const
        m_bufferSize;

    size_t const
        m_bufferAlignment;

    LIST_ENTRY
        m_freeBufferList;

    LIST_ENTRY
        m_usedBufferList;
};
