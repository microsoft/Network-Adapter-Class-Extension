// Copyright (C) Microsoft Corporation. All rights reserved.
#include "BmPrecomp.hpp"

#include <kptr.h>

#include "NonPagedBufferVector.hpp"
#include "NonPagedAllocator.hpp"
#include "NonPagedAllocator.tmh"


_Use_decl_annotations_
IBufferVector *
NonPagedAllocator::AllocateVector(
    size_t BufferCount,
    size_t BufferSize,
    size_t BufferOffset,
    NODE_REQUIREMENT
)
{
    Rtl::KArray<KPoolPtrNP<void>, NonPagedPoolBaseCacheAligned> nonPagedBufferVector;

    if (!nonPagedBufferVector.resize(BufferCount))
    {
        return nullptr;
    }

    for (size_t i = 0; i < BufferCount; i++)
    {
        auto memory = MakeSizedPoolPtrNP<BYTE>(
            BUFFER_MANAGER_POOL_TAG,
            BufferSize);

        if (!memory)
        {
            return nullptr;
        }

        nonPagedBufferVector[i] = wistd::move(memory);
    }

    auto bufferVector = wil::make_unique_nothrow<NonPagedBufferVector>(
        BufferCount,
        BufferSize,
        BufferOffset,
        wistd::move(nonPagedBufferVector));

    if (!bufferVector)
    {
        return nullptr;
    }

    return bufferVector.release();
}
