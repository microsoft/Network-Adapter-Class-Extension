/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxRingBuffer.hpp

Abstract:

    The NxRingBuffer wraps a NET_RING_BUFFER, providing simple accessor methods
    for inserting and removing items into the ring buffer.

--*/

#pragma once

/// Encapsulates a NET_RING_BUFFER
class NxRingBuffer
{
public:

    PAGED ~NxRingBuffer();

    PAGED NTSTATUS Initialize(
        _In_ HNETPACKETCLIENT packetClient,
        _In_ ULONG numberOfElements,
        _In_ ULONG numberOfFragments,
        _In_ ULONG appContextSize,
        _In_ ULONG clientContextSize);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_RING_BUFFER *Get() { return m_rb.get(); }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_RING_BUFFER const *Get() const { return m_rb.get(); }

    NET_PACKET *GetPacketAtIndex(UINT32 i) { return NetRingBufferGetPacketAtIndex(Get(), i); }

    // Returns the number of packets that are currently held by the NIC.
    // This is initially zero, and will always fall in the range [0, N-1].
    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT32 GetNumPacketsOwnedByNic() const;

    // Returns the number of packets that are currently held by the OS.
    // This is initially N, and will always fall in the range [1, N].
    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT32 GetNumPacketsOwnedByOS() const;

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

    // Returns the offset of a NET_PACKET's app context
    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG GetAppContextOffset();

    // Returns the address of a NET_PACKET's app context
    _IRQL_requires_max_(DISPATCH_LEVEL)
    PVOID GetAppContext(_In_ NET_PACKET* packet);

    // Returns the offset of a NET_PACKET's client context
    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG GetClientContextOffset();

    // Returns the address of a NET_PACKET's client context
    _IRQL_requires_max_(DISPATCH_LEVEL)
    PVOID GetClientContext(_In_ NET_PACKET* packet);

    // Returns the size of the ring buffer
    UINT32 Count() const;

private:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT32 &GetNextOSIndex() { return *reinterpret_cast<UINT32*>(&m_rb->OSReserved2[0]); }

    KPoolPtr<NET_RING_BUFFER> m_rb;

    HNETPACKETCLIENT m_packetClient = 0;
    ULONG m_appContextSize = 0;
    ULONG m_contextOffset = 0;
};
