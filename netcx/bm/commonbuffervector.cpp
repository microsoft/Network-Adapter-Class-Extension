// Copyright (C) Microsoft Corporation. All rights reserved.
#include "BmPrecomp.hpp"

#include "CommonBufferVector.hpp"
#include "CommonBufferVector.tmh"


_Use_decl_annotations_
CommonBufferVector::CommonBufferVector(
    DMA_ADAPTER * DmaAdapter,
    size_t BufferCount,
    size_t BufferSize,
    size_t BufferOffset,
    PDMA_COMMON_BUFFER_VECTOR CommonBufferVector,
    Rtl::KArray<void *, NonPagedPoolNx> && KernelModeAddress,
    Rtl::KArray<ULONG64, NonPagedPoolNx> && LogicalAddress
)
    : IBufferVector(BufferCount, BufferOffset, BufferSize)
    , m_DmaAdapter(DmaAdapter)
    , m_CommonBufferVector(CommonBufferVector)
    , m_kernelModeAddress(wistd::move(KernelModeAddress))
    , m_logicalAddress(wistd::move(LogicalAddress))
{
}

CommonBufferVector::~CommonBufferVector(
    void
)
{
    UnmapFromUserModeAddressSpace();

    // Free the common buffer vector if one was allocated
    if (m_CommonBufferVector != nullptr)
    {
        m_DmaAdapter->DmaOperations->FreeCommonBufferVector(m_DmaAdapter, m_CommonBufferVector);
        m_CommonBufferVector = nullptr;
    }
}

_Use_decl_annotations_
void *
CommonBufferVector::GetKernelModeVirtualAddress(
    size_t Index
) const
{
    return m_kernelModeAddress[Index];
}

_Use_decl_annotations_
ULONG64
CommonBufferVector::GetLogicalAddress(
    size_t Index
) const
{
    return m_logicalAddress[Index];
}
