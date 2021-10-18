// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "IBufferVectorAllocator.hpp"

class PAGED DmaAllocator : public IBufferVectorAllocator
{
public:

    DmaAllocator(
        _In_ NET_CLIENT_MEMORY_CONSTRAINTS::DMA & Dma
    );

    IBufferVector *
    AllocateVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset,
        _In_ NODE_REQUIREMENT PreferredNode
    );

private:

    DMA_ADAPTER *
        m_DmaAdapter;

    PHYSICAL_ADDRESS
        m_MaximumPhysicalAddress;

    bool
        m_CacheEnabled;

    NODE_REQUIREMENT
        m_PreferredNode;
};
