/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxRingBuffer.hpp

Abstract:

    The NxRingBuffer wraps a NET_RING_BUFFER, providing simple accessor methods
    for inserting and removing items into the ring buffer.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxRingBuffer.tmh"
#include "NxRingBuffer.hpp"

PAGED NxRingBuffer::~NxRingBuffer()
{
    if (m_packetClient)
    {
        NetPacketDestroyMany(
            m_packetClient,
            reinterpret_cast<NET_PACKET*>(m_rb->Buffer),
            m_rb->ElementStride,
            m_rb->NumberOfElements);
    }
}

PAGED
NTSTATUS 
NxRingBuffer::Initialize(
    _In_ HNETPACKETCLIENT packetClient,
    _In_ ULONG numberOfElements,
    _In_ ULONG numberOfFragments,
    _In_ ULONG appContextSize,
    _In_ ULONG clientContextSize)
{
    NT_ASSERT(!m_rb);

    m_appContextSize = ALIGN_UP_BY(appContextSize, MEMORY_ALLOCATION_ALIGNMENT);
    m_contextOffset = NetPacketGetSize(numberOfFragments);

    auto const contextSize = ALIGN_UP_BY(m_appContextSize + clientContextSize, MEMORY_ALLOCATION_ALIGNMENT);
    auto const elementSize = ALIGN_UP_BY(m_contextOffset + contextSize, NET_PACKET_ALIGNMENT_BYTES);
    auto const ringSize = numberOfElements * elementSize;
    auto const allocationSize = ringSize + FIELD_OFFSET(NET_RING_BUFFER, Buffer[0]);

    if (!WIN_VERIFY(elementSize <= USHORT_MAX))
        return STATUS_INTEGER_OVERFLOW;

    auto p = reinterpret_cast<NET_RING_BUFFER*>(ExAllocatePoolWithTag(NonPagedPoolNxCacheAligned, allocationSize, 'BRxN'));
    if (!p)
        return STATUS_INSUFFICIENT_RESOURCES;
    m_rb.reset(p);

    RtlZeroMemory(p, allocationSize);

    m_rb->ElementStride = static_cast<USHORT>(elementSize);
    m_rb->NumberOfElements = numberOfElements;
    m_rb->ElementIndexMask = numberOfElements - 1;

    NetPacketInitializeMany(
        packetClient, 
        m_rb->Buffer, 
        static_cast<ULONG>(ringSize), 
        static_cast<ULONG>(elementSize),
        numberOfFragments,
        numberOfElements);

    m_packetClient = packetClient;

    for (ULONG i = 0; i < numberOfElements; i++)
    {
        auto *packet = NetRingBufferGetPacketAtIndex(m_rb.get(), i);

        ULONG_PTR offsetToHeader;
        auto status = RtlULongPtrSub((ULONG_PTR)packet, (ULONG_PTR)m_rb.get(), &offsetToHeader);
        if (!NT_SUCCESS(status))
            return status;

        status = RtlULongPtrToUInt32(offsetToHeader, &packet->Reserved2);
        if (!NT_SUCCESS(status))
            return status;
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
UINT32 
NxRingBuffer::GetNumPacketsOwnedByNic() const
{
    return NetRingBufferGetNumberOfElementsInRange(m_rb.get(), m_rb->BeginIndex, m_rb->EndIndex);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
UINT32
NxRingBuffer::GetNumPacketsOwnedByOS() const
{
    auto numOwnedByNic = GetNumPacketsOwnedByNic();
    return m_rb->NumberOfElements - numOwnedByNic;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NET_PACKET *
NxRingBuffer::GetNextPacketToGiveToNic()
{
    // Can't give the last packet to the NIC.
    if (1 == GetNumPacketsOwnedByOS())
        return nullptr;

    return NetRingBufferGetPacketAtIndex(m_rb.get(), m_rb->EndIndex);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
void
NxRingBuffer::GiveNextPacketToNic()
{
    WIN_ASSERT(GetNumPacketsOwnedByOS() > 1);

    m_rb->EndIndex = NetRingBufferIncrementIndex(m_rb.get(), m_rb->EndIndex);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NET_PACKET *
NxRingBuffer::TakeNextPacketFromNic()
{
    auto &index = GetNextOSIndex();

    // We've processed all the packets.
    if (index == m_rb->BeginIndex)
        return nullptr;

    auto packet = NetRingBufferGetPacketAtIndex(m_rb.get(), index);
    index = NetRingBufferIncrementIndex(m_rb.get(), index);

    return packet;
}

_Use_decl_annotations_
ULONG
NxRingBuffer::GetAppContextOffset()
{
    return m_contextOffset;
}

_Use_decl_annotations_
PVOID
NxRingBuffer::GetAppContext(NET_PACKET * packet)
{
    return (PVOID)((ULONG_PTR)packet + GetAppContextOffset());
}

_Use_decl_annotations_
ULONG
NxRingBuffer::GetClientContextOffset()
{
    return m_contextOffset + m_appContextSize;
}

_Use_decl_annotations_
PVOID
NxRingBuffer::GetClientContext(NET_PACKET * packet)
{
    return (PVOID)((ULONG_PTR)packet + GetClientContextOffset());
}

UINT32
NxRingBuffer::Count() const
{
    return Get()->NumberOfElements;
}
