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
    auto bufferVector = wil::make_unique_nothrow<NonPagedBufferVector>(
        BufferCount,
        BufferSize,
        BufferOffset);

    if (!bufferVector)
    {
        return nullptr;
    }

    if (!bufferVector->m_NonPagedBufferVector.resize(BufferCount))
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

        auto const mdlSize = MmSizeOfMdl(
            memory.get(),
            BufferSize);

        auto mdl = MakeSizedPoolPtrNP<MDL>(BUFFER_MANAGER_POOL_TAG, mdlSize);

        if (!mdl)
        {
            return nullptr;
        }

        MmInitializeMdl(mdl.get(), memory.get(), BufferSize);
        MmBuildMdlForNonPagedPool(mdl.get());

        bufferVector->m_NonPagedBufferVector[i].VirtualAddress = memory.release();
        bufferVector->m_NonPagedBufferVector[i].Mdl = mdl.release();
    }

    return bufferVector.release();
}
