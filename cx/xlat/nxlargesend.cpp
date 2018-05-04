// Copyright (C) Microsoft Corporation. All rights reserved.
#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxLargeSend.tmh"

#include "NxLargeSend.hpp"


NET_PACKET_LARGE_SEND_SEGMENTATION
NxTranslateTxPacketLargeSendSegmentation(
    NET_PACKET const & packet,
    NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO const & info
    )
{
    ASSERT(packet.Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP);
    const USHORT layer4HeaderOffset =
        packet.Layout.Layer2HeaderLength +
        packet.Layout.Layer3HeaderLength;

    NET_PACKET_LARGE_SEND_SEGMENTATION lso = {};
    if (info.Value != 0)
    {
        if (info.Transmit.Type == NDIS_TCP_LARGE_SEND_OFFLOAD_V1_TYPE)
        {
            ASSERT(info.LsoV1Transmit.TcpHeaderOffset == layer4HeaderOffset);
            ASSERT(NetPacketIsIpv4(&packet));

            lso.TCP.Mss = info.LsoV1Transmit.MSS;
        }
        else if (info.Transmit.Type == NDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE)
        {
            ASSERT(info.LsoV2Transmit.TcpHeaderOffset == layer4HeaderOffset);
            if (info.LsoV2Transmit.IPVersion == NDIS_TCP_LARGE_SEND_OFFLOAD_IPv4)
            {
                ASSERT(NetPacketIsIpv4(&packet));
            }
            else if (info.LsoV2Transmit.IPVersion == NDIS_TCP_LARGE_SEND_OFFLOAD_IPv6)
            {
                ASSERT(NetPacketIsIpv6(&packet));
            }

            lso.TCP.Mss = info.LsoV2Transmit.MSS;
        }
    }

    return lso;
}
