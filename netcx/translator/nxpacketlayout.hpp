// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

_Success_(return)
bool
NxGetPacketEtherType(
    _In_ NET_RING_COLLECTION const *descriptor,
    _In_ NET_EXTENSION const & virtualAddressExtension,
    _In_ NET_PACKET const *packet,
    _Out_ USHORT *ethertype);

NET_PACKET_LAYOUT
NxGetPacketLayout(
    _In_ NDIS_MEDIUM mediaType,
    _In_ NET_RING_COLLECTION const *descriptor,
    _In_ NET_EXTENSION const & virtualAddressExtension,
    _In_ NET_PACKET const *packet,
    _In_ size_t PayloadBackfill = 0);
