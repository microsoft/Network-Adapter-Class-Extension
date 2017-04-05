// Copyright (C) Microsoft Corporation. All rights reserved.
#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxPacketLayout.tmh"
#include "NxPacketLayout.hpp"
#include "NxHeaderParser.hpp"
#include "KPacket.h"

NET_PACKET_LAYER4_TYPE
NxGetLayer4Type(KIPProtocolNumber number)
{
    switch (number)
    {
    case KIPProtocolNumber::TCP:
        return NET_PACKET_LAYER4_TYPE_TCP;
    case KIPProtocolNumber::UDP:
        return NET_PACKET_LAYER4_TYPE_UDP;
    default:
        return NET_PACKET_LAYER4_TYPE_UNSPECIFIED;
    }
}

NxLayer2Layout
NxGetEthernetLayout(NxPacketHeaderParser & parser)
{
    auto const ethernetSnapHeader = parser.TryTakeHeader<KEthernetSnapHeader>();
    UINT16 headerSize = 0;

    KEthernetHeader UNALIGNED *ethernetHeader = nullptr;
    if (ethernetSnapHeader)
    {
        if (ethernetSnapHeader->is_WellFormed())
        {
            ethernetHeader = ethernetSnapHeader;
            headerSize = sizeof(KEthernetSnapHeader);
        }
        else
        {
            // return the unqualifying snap header
            parser.Retreat(sizeof(KEthernetSnapHeader));
        }
    }

    if (!ethernetHeader)
    {
        ethernetHeader = parser.TryTakeHeader<KEthernetHeader>();
        headerSize = sizeof(KEthernetHeader);
    }

    if (!ethernetHeader)
    {
        return{};
    }

    NET_PACKET_LAYER3_TYPE layer3Type = NET_PACKET_LAYER3_TYPE_UNSPECIFIED;
    switch (ethernetHeader->TypeLength.get())
    {
    case ETHERNET_TYPE_IPV4:
        layer3Type = NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS;
        break;
    case ETHERNET_TYPE_IPV6:
        layer3Type = NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS;
        break;
    }

    return{ NET_PACKET_LAYER2_TYPE_ETHERNET, headerSize, layer3Type };
}

NxLayer3Layout
NxGetIPv4Layout(NxPacketHeaderParser & parser)
{
    auto const ipv4Header = parser.TryTakeHeader<KIPv4Header>();
    if (!(ipv4Header && ipv4Header->is_WellFormed()))
    {
        return{};
    }

    auto const ipv4HeaderLength = ipv4Header->get_HeaderSize();

    // The ENTIRE header must be contiguous, including the options,
    // so return the header, and then advance again.
    if (ipv4HeaderLength > sizeof(KIPv4Header))
    {
        parser.Retreat(sizeof(KIPv4Header));
        if (!parser.TryGetContiguousBuffer(ipv4HeaderLength))
        {
            return{};
        }
    }

    auto const layer3Type =
        ipv4Header->has_Options()
        ? NET_PACKET_LAYER3_TYPE_IPV4_WITH_OPTIONS
        : NET_PACKET_LAYER3_TYPE_IPV4_NO_OPTIONS;

    return{layer3Type, ipv4HeaderLength, NxGetLayer4Type(ipv4Header->Protocol)};
}

