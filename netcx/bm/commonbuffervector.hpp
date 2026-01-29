// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "IBufferVector.hpp"

class PAGED CommonBufferVector : public IBufferVector
{
    friend class DmaAllocator;

public:

    CommonBufferVector(
        _In_ DMA_ADAPTER * DmaAdapter,
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset,
        _In_ PDMA_COMMON_BUFFER_VECTOR CommonBufferVector,
        _In_ Rtl::KArray<void*, NonPagedPoolNx> && KernelModeAddress,
        _In_ Rtl::KArray<ULONG64, NonPagedPoolNx> && LogicalAddress
    );

    ~CommonBufferVector(
        void
    );

    virtual
    void *
    GetKernelModeVirtualAddress(
        _In_ size_t Index
    ) const override;

    virtual
    ULONG64
    GetLogicalAddress(
        _In_ size_t Index
    ) const override;

private:

    DMA_ADAPTER *
        m_DmaAdapter;

    PDMA_COMMON_BUFFER_VECTOR
        m_CommonBufferVector = nullptr;

    size_t
        m_BufferOffset = 0;

    Rtl::KArray<void *, NonPagedPoolNx>
        m_kernelModeAddress;

    Rtl::KArray<ULONG64, NonPagedPoolNx>
        m_logicalAddress;
};
