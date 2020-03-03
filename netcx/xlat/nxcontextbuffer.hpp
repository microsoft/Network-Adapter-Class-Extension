// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <NetRingBuffer.h>
#include <NetPacket.h>

#include "NxRingBuffer.hpp"


class NxContextBuffer
{

public:

    NxContextBuffer(
        _In_ NxRingBuffer const & PacketRing
        );

    NTSTATUS
    Initialize(
        _In_ size_t ElementSize
        );

    template <typename T>
    T &
    GetPacketContext(
        _In_ size_t Index
        ) const
    {
        return *static_cast<T *>(NetRingBufferGetElementAtIndex(m_contextRing.get(), static_cast<UINT32>(Index)));
    }

    template <typename T>
    T &
    GetPacketContext(
        _In_ NET_PACKET const & Packet
        ) const
    {
        auto const offset = reinterpret_cast<unsigned char const *>(&Packet) - m_packetRing.Get()->Buffer;

        NT_ASSERT(offset % m_packetRing.Get()->ElementStride == 0);

        return GetPacketContext<T>(offset / m_packetRing.Get()->ElementStride);
    }

private:

    NxRingBuffer const &
        m_packetRing;

    KPoolPtr<NET_RING_BUFFER>
        m_contextRing;
};

