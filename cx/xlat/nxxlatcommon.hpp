// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Shared header for the entire XLAT app.

--*/

#pragma once

#define NTSTRSAFE_NO_UNICODE_STRING_FUNCTIONS
#include <NxTrace.hpp>
#include <NxXlatTraceLogging.hpp>

#include "NetClientApi.h"
#include "NxApp.hpp"

#define MS_TO_100NS_CONVERSION 10000

#ifndef RTL_IS_POWER_OF_TWO
#  define RTL_IS_POWER_OF_TWO(Value) \
    ((Value != 0) && !((Value) & ((Value) - 1)))
#endif

using unique_nbl = wistd::unique_ptr<NET_BUFFER_LIST, wil::function_deleter<decltype(&NdisFreeNetBufferList), NdisFreeNetBufferList>>;
using unique_nbl_pool = wil::unique_any<NDIS_HANDLE, decltype(&::NdisFreeNetBufferListPool), &::NdisFreeNetBufferListPool>;

__inline
UINT32
NetRingBufferIncrementIndexByCount(
    _In_ NET_RING_BUFFER CONST *RingBuffer,
    _In_ UINT32 Index,
    _In_ UINT32 Count
)
{
    return (Index + Count) & RingBuffer->ElementIndexMask;
}

__inline
NET_PACKET *
NetRingBufferGetPacketAtIndex(
    _In_ NET_RING_BUFFER *RingBuffer,
    _In_ UINT32 Index
)
{
    return (NET_PACKET*) NetRingBufferGetElementAtIndex(RingBuffer, Index);
}

__inline
SIZE_T
NetPacketFragmentGetSize(
    void
)
{
    // In the future the size of a fragment might not be a simple sizeof(NET_PACKET_FRAGMENT), so create
    // a function stub now to easily track the places where we need such information
    return sizeof(NET_PACKET_FRAGMENT);
}

__inline
void
DetachFragmentsFromPacket(
    _Inout_ NET_PACKET& Packet,
    _In_ NET_DATAPATH_DESCRIPTOR const &Descriptor
)
{
    if (Packet.IgnoreThisPacket)
    {
        NT_ASSERT(Packet.FragmentValid == FALSE);
        return;
    }

    auto fragmentRing = NET_DATAPATH_DESCRIPTOR_GET_FRAGMENT_RING_BUFFER(&Descriptor);
    UINT32 fragmentCount = NetPacketGetFragmentCount(&Descriptor, &Packet);

    NT_ASSERT(Packet.FragmentOffset == fragmentRing->BeginIndex);
    NT_ASSERT(fragmentCount <= NetRingBufferGetNumberOfElementsInRange(fragmentRing, fragmentRing->BeginIndex, fragmentRing->EndIndex));

    fragmentRing->BeginIndex = NetRingBufferIncrementIndexByCount(fragmentRing, fragmentRing->BeginIndex, fragmentCount);

    Packet.FragmentValid = FALSE;
    Packet.FragmentOffset = 0;
}
