// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <ndis/nbl.h>

class NxRxXlat;

union COALEASCING_FALLBACK_INFO
{
    struct
    {
        ULONG
            Layer2Type: 4;

        ULONG
            Layer2HeaderLength: 7;

        ULONG
            Reserved: 21;
    } Info;

    PVOID
        Value;
};

struct RX_NBL_CONTEXT
{
    NxRxXlat *
        Queue;

    COALEASCING_FALLBACK_INFO
        CoaleascingFallbackInfo;
};

static_assert(
    sizeof(RX_NBL_CONTEXT) <= FIELD_SIZE(NET_BUFFER_LIST, MiniportReserved),
    "the size of RX_NBL_CONTEXT struct is larger than available space on NBL reserved for miniport");

inline
RX_NBL_CONTEXT *
GetRxContextFromNbl(
    NET_BUFFER_LIST * NetBufferList
)
{
    return reinterpret_cast<RX_NBL_CONTEXT*>(
        &NET_BUFFER_LIST_MINIPORT_RESERVED(NetBufferList)[0]);
}

struct RX_NB_CONTEXT
{
    //
    // this NB's NET_BUFFER_MINIPORT_RESERVED(NB)[0]
    // field is used for the following purpose:
    //
    // if the packet has multiple fragments
    //
    bool HasMultipleFragments = false;
};

inline
RX_NB_CONTEXT*
GetRxContextFromNb(PNET_BUFFER Nb)
{
    return reinterpret_cast<RX_NB_CONTEXT*>(
        &NET_BUFFER_MINIPORT_RESERVED(Nb)[0]);
}
