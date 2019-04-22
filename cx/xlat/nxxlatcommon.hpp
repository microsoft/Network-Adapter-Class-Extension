// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Shared header for the entire XLAT app.

--*/

#pragma once

#define NTSTRSAFE_NO_UNICODE_STRING_FUNCTIONS
#include <NxTrace.hpp>
#include <NxXlatTraceLogging.hpp>

#include "NetClientApi.h"
#include "NxApp.hpp"

#define MS_TO_100NS_CONVERSION 10000

#ifndef RTL_IS_POWER_OF_TWO
#  define RTL_IS_POWER_OF_TWO(Value) \
    ((Value != 0) && !((Value) & ((Value) - 1)))
#endif

using unique_nbl = wistd::unique_ptr<NET_BUFFER_LIST, wil::function_deleter<decltype(&NdisFreeNetBufferList), NdisFreeNetBufferList>>;
using unique_nbl_pool = wil::unique_any<NDIS_HANDLE, decltype(&::NdisFreeNetBufferListPool), &::NdisFreeNetBufferListPool>;

__inline
SIZE_T
NetPacketFragmentGetSize(
    void
)
{
    // In the future the size of a fragment might not be a simple sizeof(NET_FRAGMENT), so create
    // a function stub now to easily track the places where we need such information
    return sizeof(NET_FRAGMENT);
}

