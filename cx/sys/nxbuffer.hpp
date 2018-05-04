// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxBuffer object.

--*/

#pragma once

//
// Holds information about a context on a NET_PACKET
//
typedef struct _NET_PACKET_CONTEXT_TOKEN_INTERNAL
{
    PCNET_CONTEXT_TYPE_INFO ContextTypeInfo;
    ULONG Offset;
    ULONG Size;
} NET_PACKET_CONTEXT_TOKEN_INTERNAL;

class NxBuffer {

public:

    static
    PVOID
    _GetTypedClientContext(
        _In_ NET_DATAPATH_DESCRIPTOR const * Descriptor,
        _In_ NET_PACKET* NetPacket,
        _In_ PCNET_CONTEXT_TYPE_INFO TypeInfo
        );

    static
    PVOID
    _GetClientContextFromToken(
        _In_ NET_DATAPATH_DESCRIPTOR const * Descriptor,
        _In_ NET_PACKET* NetPacket,
        _In_ PNET_PACKET_CONTEXT_TOKEN Token
        );
};
