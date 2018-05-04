// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

NET_PACKET_CHECKSUM
NxTranslateTxPacketChecksum(
    NET_PACKET const &packet,
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const &info
    );

NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO
NxTranslateRxPacketChecksum(
    NET_PACKET const* packet,
    size_t checksumOffset
    );
