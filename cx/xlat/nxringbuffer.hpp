// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxRingBuffer wraps a NET_RING_BUFFER, providing simple accessor methods
    for inserting and removing items into the ring buffer.

--*/

#pragma once

#include "NxRingBufferRange.hpp"

struct NxRingBufferCounters
{
    ULONG64 NumberOfNetPacketsProduced = 0;
    ULONG64 NumberOfNetPacketsConsumed = 0;
    ULONG64 CumulativeRingBufferDepthInLastInterval = 0; // from begin index to end index
    ULONG64 IterationCountInLastInterval = 0;
    ULONG64 RingbufferFullyOccupiedCount = 0;
    ULONG64 RingbufferEmptyCount = 0;
    ULONG64 RingbufferPartiallyOccupiedCount = 0;
};

/// Encapsulates a NET_RING_BUFFER
class NxRingBuffer
{
public:

    PAGED ~NxRingBuffer();

    PAGED NTSTATUS
    Initialize(
        NET_RING_BUFFER * RingBuffer
        );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_RING_BUFFER *Get() { return m_rb; }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_RING_BUFFER const *Get() const { return m_rb; }

    NET_PACKET *GetPacketAtIndex(UINT32 i) { return reinterpret_cast<NET_PACKET*>(NetRingBufferGetElementAtIndex(Get(), i)); }
    NET_PACKET const *GetPacketAtIndex(UINT32 i) const
    {
        return reinterpret_cast<NET_PACKET*>(NetRingBufferGetElementAtIndex(const_cast<NET_RING_BUFFER*>(Get()), i));
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
    bool AllPacketsOwnedByNic() const
    {
        return m_rb->EndIndex == ((m_rb->BeginIndex - 1) & m_rb->ElementIndexMask);
    }

    // Returns an iterable range of packets that are empty and available for
    // deliver to the nic.
    //
    // There will always be at least one "free" packet that will not be
    // available in this view until a second packet has been moved from the
    // Completed view. This is to make the requirement that at least one packet
    // always remain in the OS view easier to reason about.
    _IRQL_requires_max_(DISPATCH_LEVEL)
    NetRbPacketRange AvailablePackets()
    {
        // a gap of 1 packet is always left between the Free packet range and the
        // Completed packet range.
        return NetRbPacketRange{ *m_rb, m_rb->EndIndex, ((GetNextOSIndex() - 1) & m_rb->ElementIndexMask) };
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

    // Returns the next NET_PACKET that will be given to the NIC.
    // (The packet isn't given to the NIC yet, though.  Call
    // GiveNextPacketToNic for that.)  Returns NULL if there is no
    // more packets that can be given to the NIC.
    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_PACKET *GetNextPacketToGiveToNic();

    // Gives the next packet to the NIC.  It is an error to give more
    // than N-1 packets to the NIC.
    _IRQL_requires_max_(DISPATCH_LEVEL)
    void GiveNextPacketToNic();

    // Returns the next NET_PACKET that was returned by the NIC.
    // Returns NULL if all packets from the NIC have been processed.
    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_PACKET *TakeNextPacketFromNic();

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
    void AdvanceEnd(NetRbPacketIterator const &iterator)
    {
        m_rb->EndIndex = iterator.GetIndex();
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NetRbPacketIterator Next()
    {
        return NetRbPacketIterator{ *m_rb, GetNextOSIndex() };
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void AdvanceNext(NetRbPacketIterator const &iterator)
    {
        GetNextOSIndex() = iterator.GetIndex();
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT16
    GetElementSize()
    {
        return m_rb->ElementStride;
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT32 &GetNextOSIndex() { return *reinterpret_cast<UINT32*>(&m_rb->OSReserved2[0]); }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT32 const &GetNextOSIndex() const { return *reinterpret_cast<UINT32 const*>(&m_rb->OSReserved2[0]); }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    UpdateRingbufferPacketCounters(
        _In_ NxRingBufferCounters const &Delta
        );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    UpdateRingbufferDepthCounters();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NxRingBufferCounters
    GetRingbufferCounters() const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    ResetRingbufferCounters();

private:
    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT32
    GetRingbufferDepth() const;

    NET_RING_BUFFER * m_rb = nullptr;
    NxRingBufferCounters m_rbCounters;
};
