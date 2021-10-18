// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

#include <ntassert.h>

#include "Layer4Layout.hpp"

namespace Layer4
{

bool
ParseLayout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer4HeaderLength == 0U);

    switch (Layout.Layer4Type)
    {

    case NetPacketLayer4TypeTcp:

        return ParseTcpLayout(Layout, Pointer, Offset, Length);

    case NetPacketLayer4TypeUdp:

        return ParseUdpLayout(Layout, Pointer, Offset, Length);

    }

    return false;
}

_Use_decl_annotations_
bool
ParseTcpLayout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer4Type == NetPacketLayer4TypeTcp);
    NT_FRE_ASSERT(Layout.Layer4HeaderLength == 0U);

    auto const tcp = reinterpret_cast<TCP UNALIGNED const *>(&Pointer[Offset]);
    auto const size = sizeof(*tcp);
    if (size > Length)
    {
        return false;
    }

    auto const headersz = static_cast<size_t>(tcp->Header.DataOffset.DataOffset) << 2;
    if (headersz > Length)
    {
        return false;
    }

    Layout.Layer4HeaderLength = headersz;

    Offset += Layout.Layer4HeaderLength;
    Length -= Layout.Layer4HeaderLength;

    return true;
}

_Use_decl_annotations_
bool
ParseUdpLayout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer4Type == NetPacketLayer4TypeUdp);
    NT_FRE_ASSERT(Layout.Layer4HeaderLength == 0U);

    auto const udp = reinterpret_cast<UDP UNALIGNED const *>(&Pointer[Offset]);
    auto const size = sizeof(*udp);
    if (size > Length)
    {
        return false;
    }

    Layout.Layer4HeaderLength = size;

    Offset += Layout.Layer4HeaderLength;
    Length -= Layout.Layer4HeaderLength;

    return true;
}

}
