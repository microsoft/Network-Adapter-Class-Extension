// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

typedef 
bool
FUNC_PARSE_LAYER_LAYOUT (
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_reads_(Offset + Length) UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

extern FUNC_PARSE_LAYER_LAYOUT* const ParseLayerDispatch[3];
