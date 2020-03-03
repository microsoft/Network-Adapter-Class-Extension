// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxChecksumInfo.tmh"
#include "NxChecksumInfo.hpp"

#include <net/checksum.h>

static
bool
IsIPv4(
    NET_PACKET_LAYOUT const &layout
)
{
    return
        layout.Layer3Type >= NetPacketLayer3TypeIPv4UnspecifiedOptions &&
        layout.Layer3Type <= NetPacketLayer3TypeIPv4NoOptions;
}

static
bool
IsIPv6(
    NET_PACKET_LAYOUT const &layout
)
{
    return
        layout.Layer3Type >= NetPacketLayer3TypeIPv6UnspecifiedExtensions &&
        layout.Layer3Type <= NetPacketLayer3TypeIPv6NoExtensions;
}

NET_PACKET_CHECKSUM
NxTranslateTxPacketChecksum(
    NET_PACKET const & packet,
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const & info
)
{
    NET_PACKET_CHECKSUM checksum = {};

    if (info.Transmit.IpHeaderChecksum)
    {
        if (info.Transmit.IsIPv4 != info.Transmit.IsIPv6)
        {
            if ((info.Transmit.IsIPv4 && IsIPv4(packet.Layout)) || (info.Transmit.IsIPv6 && IsIPv6(packet.Layout)))
            {
                checksum.Layer3 = NetPacketTxChecksumActionRequired;
            }
        }
    }

    if (info.Transmit.TcpChecksum && packet.Layout.Layer4Type == NetPacketLayer4TypeTcp)
    {
        checksum.Layer4 = NetPacketTxChecksumActionRequired;
    }
    else if (info.Transmit.UdpChecksum && packet.Layout.Layer4Type == NetPacketLayer4TypeUdp)
    {
        checksum.Layer4 = NetPacketTxChecksumActionRequired;
    }

    return checksum;
}

NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO
NxTranslateRxPacketChecksum(
    NET_PACKET const* packet,
    NET_EXTENSION const* checksumExtension,
    UINT32 packetIndex
)
{
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO checksumInfo = {};
    auto const checksumExt = NetExtensionGetPacketChecksum(checksumExtension, packetIndex);

    if (checksumExt->Layer3 == NetPacketRxChecksumEvaluationValid)
    {
        checksumInfo.Receive.IpChecksumSucceeded = true;
    }
    else if (checksumExt->Layer3 == NetPacketRxChecksumEvaluationInvalid)
    {
        checksumInfo.Receive.IpChecksumFailed = true;
    }

    if (packet->Layout.Layer4Type == NetPacketLayer4TypeTcp)
    {
        if (checksumExt->Layer4 == NetPacketRxChecksumEvaluationValid)
        {
            checksumInfo.Receive.TcpChecksumSucceeded = true;
        }
        else if (checksumExt->Layer4 == NetPacketRxChecksumEvaluationInvalid)
        {
            checksumInfo.Receive.TcpChecksumFailed = true;
        }
    }
    else if (packet->Layout.Layer4Type == NetPacketLayer4TypeUdp)
    {
        if (checksumExt->Layer4 == NetPacketRxChecksumEvaluationValid)
        {
            checksumInfo.Receive.UdpChecksumSucceeded = true;
        }
        else if (checksumExt->Layer4 == NetPacketRxChecksumEvaluationInvalid)
        {
            checksumInfo.Receive.UdpChecksumFailed = true;
        }
    }

    return checksumInfo;
}
