// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

#include <ntassert.h>

#include "Layer2Layout.hpp"

EXTERN_C_START

unsigned short __cdecl _byteswap_ushort(unsigned short);
unsigned long __cdecl _byteswap_ulong(unsigned long);

EXTERN_C_END

#pragma intrinsic(_byteswap_ushort)
#pragma intrinsic(_byteswap_ulong)

#define SNAP_DSAP 0xaa
#define SNAP_SSAP 0xaa
#define SNAP_CONTROL 0x03
#define SNAP_OUI 0x00
#define ETHERNET_TYPE_MINIMUM 0x0600

namespace Layer2
{

enum class Ieee80211FrameType : UINT8
{
    Data = 2,
};

_Use_decl_annotations_
bool
ParseLayout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer2Type != NetPacketLayer2TypeUnspecified);
    NT_FRE_ASSERT(Layout.Layer2HeaderLength == 0U);

    switch (Layout.Layer2Type)
    {

    case NetPacketLayer2TypeEthernet:

        return ParseEthernetLayout(Layout, Pointer, Offset, Length);

    case NetPacketLayer2TypeIeee80211:

        return ParseIeee80211Layout(Layout, Pointer, Offset, Length);

    case NetPacketLayer2TypeNull:

        return ParseNullLayout(Layout, Pointer, Offset, Length);

    }

    return false;
}

_Use_decl_annotations_
bool
ParseNullLayout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    UNREFERENCED_PARAMETER((Pointer, Offset, Length));

    NT_FRE_ASSERT(Layout.Layer2Type == NetPacketLayer2TypeNull);

    Layout.Layer2HeaderLength = 0U;

    return true;
}

_Use_decl_annotations_
bool
ParseEthernetLayout(
    NET_PACKET_LAYOUT & Layout,
    UINT8 const * Pointer,
    size_t & Offset,
    size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer2Type == NetPacketLayer2TypeEthernet);
    NT_FRE_ASSERT(Layout.Layer2HeaderLength == 0U);

    auto const ethernet = reinterpret_cast<Ethernet UNALIGNED const *>(&Pointer[Offset]);
    auto size = sizeof(*ethernet);
    if (size > Length)
    {
        return false;
    }

    auto etherType = _byteswap_ushort(ethernet->Header.Type);

    if (etherType < ETHERNET_TYPE_MINIMUM)
    {
        auto const snap = reinterpret_cast<Ieee8022Llc UNALIGNED const *>(ethernet + 1);
        size += sizeof(*snap);

        if (size > Length ||
            snap->Header.Control != SNAP_CONTROL ||
            snap->Header.DestinationServiceAccessPoint != SNAP_DSAP ||
            snap->Header.SourceServiceAccessPoint != SNAP_SSAP ||
            snap->Header.Oui[0] != SNAP_OUI ||
            snap->Header.Oui[0] != SNAP_OUI ||
            snap->Header.Oui[0] != SNAP_OUI)
        {
            return false;
        }

        etherType = _byteswap_ushort(snap->Header.Type);
    }

    Layout.Layer2HeaderLength = size;
    switch (etherType)
    {

    case EthernetType::IPv4:
        Layout.Layer3Type = NetPacketLayer3TypeIPv4UnspecifiedOptions;

        break;

    case EthernetType::IPv6:
        Layout.Layer3Type = NetPacketLayer3TypeIPv6UnspecifiedExtensions;

        break;

    }

    Offset += Layout.Layer2HeaderLength;
    Length -= Layout.Layer2HeaderLength;

    return true;
}

bool
ParseIeee80211Layout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer2Type == NetPacketLayer2TypeIeee80211);
    NT_FRE_ASSERT(Layout.Layer2HeaderLength == 0U);

    auto const ieee80211 = reinterpret_cast<Ieee80211 UNALIGNED const *>(&Pointer[Offset]);
    auto size = sizeof(*ieee80211);
    if (size > Length)
    {
        return false;
    }

    switch (ieee80211->Header.Type)
    {

    case Ieee80211FrameType::Data:

        if (ieee80211->Header.FromDs && ieee80211->Header.ToDs)
        {
            size += DOT11_DATA_ADDRESS4_SIZE;
        }

        if (ieee80211->Header.SubType & DOT11_DATA_SUBTYPE_QOS_FLAG)
        {
            size += DOT11_DATA_QOS_CONTROL_SIZE;
        }

        break;

    }

    if (size > Length)
    {
        return false;
    }

    auto o = Offset + size;
    auto l = Length - size;

    switch (ieee80211->Header.Type)
    {

    case Ieee80211FrameType::Data:

        if (! ParseIeee8022LlcLayout(Layout, Pointer, o, l))
        {
            return false;
        }

        break;

    }

    Layout.Layer2HeaderLength = o - Offset;

    Offset += Layout.Layer2HeaderLength;
    Length -= Layout.Layer2HeaderLength;

    return true;
}

bool
ParseIeee8022LlcLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
)
{
    NT_FRE_ASSERT(Layout.Layer2HeaderLength == 0U);

    auto o = Offset;
    auto l = Length;

    auto const llc = reinterpret_cast<Ieee8022Llc UNALIGNED const *>(&Pointer[Offset]);
    auto const size = sizeof(*llc);
    if (size > Length)
    {
        return false;
    }

    o += size;
    l -= size;

    Layout.Layer2HeaderLength = size;
    switch (_byteswap_ushort(llc->Header.Type))
    {

    case EthernetType::IPv4:
        Layout.Layer3Type = NetPacketLayer3TypeIPv4UnspecifiedOptions;

        break;

    case EthernetType::IPv6:
        Layout.Layer3Type = NetPacketLayer3TypeIPv6UnspecifiedExtensions;

        break;

    }

    Offset = o;
    Length = l;

    return true;
}

}
