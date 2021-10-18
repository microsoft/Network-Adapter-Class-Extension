// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

NET_PACKET_LAYER2_TYPE
TranslateMedium(
    _In_ NDIS_MEDIUM Medium
);

void
NxGetPacketLayout(
    _In_ NDIS_MEDIUM MediaType,
    _In_ NET_RING_COLLECTION const * Descriptor,
    _In_ NET_EXTENSION const & VirtualAddressExtension,
    _Inout_ NET_PACKET * Packet,
    _In_ size_t PayloadBackfill = 0
);

enum class AddressType : UINT8
{
    Unicast = 0x1,
    Multicast = 0x2,
    Broadcast = 0x3,
};

AddressType
NxGetPacketAddressType(
    _In_ NET_PACKET_LAYER2_TYPE Layer2Type,
    _In_ NET_BUFFER const & NetBuffer
);
