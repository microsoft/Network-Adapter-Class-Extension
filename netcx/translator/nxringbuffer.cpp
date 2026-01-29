// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxRingBuffer wraps a NET_RING, providing simple accessor methods
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
    NET_RING * RingBuffer
)
{
    m_rb = RingBuffer;

    return STATUS_SUCCESS;
}

UINT32
NxRingBuffer::Count() const
{
    return Get()->NumberOfElements;
}
