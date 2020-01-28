// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

namespace NetCx {

namespace Core {

class PAGED IBufferVectorAllocator :
    public PAGED_OBJECT<'amxn'> // 'nxma'
{

public:

    //individual buffer size must be always page aligned and is the multiple of PAGE_SIZE.

    virtual
    IBufferVector*
    AllocateVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset,
        _In_ NODE_REQUIREMENT PreferredNode
    ) = 0;
};

class PAGED BufferManager :
    public PAGED_OBJECT<'mbxn'> // 'nxbm'
{

public:

    NTSTATUS
    AddMemoryConstraints(
        _In_ NET_CLIENT_MEMORY_CONSTRAINTS* MemoryConstraints
    );

    NTSTATUS
    InitializeBufferVectorAllocator(
        void
    );

    NTSTATUS
    AllocateBufferVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferAlignment,
        _In_ size_t BufferAlignmentOffset,
        _In_ NODE_REQUIREMENT PreferredNode,
        _Out_ IBufferVector** BufferVector
    );

private:

    UINT64
        m_Id;

    Rtl::KArray<NET_CLIENT_MEMORY_CONSTRAINTS>
        m_MemoryConstraints;

    wistd::unique_ptr<IBufferVectorAllocator>
        m_BufferVectorAllocator;

};

} //namespace Core
} //namespace NetCx
