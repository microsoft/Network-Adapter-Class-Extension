// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxRingContext.tmh"
#include "NxRingContext.hpp"

NxRingContext::NxRingContext(
    _In_ NET_RING_COLLECTION const & Rings,
    _In_ size_t RingIndex
) :
    m_rings(Rings),
    m_ringIndex(RingIndex)
{
}

NTSTATUS
NxRingContext::Initialize(
    _In_ size_t ElementSize
)
{
    auto const ringSize = m_rings.Rings[m_ringIndex]->NumberOfElements * ElementSize;
    auto const allocationSize = ringSize + FIELD_OFFSET(NET_RING, Buffer[0]);

    auto context = reinterpret_cast<NET_RING *>(
        ExAllocatePoolWithTag(NonPagedPoolNxCacheAligned, allocationSize, 'BRxN'));
    if (! context)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(context, allocationSize);
    context->ElementStride = static_cast<USHORT>(ElementSize);
    context->NumberOfElements = m_rings.Rings[m_ringIndex]->NumberOfElements;
    context->ElementIndexMask = m_rings.Rings[m_ringIndex]->ElementIndexMask;

    m_context.reset(context);

    return STATUS_SUCCESS;
}

