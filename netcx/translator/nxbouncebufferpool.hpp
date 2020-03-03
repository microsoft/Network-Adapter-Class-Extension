// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <net/packet.h>
#include <NxRingContext.hpp>

class NxBounceBufferPool
{

public:

    NxBounceBufferPool(
        NET_RING_COLLECTION const & Rings,
        NET_EXTENSION const & VirtualAddressExtension,
        NET_EXTENSION const & LogicalAddressExtension
    );

    ~NxBounceBufferPool(
        void
    );

    NTSTATUS
    Initialize(
        _In_ NET_CLIENT_DISPATCH const &ClientDispatch,
        _In_ NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES &DatapathCapabilities,
        _In_ size_t NumberOfBuffers
    );

    bool
    BounceNetBuffer(
        _In_ NET_BUFFER const &NetBuffer,
        _Inout_ NET_PACKET &NetPacket
    );

    void
    FreeBounceBuffers(
        _Inout_ NET_PACKET &NetPacket
    );

private:

    NxRingContext
        m_fragmentContext;

    NET_EXTENSION const &
        m_virtualAddressExtension;

    NET_EXTENSION const &
        m_logicalAddressExtension;

    NET_CLIENT_BUFFER_POOL m_bufferPool = nullptr;
    NET_CLIENT_BUFFER_POOL_DISPATCH const *m_bufferPoolDispatch = nullptr;

    NET_RING_COLLECTION const * m_rings = nullptr;

    size_t m_bufferSize = 0;
    size_t m_txPayloadBackfill = 0;
};

