// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "IBufferVectorAllocator.hpp"

class PAGED NonPagedAllocator : public IBufferVectorAllocator
{
public:

    IBufferVector *
    AllocateVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset,
        _In_ NODE_REQUIREMENT
    );
};
