// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include "NxBuffer.tmh"
#include "NxBuffer.hpp"

#include "NxQueue.hpp"

#define NXQUEUE_FROM_RING_BUFFER(rb) ((NxQueue*)(rb)->OSReserved2[1])

PVOID
NxBuffer::_GetTypedClientContext(
    _In_ NET_DATAPATH_DESCRIPTOR const * Descriptor,
    _In_ NET_PACKET* NetPacket,
    _In_ PCNET_CONTEXT_TYPE_INFO TypeInfo
    )
{
    auto rb = NET_DATAPATH_DESCRIPTOR_GET_PACKET_RING_BUFFER(Descriptor);
    NxQueue *queue = NXQUEUE_FROM_RING_BUFFER(rb);

    if (queue->GetPrivateExtensionOffset() == 0)
    {
        // There are no client contexts in the NET_PACKET
        return nullptr;
    }

    for (auto& internalToken : queue->GetClientContextInfo())
    {
        // See if the context type exists in the NET_PACKET
        if (internalToken.ContextTypeInfo == TypeInfo)
        {
            return (PVOID)((PUCHAR)NetPacket + internalToken.Offset);
        }
    }

    return nullptr;
}

PVOID
NxBuffer::_GetClientContextFromToken(
    _In_ NET_DATAPATH_DESCRIPTOR const * Descriptor,
    _In_ NET_PACKET* NetPacket,
    _In_ PNET_PACKET_CONTEXT_TOKEN Token
    )
{
    auto rb = NET_DATAPATH_DESCRIPTOR_GET_PACKET_RING_BUFFER(Descriptor);
    NxQueue *queue = NXQUEUE_FROM_RING_BUFFER(rb);

    if (queue->GetPrivateExtensionOffset() == 0)
    {
        // There are no client contexts in the NET_PACKET
        return nullptr;
    }

    NET_PACKET_CONTEXT_TOKEN_INTERNAL *internalToken = reinterpret_cast<NET_PACKET_CONTEXT_TOKEN_INTERNAL *>(Token);
    return (PVOID)((PUCHAR)NetPacket + internalToken->Offset);
}
