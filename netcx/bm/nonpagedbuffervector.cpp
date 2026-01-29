// Copyright (C) Microsoft Corporation. All rights reserved.
#include "BmPrecomp.hpp"

#include "NonPagedBufferVector.hpp"
#include "NonPagedBufferVector.tmh"

NonPagedBufferVector::NonPagedBufferVector(
    _In_ size_t BufferCount,
    _In_ size_t BufferSize,
    _In_ size_t BufferOffset,
    _In_ Rtl::KArray<KPoolPtrNP<void>, NonPagedPoolBaseCacheAligned> && NonPagedBufferVector
)
    : IBufferVector(BufferCount, BufferOffset, BufferSize)
    , m_NonPagedBufferVectorKmAddresses(wistd::move(NonPagedBufferVector))
{
}

NonPagedBufferVector::~NonPagedBufferVector(
    void
)
{
    UnmapFromUserModeAddressSpace();
}

_Use_decl_annotations_
void *
NonPagedBufferVector::GetKernelModeVirtualAddress(
    size_t Index
) const
{
    return m_NonPagedBufferVectorKmAddresses[Index].get();
}

_Use_decl_annotations_
ULONG64
NonPagedBufferVector::GetLogicalAddress(
    size_t Index
) const
{
    UNREFERENCED_PARAMETER(Index);

    return 0;
}
