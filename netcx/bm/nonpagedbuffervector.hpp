// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "IBufferVector.hpp"

class PAGED NonPagedBufferVector : public IBufferVector
{
    friend class NonPagedAllocator;

public:

    NonPagedBufferVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset,
        _In_ Rtl::KArray<KPoolPtrNP<void>, NonPagedPoolBaseCacheAligned> && NonPagedBufferVector
    );

    ~NonPagedBufferVector(
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
    Rtl::KArray<KPoolPtrNP<void>, NonPagedPoolBaseCacheAligned>
        m_NonPagedBufferVectorKmAddresses;
};
