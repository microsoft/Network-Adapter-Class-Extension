// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxBufferPool object.

--*/

#pragma once

#include "KBitmap.h"
#include "Mdl.hpp"

class PAGED NxBufferPool :
    public NONPAGED_OBJECT<'pbxn'> // 'nxbp'
{

public:

    NxBufferPool(
        void
        );

    ~NxBufferPool(
        void
        );

    NTSTATUS
    Initialize(
        _In_ size_t PoolSize,
        _In_ size_t RequestedSize,
        _In_ size_t AlignmentOffset,
        _In_ size_t Alignment,
        _Out_ size_t * MinimumSizeRequested,
        _Out_ size_t * MinimumChunkSize
        );

    NTSTATUS
    AddMemoryChunks(
        _In_ Rtl::KArray<wistd::unique_ptr<INxMemoryChunk>>& MemoryChunks
        );

    NONPAGED
    NTSTATUS
    Allocate(
        _Out_ void ** VirtualAddress,
        _Out_ LOGICAL_ADDRESS * LogicalAddress,
        _Out_ SIZE_T * Offset,
        _Out_ SIZE_T * AllocatedSize
        );

    NONPAGED
    VOID
    Free(
        _In_ PVOID VirtualAddress
        );

    NONPAGED
    size_t
    AvailableBuffersCount(
        void
        );

private:

    UINT64
        m_Id = 0;

    PVOID
        m_BaseVirtualAddress = nullptr;

    size_t
        m_ContiguousVirtualLength = 0;

    wistd::unique_ptr<RtlMdl>
        m_StitchedMdl;

    size_t
        m_NumMemoryChunks = 0;

    size_t
        m_MemoryChunkSize = 0;

    Rtl::KArray<wistd::unique_ptr<INxMemoryChunk>>
        m_MemoryChunks;

    struct NxChunkBaseAddress
    {
        PVOID VirtualAddress;
        LOGICAL_ADDRESS LogicalAddress;
    };

    Rtl::KArray<NxChunkBaseAddress>
        m_MemoryChunkBaseAddresses;

    size_t
        m_NumBuffersPerChunk = 0;

    //individual buffer size information
    size_t
        m_StrideSize = 0;

    size_t
        m_ChunkOffset = 0;

    size_t
        m_AlignmentOffset = 0;

    size_t
        m_Alignment = 1;

    //Pool wide size information
    size_t
        m_RequestedPoolSize = 0;

    size_t
        m_PopulatedPoolSize = 0;

    size_t
        m_NumBuffersInUse = 0;

    struct NxBufferDescriptor
    {
        PVOID VirtualAddress;
        LOGICAL_ADDRESS LogicalAddress;
        size_t ChunkIndex;
        size_t BufferIndex;
        PVOID Context;
    };

    Rtl::KArray<NxBufferDescriptor>
        m_Buffers;

    Rtl::KBitmap
        m_BuffersInUseFlag;

private:

    NTSTATUS
        StitchMemoryChunks();

    void
        FillBufferPool();
};

