// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <ndis/nbl.h>

// GSO auxilary information stored inplace NBL.
struct GSO_CONTEXT
{
    bool
        Segmented = false;

    ULONG
        DataOffset = 0U;
};

struct TX_NBL_CONTEXT
{
    GSO_CONTEXT
        gsoNblContext = {};
};

static_assert(sizeof(TX_NBL_CONTEXT) <= FIELD_SIZE(NET_BUFFER_LIST, MiniportReserved),
              "the size of TX_NBL_CONTEXT struct is larger than available space on NBL reserved for miniport");

inline
_IRQL_requires_(PASSIVE_LEVEL)
GSO_CONTEXT *
GetGsoContextFromNetBufferList(
    NET_BUFFER_LIST * NetBufferList
)
{
    auto txNblContext = reinterpret_cast<TX_NBL_CONTEXT *>(&NetBufferList->MiniportReserved[0]);

    return &txNblContext->gsoNblContext;
}

inline
_IRQL_requires_(PASSIVE_LEVEL)
bool
IsNetBufferListSoftwareSegmented(
    _In_ NET_BUFFER_LIST const * NetBufferList
)
{
    return GetGsoContextFromNetBufferList(const_cast<NET_BUFFER_LIST *>(NetBufferList))->Segmented;
}
