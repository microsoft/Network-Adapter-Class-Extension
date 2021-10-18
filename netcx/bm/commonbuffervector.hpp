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
        _In_ size_t BufferOffset
    );

    ~CommonBufferVector(
        void
    );

    NONPAGED
    void
    GetEntryByIndex(
        _In_ size_t Index,
        _Out_ NET_DATA_HEADER * NetDataHeader
    ) const override;

private:

    DMA_ADAPTER *
        m_DmaAdapter;

    PDMA_COMMON_BUFFER_VECTOR
        m_CommonBufferVector = nullptr;

    size_t
        m_BufferOffset = 0;
};
