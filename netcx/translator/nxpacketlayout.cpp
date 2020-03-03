// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxPacketLayout.hpp"
#include "NxPacketLayout.tmh"

#include <net/ring.h>
#include <net/fragment.h>
#include <net/virtualaddress_p.h>
#include <netiodef.h>

#define IP_VERSION_4 4
#define IP_VERSION_6 6

_Success_(return)
bool
NxGetPacketEtherType(
    _In_ NET_RING_COLLECTION const * descriptor,
    _In_ NET_EXTENSION const & virtualAddressExtension,
    _In_ NET_PACKET const *packet,
    _Out_ USHORT *ethertype)
{
    auto fr = NetRingCollectionGetFragmentRing(descriptor);
    auto const fragment = NetRingGetFragmentAtIndex(fr, packet->FragmentIndex);
    auto const virtualAddress = NetExtensionGetFragmentVirtualAddress(
        &virtualAddressExtension, packet->FragmentIndex);
    auto buffer = (UCHAR const*)virtualAddress->VirtualAddress + fragment->Offset;
    auto bytesRemaining = (ULONG)fragment->ValidLength;

    if (bytesRemaining < sizeof(ETHERNET_HEADER))
        return false;

    auto ethernet = (ETHERNET_HEADER UNALIGNED const*)buffer;
    *ethertype = RtlUshortByteSwap(ethernet->Type);

    if (*ethertype >= ETHERNET_TYPE_MINIMUM)
    {
        return true;
    }
    else if (bytesRemaining >= sizeof(SNAP_HEADER))
    {
        auto snap = (SNAP_HEADER UNALIGNED const *)(ethernet + 1);
        if (snap->Control == SNAP_CONTROL &&
            snap->Dsap == SNAP_DSAP &&
            snap->Ssap == SNAP_SSAP &&
            snap->Oui[0] == SNAP_OUI &&
            snap->Oui[1] == SNAP_OUI &&
            snap->Oui[2] == SNAP_OUI)
        {
            *ethertype = RtlUshortByteSwap(snap->Type);
            return true;
        }
    }

    return false;
}

static
void
ParseEthernetHeader(
    _Outref_result_bytebuffer_(bytesRemaining) UCHAR const *&buffer,
    _Inout_ ULONG &bytesRemaining,
    _Out_ NET_PACKET_LAYOUT &layout)
{
    if (bytesRemaining < sizeof(ETHERNET_HEADER))
        return;

    auto ethernet = (ETHERNET_HEADER UNALIGNED const*)buffer;
    auto ethertype = RtlUshortByteSwap(ethernet->Type);

    if (ethertype >= ETHERNET_TYPE_MINIMUM)
    {
        layout.Layer2Type = NetPacketLayer2TypeEthernet;
        layout.Layer2HeaderLength = sizeof(ETHERNET_HEADER);
        buffer += sizeof(ETHERNET_HEADER);
        bytesRemaining -= sizeof(ETHERNET_HEADER);
    }
    else if (bytesRemaining >= sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER))
    {
        auto snap = (SNAP_HEADER UNALIGNED const *)(ethernet + 1);
        if (snap->Control == SNAP_CONTROL &&
            snap->Dsap == SNAP_DSAP &&
            snap->Ssap == SNAP_SSAP &&
            snap->Oui[0] == SNAP_OUI &&
            snap->Oui[1] == SNAP_OUI &&
            snap->Oui[2] == SNAP_OUI)
        {
            layout.Layer2Type = NetPacketLayer2TypeEthernet;
            layout.Layer2HeaderLength = sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER);
            buffer += sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER);
            bytesRemaining -= sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER);

            ethertype = RtlUshortByteSwap(snap->Type);
        }
        else
        {
            layout.Layer2Type = NetPacketLayer2TypeUnspecified;
            return;
        }
    }
    else
    {
        layout.Layer2Type = NetPacketLayer2TypeUnspecified;
        return;
    }

    switch (ethertype)
    {
    case ETHERNET_TYPE_IPV4:
        layout.Layer3Type = NetPacketLayer3TypeIPv4UnspecifiedOptions;
        break;
    case ETHERNET_TYPE_IPV6:
        layout.Layer3Type = NetPacketLayer3TypeIPv6UnspecifiedExtensions;
        break;
    }
}

static
void
ParseRawIPHeader(
    _Inout_ UCHAR const *&buffer,
    _Inout_ ULONG &bytesRemaining,
    _Out_ NET_PACKET_LAYOUT &layout)
{
    if (bytesRemaining < 1)
        return;

    layout.Layer2Type = NetPacketLayer2TypeNull;
    layout.Layer2HeaderLength = 0;

    switch (buffer[0] >> 4)
    {
    case IP_VERSION_4:
        layout.Layer3Type = NetPacketLayer3TypeIPv4UnspecifiedOptions;
        break;
    case IP_VERSION_6:
        layout.Layer3Type = NetPacketLayer3TypeIPv6UnspecifiedExtensions;
        break;
    }
}

