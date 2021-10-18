// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "IBufferVector.hpp"

class PAGED NonPagedBufferVector : public IBufferVector
{
    friend class NonPagedAllocator;

    struct Buffer
    {
        void *
            VirtualAddress = nullptr;

        MDL *
            Mdl = nullptr;
    };

public:

    NonPagedBufferVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferSize,
        _In_ size_t BufferOffset
    );

    ~NonPagedBufferVector(
        void
    );

    NONPAGED
    void
    GetEntryByIndex(
        size_t Index,
        NET_DATA_HEADER* NetDataHeader
    ) const override;

private:

    Rtl::KArray<Buffer, NonPagedPoolNxCacheAligned>
        m_NonPagedBufferVector;
};
