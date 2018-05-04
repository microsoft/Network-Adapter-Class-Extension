// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxRingBuffer wraps a NET_RING_BUFFER, providing simple accessor methods
    for inserting and removing items into the ring buffer.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxRingBuffer.tmh"
#include "NxRingBuffer.hpp"

#include "NxRingBufferRange.hpp"

PAGED
NxRingBuffer::~NxRingBuffer()
{
}

PAGED
NTSTATUS 
NxRingBuffer::Initialize(
    NET_RING_BUFFER * RingBuffer
    )
{
    m_rb = RingBuffer;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NET_PACKET *
NxRingBuffer::GetNextPacketToGiveToNic()
{
    // Can't give the last packet to the NIC.
    if (AvailablePackets().Count() == 0)
        return nullptr;

    return NetRingBufferGetPacketAtIndex(m_rb, m_rb->EndIndex);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
NxRingBuffer::GiveNextPacketToNic()
{
    WIN_ASSERT(AvailablePackets().Count() != 0);

    m_rb->EndIndex = NetRingBufferIncrementIndex(m_rb, m_rb->EndIndex);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NET_PACKET *
NxRingBuffer::TakeNextPacketFromNic()
{
    auto &index = GetNextOSIndex();

    // We've processed all the packets.
    if (index == m_rb->BeginIndex)
        return nullptr;

    auto packet = NetRingBufferGetPacketAtIndex(m_rb, index);
    index = NetRingBufferIncrementIndex(m_rb, index);

    return packet;
}

UINT32
NxRingBuffer::Count() const
{
    return Get()->NumberOfElements;
}
