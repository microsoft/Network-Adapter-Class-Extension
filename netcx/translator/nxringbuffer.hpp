// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxRingBuffer wraps a NET_RING, providing simple accessor methods
    for inserting and removing items into the ring buffer.

--*/

#pragma once

#include "NxRingBufferRange.hpp"

/// Encapsulates a NET_RING
class NxRingBuffer
{
public:

    PAGED ~NxRingBuffer();

    PAGED NTSTATUS
    Initialize(
        NET_RING * RingBuffer
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_RING * Get() { return m_rb; }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_RING const * Get() const { return m_rb; }

    NET_PACKET *GetPacketAtIndex(UINT32 i) { return reinterpret_cast<NET_PACKET*>(NetRingGetElementAtIndex(Get(), i)); }
    NET_PACKET const *GetPacketAtIndex(UINT32 i) const
    {
        return reinterpret_cast<NET_PACKET*>(NetRingGetElementAtIndex(const_cast<NET_RING *>(Get()), i));
    }

    // Returns an iterable range of packets that are currently held by the NIC.
    _IRQL_requires_max_(DISPATCH_LEVEL)
    NetRbPacketRange NicPackets()
    {
        return NetRbPacketRange{ Begin(), End() };
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool AnyNicPackets() const
    {
        return m_rb->BeginIndex != m_rb->EndIndex;
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool AnyAvailablePackets() const
    {
        return m_rb->EndIndex != ((GetNextOSIndex() - 1) & m_rb->ElementIndexMask);
    }

    // Returns an iterable range of packets that have been completed by the
    // NIC. The NBLs attached to these packets need to be returned to the upper layer.
    _IRQL_requires_max_(DISPATCH_LEVEL)
    NetRbPacketRange ReturnedPackets()
    {
        return NetRbPacketRange{ Next(), Begin() };
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool AnyReturnedPackets() const
    {
        return GetNextOSIndex() != m_rb->BeginIndex;
    }

    // Returns the size of the ring buffer
    UINT32 Count() const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NetRbPacketIterator Begin()
    {
        return NetRbPacketIterator{ *m_rb, m_rb->BeginIndex };
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NetRbPacketIterator End()
    {
        return NetRbPacketIterator{ *m_rb, m_rb->EndIndex };
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NetRbPacketIterator Next()
    {
        return NetRbPacketIterator{ *m_rb, GetNextOSIndex() };
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT16
    GetElementSize() const
    {
        return m_rb->ElementStride;
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT32 &GetNextOSIndex() { return m_rb->OSReserved0; }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT32 const &GetNextOSIndex() const { return m_rb->OSReserved0; }

private:

    NET_RING * m_rb = nullptr;
};
