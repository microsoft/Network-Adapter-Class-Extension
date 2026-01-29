// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "KPtr.h"
#include "KBitmap.h"
#include "kStackstorage.h"
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
    BufferPool(
        _In_ PEPROCESS AssociatedProcess
    );

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

    PEPROCESS
    GetAssociatedProcess(
        void
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

    PEPROCESS
        m_associatedProcess;
};


