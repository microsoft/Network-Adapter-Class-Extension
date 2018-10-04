// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxRingBufferRange.hpp"

class NxBounceBufferPool
{
public:

    ~NxBounceBufferPool(
        void
        );

    NTSTATUS
    Initialize(
        _In_ NET_CLIENT_DISPATCH const &ClientDispatch,
        _In_ NET_DATAPATH_DESCRIPTOR const *Descriptor,
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

    NET_CLIENT_BUFFER_POOL m_bufferPool = nullptr;
    NET_CLIENT_BUFFER_POOL_DISPATCH const *m_bufferPoolDispatch = nullptr;

    NET_DATAPATH_DESCRIPTOR const *m_descriptor = nullptr;

    size_t m_bufferSize = 0;
};