static
NET_PACKET_LAYER4_TYPE
GetLayer4Type(
    _In_ ULONG protocolId)
{
    switch (protocolId)
    {
    case IPPROTO_TCP:
        return NetPacketLayer4TypeTcp;
    case IPPROTO_UDP:
        return NetPacketLayer4TypeUdp;
    default:
        return NetPacketLayer4TypeUnspecified;
    }
}

static
void
ParseIPv4Header(
    _Outref_result_bytebuffer_(bytesRemaining) UCHAR const *&buffer,
    _Inout_ ULONG &bytesRemaining,
    _Out_ NET_PACKET_LAYOUT &layout)
{
    if (bytesRemaining < sizeof(IPV4_HEADER))
    {
        layout.Layer3Type = NetPacketLayer3TypeUnspecified;
        return;
    }

    auto ip = (IPV4_HEADER UNALIGNED const*)buffer;
    auto length = Ip4HeaderLengthInBytes(ip);
    if (bytesRemaining < length || length > MAX_IPV4_HLEN)
    {
        layout.Layer3Type = NetPacketLayer3TypeUnspecified;
        return;
    }

    layout.Layer3Type = (length == sizeof(IPV4_HEADER))
        ? NetPacketLayer3TypeIPv4NoOptions
        : NetPacketLayer3TypeIPv4WithOptions;
    layout.Layer3HeaderLength = length;
    layout.Layer4Type = GetLayer4Type(ip->Protocol);
    buffer += length;
    bytesRemaining -= length;
}

enum class IPv6ExtensionParseResult
{
    Ok,
    UnknownExtension,
    MalformedExtension,
};

static
IPv6ExtensionParseResult
ParseIPv6ExtensionHeader(
    _In_ ULONG headerType,
    _In_reads_bytes_(bytesRemaining) UCHAR const *buffer,
    _In_ ULONG bytesRemaining,
    _Out_ ULONG *length,
    _Out_ ULONG *nextHeaderType)
{
    switch (headerType)
    {
    case IPPROTO_HOPOPTS:
    case IPPROTO_DSTOPTS:
    case IPPROTO_ROUTING:
    {
        auto extension = (IPV6_EXTENSION_HEADER UNALIGNED const*)buffer;

        if (sizeof(IPV6_EXTENSION_HEADER) > bytesRemaining ||
            (ULONG)IPV6_EXTENSION_HEADER_LENGTH(extension->Length) > bytesRemaining)
        {
            return IPv6ExtensionParseResult::MalformedExtension;
        }

        *nextHeaderType = extension->NextHeader;
        *length = IPV6_EXTENSION_HEADER_LENGTH(extension->Length);
        return IPv6ExtensionParseResult::Ok;
    }

    case IPPROTO_AH:
    {
        auto extension = (IPV6_EXTENSION_HEADER UNALIGNED const*)buffer;

        if (sizeof(IPV6_EXTENSION_HEADER) > bytesRemaining ||
            (ULONG)IP_AUTHENTICATION_HEADER_LENGTH(extension->Length) > bytesRemaining)
        {
            return IPv6ExtensionParseResult::MalformedExtension;
        }

        *nextHeaderType = extension->NextHeader;
        *length = IP_AUTHENTICATION_HEADER_LENGTH(extension->Length);
        return IPv6ExtensionParseResult::Ok;
    }

    case IPPROTO_FRAGMENT:
    {
        auto extension = (IPV6_FRAGMENT_HEADER UNALIGNED const*)buffer;

        if (sizeof(IPV6_FRAGMENT_HEADER) > bytesRemaining)
        {
            return IPv6ExtensionParseResult::MalformedExtension;
        }

        *nextHeaderType = extension->NextHeader;
        *length = sizeof(IPV6_FRAGMENT_HEADER);
        return IPv6ExtensionParseResult::Ok;
    }
    }

    return IPv6ExtensionParseResult::UnknownExtension;
}

