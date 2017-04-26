/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxXlatCommon.hpp

Abstract:

    Shared header for the entire XLAT app.

--*/

#pragma once

#define NTSTRSAFE_NO_UNICODE_STRING_FUNCTIONS
#include "nxtrace.hpp"
#include "nxtracelogging.hpp"

#include "NxApp.hpp"

#define THREAD_802_15_4_MSIEx_PACKET_EXTENSION_NAME L"NblMediaSpecificInformationEx"
#define THREAD_802_15_4_STATUS_PACKET_EXTENSION_NAME L"Thread802_15_4_StatusExtension"

#ifndef RTL_IS_POWER_OF_TWO
#  define RTL_IS_POWER_OF_TWO(Value) \
    ((Value != 0) && !((Value) & ((Value) - 1)))
#endif


using unique_nbl = wistd::unique_ptr<NET_BUFFER_LIST, wil::function_deleter<decltype(&NdisFreeNetBufferList), NdisFreeNetBufferList>>;
using unique_nbl_pool = wil::unique_any<NDIS_HANDLE, decltype(&::NdisFreeNetBufferListPool), &::NdisFreeNetBufferListPool>;

using unique_packet_extension = wil::unique_any<HNETPACKETEXTENSION, decltype(&::NetPacketExtensionFree), &::NetPacketExtensionFree>;

__inline
NET_PACKET *
NetRingBufferGetPacketAtIndex(
    _In_ NET_RING_BUFFER *RingBuffer,
    _In_ UINT32 Index
)
{
    return (NET_PACKET*)NetRingBufferGetElementAtIndex(RingBuffer, Index);
}

#define NET_PACKET_INTERNAL_802_15_4_INFO(netpacket, offset) \
    (*(PVOID*)((PUCHAR)(netpacket) + (offset)))

#define NET_PACKET_INTERNAL_802_15_4_STATUS(netpacket, offset) \
    (*(NTSTATUS*)((PUCHAR)(netpacket) + (offset)))
