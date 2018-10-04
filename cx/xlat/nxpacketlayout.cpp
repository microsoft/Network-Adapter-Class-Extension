// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxPacketLayout.hpp"
#include "NxPacketLayout.tmh"

#define IP_VERSION_4 4
#define IP_VERSION_6 6

_Success_(return)
bool
NxGetPacketEtherType(
    _In_ NET_DATAPATH_DESCRIPTOR const *descriptor,
    _In_ NET_PACKET const *packet,
    _Out_ USHORT *ethertype)
{
    auto fragment = NET_PACKET_GET_FRAGMENT(packet, descriptor, 0);
    auto buffer = (UCHAR const*)fragment->VirtualAddress + fragment->Offset;
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
        layout.Layer2Type = NET_PACKET_LAYER2_TYPE_ETHERNET;
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
            layout.Layer2Type = NET_PACKET_LAYER2_TYPE_ETHERNET;
            layout.Layer2HeaderLength = sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER);
            buffer += sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER);
            bytesRemaining -= sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER);

            ethertype = RtlUshortByteSwap(snap->Type);
        }
        else
        {
            layout.Layer2Type = NET_PACKET_LAYER2_TYPE_UNSPECIFIED;
            return;
        }
    }
    else
    {
        layout.Layer2Type = NET_PACKET_LAYER2_TYPE_UNSPECIFIED;
        return;
    }

    switch (ethertype)
    {
    case ETHERNET_TYPE_IPV4:
        layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS;
        break;
    case ETHERNET_TYPE_IPV6:
        layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS;
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

    layout.Layer2Type = NET_PACKET_LAYER2_TYPE_NULL;
    layout.Layer2HeaderLength = 0;

    switch (buffer[0] >> 4)
    {
    case IP_VERSION_4:
        layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS;
        break;
    case IP_VERSION_6:
        layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS;
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
        return NET_PACKET_LAYER4_TYPE_TCP;
    case IPPROTO_UDP:
        return NET_PACKET_LAYER4_TYPE_UDP;
    default:
        return NET_PACKET_LAYER4_TYPE_UNSPECIFIED;
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
        layout.Layer3Type = NET_PACKET_LAYER3_TYPE_UNSPECIFIED;
        return;
    }

    auto ip = (IPV4_HEADER UNALIGNED const*)buffer;
    auto length = Ip4HeaderLengthInBytes(ip);
    if (bytesRemaining < length || length > MAX_IPV4_HLEN)
    {
        layout.Layer3Type = NET_PACKET_LAYER3_TYPE_UNSPECIFIED;
        return;
    }

    layout.Layer3Type = (length == sizeof(IPV4_HEADER))
        ? NET_PACKET_LAYER3_TYPE_IPV4_NO_OPTIONS
        : NET_PACKET_LAYER3_TYPE_IPV4_WITH_OPTIONS;
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
        layout.Layer3Type = NET_PACKET_LAYER3_TYPE_UNSPECIFIED;
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
            layout.Layer3Type = NET_PACKET_LAYER3_TYPE_UNSPECIFIED;
            return;

        case IPv6ExtensionParseResult::Ok:
            offset += extensionLength;
            break;

        case IPv6ExtensionParseResult::UnknownExtension:
            if (offset > 0x1ff)
            {
                layout.Layer3Type = NET_PACKET_LAYER3_TYPE_UNSPECIFIED;
                return;
            }
            else if (offset == sizeof(IPV6_HEADER))
            {
                layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV6_NO_EXTENSIONS;
            }
            else
            {
                layout.Layer3Type = NET_PACKET_LAYER3_TYPE_IPV6_WITH_EXTENSIONS;
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
        layout.Layer4Type = NET_PACKET_LAYER4_TYPE_UNSPECIFIED;
        return;
    }

    auto tcp = (TCP_HDR UNALIGNED const *)buffer;
    auto length = (ULONG)tcp->th_len * 4;
    if (bytesRemaining < length || length > TH_MAX_LEN)
    {
        layout.Layer4Type = NET_PACKET_LAYER4_TYPE_UNSPECIFIED;
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
        layout.Layer4Type = NET_PACKET_LAYER4_TYPE_UNSPECIFIED;
        return;
    }

    layout.Layer4HeaderLength = UDP_HEADER_SIZE;
    buffer += UDP_HEADER_SIZE;
    bytesRemaining -= UDP_HEADER_SIZE;
}

NET_PACKET_LAYOUT
NxGetPacketLayout(
    _In_ NDIS_MEDIUM mediaType,
    _In_ NET_DATAPATH_DESCRIPTOR const * descriptor,
    _In_ NET_PACKET const *packet)
{
    NT_ASSERT(packet->FragmentCount != 0);

    auto fragment = NET_PACKET_GET_FRAGMENT(packet, descriptor, 0);
    auto buffer = (UCHAR const*)fragment->VirtualAddress + fragment->Offset;
    auto bytesRemaining = (ULONG)fragment->ValidLength;

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
    case NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS:
        ParseIPv4Header(buffer, bytesRemaining, layout);
        break;
    case NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS:
        ParseIPv6Header(buffer, bytesRemaining, layout);
        break;
    }

    switch (layout.Layer4Type)
    {
    case NET_PACKET_LAYER4_TYPE_TCP:
        ParseTcpHeader(buffer, bytesRemaining, layout);
        break;
    case NET_PACKET_LAYER4_TYPE_UDP:
        ParseUdpHeader(buffer, bytesRemaining, layout);
        break;
    }

    return layout;
}
