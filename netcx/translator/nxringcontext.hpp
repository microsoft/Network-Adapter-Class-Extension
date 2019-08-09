// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/ringcollection.h>

class NxRingContext
{

public:

    NxRingContext(
        _In_ NET_RING_COLLECTION const & Rings,
        _In_ size_t RingIndex);

    NTSTATUS
    Initialize(
        _In_ size_t ElementSize);

    template <typename TContext>
    TContext &
    GetContext(
        _In_ size_t Index
    ) const
    {
        return *static_cast<TContext *>(
            NetRingGetElementAtIndex(
                m_context.get(),
                static_cast<UINT32>(Index)));
    }

    template <typename TContext, typename TElement>
    TContext &
    GetContextByElement(
        _In_ TElement const & Element
    ) const
    {
        auto const buffer = m_rings.Rings[m_ringIndex]->Buffer;
        auto const offset = reinterpret_cast<unsigned char const *>(&Element) - buffer;

        NT_ASSERT(offset % m_rings.Rings[m_ringIndex]->ElementStride == 0);

        return GetContext<TContext>(offset / m_rings.Rings[m_ringIndex]->ElementStride);
    }

private:

    NET_RING_COLLECTION const &
        m_rings;

    size_t const
        m_ringIndex;

    KPoolPtr<NET_RING>
        m_context;
};

