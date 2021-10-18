// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "IBufferVector.hpp"

class PAGED IBufferVectorAllocator :
    public PAGED_OBJECT<'amxn'>
{
public:

    //individual buffer size must be always page aligned and is the multiple of PAGE_SIZE.

    virtual
    IBufferVector *
    AllocateVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset,
        _In_ NODE_REQUIREMENT PreferredNode
    ) = 0;
};
