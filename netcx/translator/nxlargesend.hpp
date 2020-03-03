// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/lsotypes.h>

NET_PACKET_LARGE_SEND_SEGMENTATION
NxTranslateTxPacketLargeSendSegmentation(
    NET_PACKET const & packet,
    NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO const & info
);

