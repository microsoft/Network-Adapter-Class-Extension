// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxBufferPool object.

--*/

#pragma once

class NxBufferPool;

typedef UINT64 LOGICAL_ADDRESS;

class PAGED INxMemoryChunk :
    public PAGED_OBJECT<'cmxn'> // 'nxmc'
{

    friend class INxMemoryChunkAllocator;

public:

    virtual
    ~INxMemoryChunk(
        void
        ) = default;

    virtual
    size_t
    GetLength(
        void
        ) const = 0;

    virtual
    PVOID
    GetVirtualAddress(
        void
        ) const = 0;

    virtual
    LOGICAL_ADDRESS
    GetLogicalAddress(
        void
        ) const = 0;

private:

    NxBufferPool* m_BufferPool;

};

class PAGED INxMemoryChunkAllocator :
    public PAGED_OBJECT<'rmxn'> // 'nxmc'
{

public:

    //the memory chunk allocated is always page aligned and is the multiple of PAGE_SIZE.
    //Any unused bytes on the last allocated page are essentially wasted.
    virtual
    INxMemoryChunk *
    AllocateMemoryChunk(
        _In_ size_t Size,
        _In_ NODE_REQUIREMENT PreferredNode
        ) = 0;

};

class PAGED NxBufferManager :
    public PAGED_OBJECT<'mbxn'> // 'nxbm'
{

public:

    NxBufferManager(
        void
        );

    ~NxBufferManager(
        void
        );

    NTSTATUS
    AddMemoryConstraints(
        _In_ NET_CLIENT_MEMORY_CONSTRAINTS * MemoryConstraints
        );

    NTSTATUS
    InitializeMemoryChunkAllocator(
        void
        );

    NTSTATUS
    AllocateMemoryChunks(
        _In_ size_t MinimumRequestedSize,
        _In_ size_t MinimumChunkSize,
        _In_ NODE_REQUIREMENT PreferredNode,
        _Inout_ Rtl::KArray<wistd::unique_ptr<INxMemoryChunk>> & MemoryChunks
        );

private:

    UINT64
        m_Id;

    Rtl::KArray<NET_CLIENT_MEMORY_CONSTRAINTS>
        m_MemoryConstraints;

    wistd::unique_ptr<INxMemoryChunkAllocator>
        m_MemoryChunkAllocator;

};

