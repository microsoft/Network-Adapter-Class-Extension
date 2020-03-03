// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxContextBuffer.tmh"
#include "NxContextBuffer.hpp"

NxContextBuffer::NxContextBuffer(
    _In_ NxRingBuffer const & PacketRing
    ) :
    m_packetRing(PacketRing)
{
}

NTSTATUS
NxContextBuffer::Initialize(
    _In_ size_t ElementSize
    )
{
    auto const ringSize = m_packetRing.Count() * ElementSize;
    auto const allocationSize = ringSize + FIELD_OFFSET(NET_RING_BUFFER, Buffer[0]);

    auto contextRing = reinterpret_cast<NET_RING_BUFFER *>(
        ExAllocatePoolWithTag(NonPagedPoolNxCacheAligned, allocationSize, 'BRxN'));
    if (! contextRing)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(contextRing, allocationSize);
    contextRing->ElementStride = static_cast<USHORT>(ElementSize);
    contextRing->NumberOfElements = m_packetRing.Count();
    contextRing->ElementIndexMask = m_packetRing.Get()->ElementIndexMask;

    m_contextRing.reset(contextRing);

    return STATUS_SUCCESS;
}

