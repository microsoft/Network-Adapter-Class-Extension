// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

class PAGED IBufferVector :
    public NONPAGED_OBJECT<'vmxn'>
{
    friend class BufferPool;

public:

    IBufferVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferOffset,
        _In_ size_t BufferCapacity
    );

    virtual
    ~IBufferVector(
        void
    ) = default;

    virtual
    void
    GetEntryByIndex(
        _In_ size_t Index,
        _Out_ NET_DATA_HEADER* NetDataHeader
    ) const = 0;

protected:

    size_t
        m_BufferCount = 0;

    size_t
        m_BufferOffset = 0;

    size_t
        m_BufferCapacity = 0;
};