static
void
ParseIPv6Header(
    _Outref_result_bytebuffer_(bytesRemaining) UCHAR const *&buffer,
    _Inout_ ULONG &bytesRemaining,
    _Out_ NET_PACKET_LAYOUT &layout)
{
    if (bytesRemaining < sizeof(IPV6_HEADER))
    {
        layout.Layer3Type = NetPacketLayer3TypeUnspecified;
        return;
    }

    auto ip = (IPV6_HEADER UNALIGNED const*)buffer;
    auto nextHeader = (ULONG)ip->NextHeader;

    auto offset = (ULONG)sizeof(IPV6_HEADER);

    while (true)
    {
        ULONG extensionLength;
        switch (ParseIPv6ExtensionHeader(nextHeader, buffer + offset, bytesRemaining - offset, &extensionLength, &nextHeader))
        {
        case IPv6ExtensionParseResult::MalformedExtension:
            layout.Layer3Type = NetPacketLayer3TypeUnspecified;
            return;

        case IPv6ExtensionParseResult::Ok:
            offset += extensionLength;
            break;

        case IPv6ExtensionParseResult::UnknownExtension:
            if (offset > 0x1ff)
            {
                layout.Layer3Type = NetPacketLayer3TypeUnspecified;
                return;
            }
            else if (offset == sizeof(IPV6_HEADER))
            {
                layout.Layer3Type = NetPacketLayer3TypeIPv6NoExtensions;
            }
            else
            {
                layout.Layer3Type = NetPacketLayer3TypeIPv6WithExtensions;
            }

            layout.Layer3HeaderLength = offset;
            layout.Layer4Type = GetLayer4Type(nextHeader);
            buffer += offset;
            bytesRemaining -= offset;
            return;
        }
    }
}

static
void
ParseTcpHeader(
    _Outref_result_bytebuffer_(bytesRemaining) UCHAR const *&buffer,
    _Inout_ ULONG &bytesRemaining,
    _Out_ NET_PACKET_LAYOUT &layout)
{
    if (bytesRemaining < sizeof(TCP_HDR))
    {
        layout.Layer4Type = NetPacketLayer4TypeUnspecified;
        return;
    }

    auto tcp = (TCP_HDR UNALIGNED const *)buffer;
    auto length = (ULONG)tcp->th_len * 4;
    if (bytesRemaining < length || length > TH_MAX_LEN)
    {
        layout.Layer4Type = NetPacketLayer4TypeUnspecified;
        return;
    }

    layout.Layer4HeaderLength = length;
    buffer += length;
    bytesRemaining -= length;
}

static
void
ParseUdpHeader(
    _Inout_ UCHAR const *&buffer,
    _Inout_ ULONG &bytesRemaining,
    _Out_ NET_PACKET_LAYOUT &layout)
{
    const auto UDP_HEADER_SIZE = 8;

    if (bytesRemaining < UDP_HEADER_SIZE)
    {
        layout.Layer4Type = NetPacketLayer4TypeUnspecified;
        return;
    }

    layout.Layer4HeaderLength = UDP_HEADER_SIZE;
    buffer += UDP_HEADER_SIZE;
    bytesRemaining -= UDP_HEADER_SIZE;
}

NET_PACKET_LAYOUT
NxGetPacketLayout(
    _In_ NDIS_MEDIUM mediaType,
    _In_ NET_RING_COLLECTION const * descriptor,
    _In_ NET_EXTENSION const & virtualAddressExtension,
    _In_ NET_PACKET const *packet,
    _In_ size_t PayloadBackfill)
{
    NT_ASSERT(packet->FragmentCount != 0);

    auto fr = NetRingCollectionGetFragmentRing(descriptor);
    auto const fragment = NetRingGetFragmentAtIndex(fr, packet->FragmentIndex);
    auto const virtualAddress = NetExtensionGetFragmentVirtualAddress(
        &virtualAddressExtension, packet->FragmentIndex);

    // Skip the backfill space
    auto buffer = (UCHAR const*)virtualAddress->VirtualAddress + fragment->Offset + PayloadBackfill;
    auto bytesRemaining = (ULONG)fragment->ValidLength - (ULONG)PayloadBackfill;

    NET_PACKET_LAYOUT layout = { };

    switch (mediaType)
    {
    case NdisMedium802_3:
        ParseEthernetHeader(buffer, bytesRemaining, layout);
        break;
    case NdisMediumIP:
    case NdisMediumWiMAX:
    case NdisMediumWirelessWan:
        ParseRawIPHeader(buffer, bytesRemaining, layout);
        break;
    }

    switch (layout.Layer3Type)
    {
    case NetPacketLayer3TypeIPv4UnspecifiedOptions:
        ParseIPv4Header(buffer, bytesRemaining, layout);
        break;
    case NetPacketLayer3TypeIPv6UnspecifiedExtensions:
        ParseIPv6Header(buffer, bytesRemaining, layout);
        break;
    }

    switch (layout.Layer4Type)
    {
    case NetPacketLayer4TypeTcp:
        ParseTcpHeader(buffer, bytesRemaining, layout);
        break;
    case NetPacketLayer4TypeUdp:
        ParseUdpHeader(buffer, bytesRemaining, layout);
        break;
    }

    return layout;
}
