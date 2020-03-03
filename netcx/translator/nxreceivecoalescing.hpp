// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>
#include <net/rsctypes.h>

NDIS_RSC_NBL_INFO
NxTranslateRxPacketRsc(
    _In_ NET_PACKET const * packet,
    _In_ NET_EXTENSION const * rscExtension,
    _In_ UINT32 packetIndex
);

void *
NxTranslateRxPacketRscTimestamp(
    _In_ NET_PACKET const * packet,
    _In_ NET_EXTENSION const * rscTimestampExtension,
    _In_ UINT32 packetIndex
);
