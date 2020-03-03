// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/checksumtypes.h>

NET_PACKET_CHECKSUM
NxTranslateTxPacketChecksum(
    NET_PACKET const &packet,
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const &info
);

NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO
NxTranslateRxPacketChecksum(
    NET_PACKET const* packet,
    NET_EXTENSION const* checksumExtension,
    UINT32 packetIndex
);

