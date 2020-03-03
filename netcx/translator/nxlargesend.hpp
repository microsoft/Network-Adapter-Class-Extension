// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>
#include <net/lsotypes.h>

NET_PACKET_LSO
NxTranslateTxPacketLargeSendSegmentation(
    NET_PACKET const & packet,
    NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO const & info
);

