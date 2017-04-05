// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "KPacket.h"
#include "NxHeaderParser.hpp"

struct NxLayer2Layout
{
    NET_PACKET_LAYER2_TYPE Type;
    UINT16 Length;
    NET_PACKET_LAYER3_TYPE L3Type;
};

struct NxLayer3Layout
{
    NET_PACKET_LAYER3_TYPE Type;
    UINT16 Length;
    NET_PACKET_LAYER4_TYPE L4Type;
};

struct NxLayer4Layout
{
    NET_PACKET_LAYER4_TYPE Type;
    UINT8 Length;
};

// Gets corresponding layer types to network protocol IDs
NET_PACKET_LAYER4_TYPE NxGetLayer4Type(KIPProtocolNumber number);

// Parses known headers
NxLayer2Layout NxGetEthernetLayout(NxPacketHeaderParser &parser);

NxLayer3Layout NxGetIPv4Layout(NxPacketHeaderParser &parser);
NxLayer3Layout NxGetIPv6Layout(NxPacketHeaderParser &parser);

NxLayer4Layout NxGetTcpLayout(NxPacketHeaderParser &parser);
NxLayer4Layout NxGetUdpLayout(NxPacketHeaderParser &parser);

// Parses unknown headers
NxLayer2Layout NxGetLayer2Layout(NDIS_MEDIUM mediaType, NxPacketHeaderParser &parser);
NxLayer3Layout NxGetLayer3Layout(NET_PACKET_LAYER3_TYPE type, NxPacketHeaderParser &parser);
NxLayer4Layout NxGetLayer4Layout(NET_PACKET_LAYER4_TYPE type, NxPacketHeaderParser &parser);

NET_PACKET_LAYOUT NxGetPacketLayout(NDIS_MEDIUM mediaType, NET_PACKET const& packet);

