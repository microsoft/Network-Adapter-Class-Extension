/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxBuffer.cpp

Abstract:

    This is the main NetAdapterCx driver framework.





Environment:

    kernel mode only

Revision History:

--*/

#include "Nx.hpp"

// Tracing support
extern "C" {
#include "NxBuffer.tmh"
}

#define NET_RING_BUFFER_FROM_NET_PACKET(_netpacket) (NET_RING_BUFFER *)((PUCHAR)(_netpacket) - (_netpacket)->Reserved2)
#define NXQUEUE_FROM_RING_BUFFER(rb) ((NxQueue*)(rb)->OSReserved2[1])

PVOID
NxBuffer::_GetDefaultClientContext(
    _In_ NET_PACKET             *NetPacket
    )
/*++
Routine Description: 
    Retrieves the Default Client NET_PACKET Context
--*/
{
    auto rb = NET_RING_BUFFER_FROM_NET_PACKET(NetPacket);
    NxQueue *queue = NXQUEUE_FROM_RING_BUFFER(rb);
    return (PVOID)((PUCHAR)NetPacket + queue->m_info.ContextOffset);
}

ULONG_PTR
NxBuffer::_Get802_15_4Info(
    _In_ NET_PACKET *NetPacket
    )
/*++
Routine Description:
    Retrieves information used only by thread driver adapter. This is to help
    translate between NBL model used by the thread filter driver and the 
    NET_PACKET model used by the adapter.

    802.15.4 info refers to MediaSpecificInformationEx on the NBL.
--*/
{
    auto rb = NET_RING_BUFFER_FROM_NET_PACKET(NetPacket);
    NxQueue *queue = NXQUEUE_FROM_RING_BUFFER(rb);
    return (ULONG_PTR)((PUCHAR)NetPacket + queue->m_info.Thread802_15_4MSIExExtensionOffset);
}

ULONG_PTR
NxBuffer::_Get802_15_4Status(
    _In_ NET_PACKET *NetPacket
    )
/*++
Routine Description:
    Retrieves information used only by thread driver adapter. This is to help
    translate between NBL model used by the thread filter driver and the 
    NET_PACKET model used by the adapter.

    802.15.4 status refers to Status field on the NBL.
--*/
{
    auto rb = NET_RING_BUFFER_FROM_NET_PACKET(NetPacket);
    NxQueue *queue = NXQUEUE_FROM_RING_BUFFER(rb);
    return (ULONG_PTR)((PUCHAR)NetPacket + queue->m_info.Thread802_15_4StatusOffset);
}
