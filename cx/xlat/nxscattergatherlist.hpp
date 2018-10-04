// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxDma.hpp"

// Wrapper for a SCATTER_GATHER_LIST. It makes sure
// to free the DMA resources on destruction, unless
// if the release() method is called
class NxScatterGatherList
{
public:

    explicit NxScatterGatherList(
        _In_ NxDmaAdapter const &DmaAdapter
        );

    ~NxScatterGatherList(
        void
        );

    SCATTER_GATHER_LIST *
    release(
        void
        );

    SCATTER_GATHER_LIST *
    get(
        void
        ) const;

    SCATTER_GATHER_LIST ** const
    releaseAndGetAddressOf(
        void
        );

    SCATTER_GATHER_LIST const *
    operator->(
        void
        ) const;

    SCATTER_GATHER_LIST const &
    operator*(
        void
        ) const;

    NxScatterGatherList(const NxScatterGatherList&) = delete;
    NxScatterGatherList& operator=(const NxScatterGatherList&) = delete;

    NxScatterGatherList(const NxScatterGatherList&&) = delete;
    NxScatterGatherList& operator=(NxScatterGatherList&&) = delete;

private:
    NxDmaAdapter const &m_dmaAdapter;
    SCATTER_GATHER_LIST *m_scatterGatherList = nullptr;
};
