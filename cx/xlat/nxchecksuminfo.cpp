// Copyright (C) Microsoft Corporation. All rights reserved.
#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxChecksumInfo.tmh"
#include "NxChecksumInfo.hpp"

static bool IsIPv4(NET_PACKET_LAYOUT const &layout)
{
    return
        layout.Layer3Type >= NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS &&
        layout.Layer3Type <= NET_PACKET_LAYER3_TYPE_IPV4_NO_OPTIONS;
}

static bool IsIPv6(NET_PACKET_LAYOUT const &layout)
{
    return
        layout.Layer3Type >= NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS &&
        layout.Layer3Type <= NET_PACKET_LAYER3_TYPE_IPV6_NO_EXTENSIONS;
}

NET_PACKET_CHECKSUM
NxTranslateTxPacketChecksum(
    NET_PACKET const & packet,
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const & info)
{
    NET_PACKET_CHECKSUM checksum = {};

    if (info.Transmit.IpHeaderChecksum)
    {
        if (info.Transmit.IsIPv4 != info.Transmit.IsIPv6)
        {
            if ((info.Transmit.IsIPv4 && IsIPv4(packet.Layout)) || (info.Transmit.IsIPv6 && IsIPv6(packet.Layout)))
            {
                checksum.Layer3 = NET_PACKET_TX_CHECKSUM_REQUIRED;
            }
        }
    }

    if (info.Transmit.TcpChecksum && packet.Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP)
    {
        checksum.Layer4 = NET_PACKET_TX_CHECKSUM_REQUIRED;
    }
    else if (info.Transmit.UdpChecksum && packet.Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_UDP)
    {
        checksum.Layer4 = NET_PACKET_TX_CHECKSUM_REQUIRED;
    }

    return checksum;
}

NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO
NxTranslateRxPacketChecksum(NET_PACKET const & packet)
{
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO checksumInfo = {};

    if (packet.Checksum.Layer3 == NET_PACKET_RX_CHECKSUM_VALID)
    {
        checksumInfo.Receive.IpChecksumSucceeded = true;
    }
    else if (packet.Checksum.Layer3 == NET_PACKET_RX_CHECKSUM_INVALID)
    {
        checksumInfo.Receive.IpChecksumFailed = true;
    }

    if (packet.Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP)
    {
        if (packet.Checksum.Layer4 == NET_PACKET_RX_CHECKSUM_VALID)
        {
            checksumInfo.Receive.TcpChecksumSucceeded = true;
        }
        else if (packet.Checksum.Layer4 == NET_PACKET_RX_CHECKSUM_INVALID)
        {
            checksumInfo.Receive.TcpChecksumFailed = true;
        }
    }
    else if (packet.Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_UDP)
    {
        if (packet.Checksum.Layer4 == NET_PACKET_RX_CHECKSUM_VALID)
        {
            checksumInfo.Receive.UdpChecksumSucceeded = true;
        }
        else if (packet.Checksum.Layer4 == NET_PACKET_RX_CHECKSUM_INVALID)
        {
            checksumInfo.Receive.UdpChecksumFailed = true;
        }
    }

    return checksumInfo;
}
