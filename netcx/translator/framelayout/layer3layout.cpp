// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

#include <ntassert.h>

#include "Layer3Layout.hpp"

#define IP_VERSION_4 4
#define IP_VERSION_6 6

namespace Layer3
{

_Use_decl_annotations_
bool
ParseLayout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer3HeaderLength == 0U);

    switch (Layout.Layer3Type)
    {

    case NetPacketLayer3TypeUnspecified:

        if (Layout.Layer2Type != NetPacketLayer2TypeNull)
        {
            return false;
        }
        return ParseIPv4orIPv6Layout(Layout, Pointer, Offset, Length);

    case NetPacketLayer3TypeIPv4UnspecifiedOptions:
    case NetPacketLayer3TypeIPv4WithOptions:
    case NetPacketLayer3TypeIPv4NoOptions:

        return ParseIPv4Layout(Layout, Pointer, Offset, Length);

    case NetPacketLayer3TypeIPv6UnspecifiedExtensions:
    case NetPacketLayer3TypeIPv6WithExtensions:
    case NetPacketLayer3TypeIPv6NoExtensions:

        return ParseIPv6Layout(Layout, Pointer, Offset, Length);

    }

    return false;
}

_Use_decl_annotations_
bool
ParseIPv4orIPv6Layout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer3Type == NetPacketLayer3TypeUnspecified);
    NT_FRE_ASSERT(Layout.Layer3HeaderLength == 0U);

    if (ParseIPv4Layout(Layout, Pointer, Offset, Length))
    {
        return true;
    }

    NT_FRE_ASSERT(Layout.Layer3Type == NetPacketLayer3TypeUnspecified);
    NT_FRE_ASSERT(Layout.Layer3HeaderLength == 0U);

    if (ParseIPv6Layout(Layout, Pointer, Offset, Length))
    {
        return true;
    }

    NT_FRE_ASSERT(Layout.Layer3Type == NetPacketLayer3TypeUnspecified);
    NT_FRE_ASSERT(Layout.Layer3HeaderLength == 0U);

    return true;
}

_Use_decl_annotations_
bool
ParseIPv4Layout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer3Type != NetPacketLayer3TypeIPv6UnspecifiedExtensions);
    NT_FRE_ASSERT(Layout.Layer3Type != NetPacketLayer3TypeIPv6WithExtensions);
    NT_FRE_ASSERT(Layout.Layer3Type != NetPacketLayer3TypeIPv6NoExtensions);
    NT_FRE_ASSERT(Layout.Layer3HeaderLength == 0U);

    auto const ipv4 = reinterpret_cast<IPv4 UNALIGNED const *>(&Pointer[Offset]);
    auto const size = sizeof(*ipv4);
    if (size > Length)
    {
        return false;
    }

    if (ipv4->Header.Version != IP_VERSION_4)
    {
        return false;
    }

    auto const headersz = static_cast<size_t>(ipv4->Header.HeaderLength) << 2;
    if (headersz > Length)
    {
        return false;
    }

    auto const options = size != headersz;
    Layout.Layer3HeaderLength = headersz;
    Layout.Layer3Type = options ? NetPacketLayer3TypeIPv4WithOptions : NetPacketLayer3TypeIPv4NoOptions;

    switch (ipv4->Header.Protocol)
    {

    case Protocol::TCP:
        Layout.Layer4Type = NetPacketLayer4TypeTcp;

        break;

    case Protocol::UDP:
        Layout.Layer4Type = NetPacketLayer4TypeUdp;

        break;
    }

    Offset += Layout.Layer3HeaderLength;
    Length -= Layout.Layer3HeaderLength;

    return true;
}

static
bool
ParseIPv6ExtensionLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer3Type != NetPacketLayer3TypeIPv4UnspecifiedOptions);
    NT_FRE_ASSERT(Layout.Layer3Type != NetPacketLayer3TypeIPv4WithOptions);
    NT_FRE_ASSERT(Layout.Layer3Type != NetPacketLayer3TypeIPv4NoOptions);
    NT_FRE_ASSERT(Layout.Layer3HeaderLength == 0U);

    auto o = Offset;
    auto l = Length;

    auto nextHeader = true;
    while (nextHeader && Layout.Layer4Type == NetPacketLayer4TypeUnspecified)
    {
        auto const extension = reinterpret_cast<IPv6Extension UNALIGNED const *>(&Pointer[o]);
        auto const size = sizeof(*extension);

        if (size > l)
        {
            return false;
        }

        size_t const headersz = size + extension->Header.Length * size;
        if (headersz > l)
        {
            return false;
        }

        o += headersz;
        l -= headersz;

        switch (extension->Header.NextHeader)
        {

        case NextHeader::TCP:
            Layout.Layer4Type = NetPacketLayer4TypeTcp;

            break;

        case NextHeader::UDP:
            Layout.Layer4Type = NetPacketLayer4TypeUdp;

            break;

        case NextHeader::NoNextHeader:
            nextHeader = false;

            break;

        }
    }

    Layout.Layer3Type = NetPacketLayer3TypeIPv6WithExtensions;

    Offset = o;
    Length = l;

    return true;
}

_Use_decl_annotations_
bool
ParseIPv6Layout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(
        Layout.Layer3Type == NetPacketLayer3TypeIPv6UnspecifiedExtensions ||
        Layout.Layer3Type == NetPacketLayer3TypeUnspecified);
    NT_FRE_ASSERT(Layout.Layer3HeaderLength == 0U);

    auto const ipv6 = reinterpret_cast<IPv6 UNALIGNED const *>(&Pointer[Offset]);
    auto const size = sizeof(*ipv6);
    if (size > Length)
    {
        return false;
    }

    if (ipv6->Header.VersionClassFlow.Version != IP_VERSION_6)
    {
        return false;
    }

    auto o = Offset + size;
    auto l = Length - size;

    switch (ipv6->Header.NextHeader)
    {

    case NextHeader::TCP:
        Layout.Layer3Type = NetPacketLayer3TypeIPv6NoExtensions;
        Layout.Layer4Type = NetPacketLayer4TypeTcp;

        break;

    case NextHeader::UDP:
        Layout.Layer3Type = NetPacketLayer3TypeIPv6NoExtensions;
        Layout.Layer4Type = NetPacketLayer4TypeUdp;

        break;

    case NextHeader::ICMPv4:
    case NextHeader::ICMPv6:
        Layout.Layer3Type = NetPacketLayer3TypeIPv6NoExtensions;

        break;

    case NextHeader::NoNextHeader:
        Layout.Layer3Type = NetPacketLayer3TypeIPv6NoExtensions;

        break;

    default:
        if (! ParseIPv6ExtensionLayout(Layout, Pointer, o, l))
        {
            return false;
        }

        break;

    }

    Layout.Layer3HeaderLength = o - Offset;

    Offset += Layout.Layer3HeaderLength;
    Length -= Layout.Layer3HeaderLength;

    return true;
}

}