NxLayer3Layout
NxGetIPv6Layout(NxPacketHeaderParser & parser)
{
    auto const ipv6Header = parser.TryTakeHeader<KIPv6Header>();
    if (!ipv6Header)
    {
        return{};
    }

    UINT16 ipv6HeaderLength = sizeof(KIPv6Header);
    auto extensionType = ipv6Header->NextHeader;

    while (IsProtocolIPv6Extension(extensionType))
    {
        auto header = parser.TryTakeHeader<KIPv6ExtensionHeader>(KIPv6ExtensionHeader::get_HeaderSize());
        if (!header)
        {
            return{};
        }

        UINT8 extSize;
        if (!header->tryget_ExtensionSize(extensionType, &extSize))
        {
            return{};
        }

        // The ENTIRE header must be contiguous, including the extensions,
        // so return the header, and then advance again.
        parser.Retreat(ipv6HeaderLength + KIPv6ExtensionHeader::get_HeaderSize());

        ipv6HeaderLength += extSize;
        extensionType = header->NextHeader;

        if (!parser.TryGetContiguousBuffer(ipv6HeaderLength))
        {
            return{};
        }
    }

    auto const layer3Type =
        ipv6HeaderLength > sizeof(KIPv6Header)
        ? NET_PACKET_LAYER3_TYPE_IPV6_WITH_EXTENSIONS
        : NET_PACKET_LAYER3_TYPE_IPV6_NO_EXTENSIONS;

    return{ layer3Type, ipv6HeaderLength, NxGetLayer4Type(extensionType) };
}

NxLayer4Layout
NxGetTcpLayout(NxPacketHeaderParser & parser)
{
    auto const tcpHeader = parser.TryTakeHeader<KTcpHeader>();
    if (!(tcpHeader && tcpHeader->is_WellFormed()))
    {
        return{};
    }

    return{ NET_PACKET_LAYER4_TYPE_TCP, tcpHeader->get_HeaderSize() };
}

NxLayer4Layout
NxGetUdpLayout(NxPacketHeaderParser & parser)
{
    UNREFERENCED_PARAMETER(parser);
    if (!parser.TryTakeHeader<KUdpHeader>())
    {
        return{};
    }

    return{ NET_PACKET_LAYER4_TYPE_UDP, KUdpHeader::get_HeaderSize() };
}

NxLayer2Layout
NxGetLayer2Layout(NDIS_MEDIUM mediaType, NxPacketHeaderParser & parser)
{
    switch (mediaType)
    {
    case NdisMedium802_3:
        return NxGetEthernetLayout(parser);
    default:
        return{};
    }
}

NxLayer3Layout
NxGetLayer3Layout(NET_PACKET_LAYER3_TYPE type, NxPacketHeaderParser & parser)
{
    switch (type)
    {
    case NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS:
        return NxGetIPv4Layout(parser);
    case NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS:
        return NxGetIPv6Layout(parser);
    default:
        return{};
    }
}

NxLayer4Layout
NxGetLayer4Layout(NET_PACKET_LAYER4_TYPE type, NxPacketHeaderParser & parser)
{
    switch (type)
    {
    case NET_PACKET_LAYER4_TYPE_TCP:
        return NxGetTcpLayout(parser);
    case NET_PACKET_LAYER4_TYPE_UDP:
        return NxGetUdpLayout(parser);
    default:
        return{};
    }
}

NET_PACKET_LAYOUT
NxGetPacketLayout(NDIS_MEDIUM mediaType, NET_PACKET const & packet)
{
    NET_PACKET_LAYOUT layout = { 0 };

    NxPacketHeaderParser parser(packet);

    auto const l2Layout = NxGetLayer2Layout(mediaType, parser);

    if (l2Layout.Type == NET_PACKET_LAYER2_TYPE_UNSPECIFIED)
    {
        return layout;
    }

    layout.Layer2Type = l2Layout.Type;
    layout.Layer2HeaderLength = l2Layout.Length;

    auto const l3Layout = NxGetLayer3Layout(l2Layout.L3Type, parser);

    if (l3Layout.Type == NET_PACKET_LAYER3_TYPE_UNSPECIFIED)
    {
        return layout;
    }

    layout.Layer3Type = l3Layout.Type;
    layout.Layer3HeaderLength = l3Layout.Length;

    auto const l4Layout = NxGetLayer4Layout(l3Layout.L4Type, parser);

    if (l4Layout.Type == NET_PACKET_LAYER4_TYPE_UNSPECIFIED)
    {
        return layout;
    }

    layout.Layer4Type = l4Layout.Type;
    layout.Layer4HeaderLength = l4Layout.Length;

    return layout;
}
