// Copyright (C) Microsoft Corporation. All rights reserved.

#ifdef _KERNEL_MODE
#include <ntddk.h>
#include <ntassert.h>
#include <wdf.h>
#include <preview/netadaptercx.h>
#endif

#include <ndis.h>
#include <netiodef.h>

#include "MetadataTranslator.hpp"

#include <net/ring.h>
#include <net/fragment.h>
#include <net/checksum.h>
#include <net/rsc.h>
#include <net/lso.h>
#include <net/virtualaddress.h>
#include <net/wifi/exemptionaction.h>

_Use_decl_annotations_
RxMetadataTranslator::RxMetadataTranslator(
    NET_RING_COLLECTION const & Rings,
    RxExtensions const & Extensions
) :
    m_rings(Rings),
    m_extensions(Extensions)
{
}

_Use_decl_annotations_
void
RxMetadataTranslator::TranslatePacketChecksum(
    NET_BUFFER_LIST & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Checksum.Enabled)
    {
        return;
    }

    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO info = {};
    auto const checksum = NetExtensionGetPacketChecksum(&m_extensions.Extension.Checksum, PacketIndex);

    if (checksum->Layer3 == NetPacketRxChecksumEvaluationValid)
    {
        info.Receive.IpChecksumSucceeded = true;
    }
    else if (checksum->Layer3 == NetPacketRxChecksumEvaluationInvalid)
    {
        info.Receive.IpChecksumFailed = true;
    }

    auto const packet = NetRingGetPacketAtIndex(NetRingCollectionGetPacketRing(&m_rings), PacketIndex);

    if (packet->Layout.Layer4Type == NetPacketLayer4TypeTcp)
    {
        if (checksum->Layer4 == NetPacketRxChecksumEvaluationValid)
        {
            info.Receive.TcpChecksumSucceeded = true;
        }
        else if (checksum->Layer4 == NetPacketRxChecksumEvaluationInvalid)
        {
            info.Receive.TcpChecksumFailed = true;
        }
    }
    else if (packet->Layout.Layer4Type == NetPacketLayer4TypeUdp)
    {
        if (checksum->Layer4 == NetPacketRxChecksumEvaluationValid)
        {
            info.Receive.UdpChecksumSucceeded = true;
        }
        else if (checksum->Layer4 == NetPacketRxChecksumEvaluationInvalid)
        {
            info.Receive.UdpChecksumFailed = true;
        }
    }

    NetBufferList.NetBufferListInfo[TcpIpChecksumNetBufferListInfo] = info.Value;
}

_Use_decl_annotations_
void
RxMetadataTranslator::TranslatePacketRsc(
    NET_BUFFER_LIST & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Rsc.Enabled)
    {
        return;
    }

    auto const packet = NetRingGetPacketAtIndex(NetRingCollectionGetPacketRing(&m_rings), PacketIndex);

    // We currently support RSC only for TCP
    if (packet->Layout.Layer4Type != NetPacketLayer4TypeTcp)
    {
        return;
    }

    auto const rsc = NetExtensionGetPacketRsc(&m_extensions.Extension.Rsc, PacketIndex);
    NDIS_RSC_NBL_INFO info = {};

    if (rsc->TCP.CoalescedSegmentCount > 0)
    {
        info.Info.CoalescedSegCount = rsc->TCP.CoalescedSegmentCount;
        info.Info.DupAckCount = rsc->TCP.DuplicateAckCount;
    }

    NetBufferList.NetBufferListInfo[TcpRecvSegCoalesceInfo] = info.Value;
}

_Use_decl_annotations_
void
RxMetadataTranslator::TranslatePacketRscTimestamp(
    NET_BUFFER_LIST & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.RscTimestamp.Enabled)
    {
        return;
    }

    auto const packet = NetRingGetPacketAtIndex(NetRingCollectionGetPacketRing(&m_rings), PacketIndex);

    // We currently support RSC only for TCP
    if (packet->Layout.Layer4Type != NetPacketLayer4TypeTcp)
    {
        return;
    }

    auto const rsc = NetExtensionGetPacketRscTimestamp(&m_extensions.Extension.RscTimestamp, PacketIndex);
    void * info = reinterpret_cast<void *>(rsc->TCP.RscTcpTimestampDelta);

    NetBufferList.NetBufferListInfo[RscTcpTimestampDelta] = info;
}

_Use_decl_annotations_
void
RxMetadataTranslator::TranslatePacketLayout(
    _In_ NET_BUFFER_LIST & NetBufferList,
    _In_ UINT32 const PacketIndex
) const
{
    auto const & packet = *NetRingGetPacketAtIndex(NetRingCollectionGetPacketRing(&m_rings), PacketIndex);

    NetBufferList.NblFlags = 0;

    UINT16 layer3Type = 0u;
    switch (packet.Layout.Layer3Type)
    {

    case NetPacketLayer3TypeIPv4UnspecifiedOptions:
    case NetPacketLayer3TypeIPv4WithOptions:
    case NetPacketLayer3TypeIPv4NoOptions:
        layer3Type = RtlUshortByteSwap(ETHERNET_TYPE_IPV4);
        NdisSetNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_IPV4);
        break;

    case NetPacketLayer3TypeIPv6UnspecifiedExtensions:
    case NetPacketLayer3TypeIPv6WithExtensions:
    case NetPacketLayer3TypeIPv6NoExtensions:
        layer3Type = RtlUshortByteSwap(ETHERNET_TYPE_IPV6);
        NdisSetNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_IPV6);
        break;

    default:
        layer3Type = ParseLayer3Type(packet);
        break;

    }

    // NetBufferListFrameType expects layer 3 type in network byte order
    NetBufferList.NetBufferListInfo[NetBufferListFrameType] = reinterpret_cast<void *>(layer3Type);

    switch (packet.Layout.Layer4Type)
    {

    case NetPacketLayer4TypeTcp:
        NdisSetNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_TCP);
        break;

    case NetPacketLayer4TypeUdp:
        NdisSetNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_UDP);
        break;

    }
}

_Use_decl_annotations_
UINT16
RxMetadataTranslator::ParseLayer3Type(
    NET_PACKET const & Packet
) const
{
    if (Packet.Layout.Layer2Type != NetPacketLayer2TypeEthernet)
    {
        return 0u;
    }

    if (! m_extensions.Extension.VirtualAddress.Enabled)
    {
        return 0u;
    }

    auto const fragment = NetRingGetFragmentAtIndex(
        NetRingCollectionGetFragmentRing(&m_rings), Packet.FragmentIndex);
    auto const virtualAddress = NetExtensionGetFragmentVirtualAddress(
        &m_extensions.Extension.VirtualAddress, Packet.FragmentIndex);
    auto const ethernet = reinterpret_cast<ETHERNET_HEADER UNALIGNED const *>(
        static_cast<unsigned char *>(virtualAddress->VirtualAddress) + fragment->Offset);

    if (fragment->ValidLength < sizeof(*ethernet))
    {
        return 0u;
    }

    if (ETHERNET_TYPE_MINIMUM < RtlUshortByteSwap(ethernet->Type))
    {
        return ethernet->Type;
    }

    auto const snap = reinterpret_cast<SNAP_HEADER UNALIGNED const *>(ethernet + 1);

    if (fragment->ValidLength - sizeof(*ethernet) < sizeof(*snap))
    {
        return 0u;
    }

    if (snap->Control == SNAP_CONTROL
        && snap->Dsap == SNAP_DSAP
        && snap->Ssap == SNAP_SSAP
        && snap->Oui[0] == SNAP_OUI
        && snap->Oui[1] == SNAP_OUI
        && snap->Oui[2] == SNAP_OUI)
    {
        return snap->Type;
    }

    return 0u;
}

_Use_decl_annotations_
TxMetadataTranslator::TxMetadataTranslator(
    NET_RING_COLLECTION const & Rings,
    TxExtensions const & Extensions
) :
    m_rings(Rings),
    m_extensions(Extensions)
{
}

_Use_decl_annotations_
void
TxMetadataTranslator::TranslatePacketChecksum(
    NET_BUFFER_LIST const & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Checksum.Enabled)
    {
        return;
    }

    auto const & info = *reinterpret_cast<NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const *>(
        &NetBufferList.NetBufferListInfo[TcpIpChecksumNetBufferListInfo]);
    auto & checksum = *NetExtensionGetPacketChecksum(&m_extensions.Extension.Checksum, PacketIndex);

    checksum = {};
    if (info.Transmit.IsIPv4 && info.Transmit.IpHeaderChecksum)
    {
        checksum.Layer3 = NetPacketTxChecksumActionRequired;
    }

    if ((info.Transmit.IsIPv4 || info.Transmit.IsIPv6) &&
        (info.Transmit.UdpChecksum || info.Transmit.TcpChecksum))
    {
        checksum.Layer4 = NetPacketTxChecksumActionRequired;
    }
}

_Use_decl_annotations_
void
TxMetadataTranslator::TranslatePacketLso(
    NET_BUFFER_LIST const & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Lso.Enabled)
    {
        return;
    }

    auto const & info = *reinterpret_cast<NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO const *>(
        &NetBufferList.NetBufferListInfo[TcpLargeSendNetBufferListInfo]);
    auto & lso = *NetExtensionGetPacketLso(&m_extensions.Extension.Lso, PacketIndex);

    lso = {};
    switch (info.Transmit.Type)
    {
    case NDIS_TCP_LARGE_SEND_OFFLOAD_V1_TYPE:
        lso.TCP.Mss = info.LsoV1Transmit.MSS;
        break;

    case NDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE:
        lso.TCP.Mss = info.LsoV2Transmit.MSS;
        break;
    }
}

_Use_decl_annotations_
void
TxMetadataTranslator::TranslatePacketLsoComplete(
    NET_BUFFER_LIST & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Lso.Enabled)
    {
        return;
    }

    auto const packet = NetRingGetPacketAtIndex(NetRingCollectionGetPacketRing(&m_rings), PacketIndex);

    if (packet->Layout.Layer4Type != NetPacketLayer4TypeTcp)
    {
        return;
    }

    auto const & lso = *NetExtensionGetPacketLso(&m_extensions.Extension.Lso, PacketIndex);

    if (lso.TCP.Mss == 0)
    {
        return;
    }

    auto & info = *reinterpret_cast<NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO *>(
        &NetBufferList.NetBufferListInfo[TcpLargeSendNetBufferListInfo]);
    auto const type = info.Transmit.Type;

    info = {};
    info.Transmit.Type = type;
    if (info.Transmit.Type == NDIS_TCP_LARGE_SEND_OFFLOAD_V1_TYPE)
    {
        auto const fr = NetRingCollectionGetFragmentRing(&m_rings);
        for (UINT32 index = 0; index < packet->FragmentCount; index++)
        {
            auto const fragmentIndex = NetRingAdvanceIndex(fr, packet->FragmentIndex, index);
            auto const fragment = NetRingGetFragmentAtIndex(fr, fragmentIndex);

            info.LsoV1TransmitComplete.TcpPayload += static_cast<UINT32>(fragment->ValidLength);
        }

        auto const payloadOffset = packet->Layout.Layer2HeaderLength
            + packet->Layout.Layer3HeaderLength
            + packet->Layout.Layer4HeaderLength;

        NT_FRE_ASSERT(static_cast<UINT32>(payloadOffset) <= info.LsoV1TransmitComplete.TcpPayload);

        info.LsoV1TransmitComplete.TcpPayload -= payloadOffset;
    }
}

_Use_decl_annotations_
void
TxMetadataTranslator::TranslatePacketWifiExemptionAction(
    NET_BUFFER_LIST const & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.WifiExemptionAction.Enabled)
    {
        return;
    }

    auto const & info = *reinterpret_cast<DOT11_EXTSTA_SEND_CONTEXT const *>(
        &NetBufferList.NetBufferListInfo[MediaSpecificInformation]);
    auto & exemptionAction = *WifiExtensionGetExemptionAction(&m_extensions.Extension.WifiExemptionAction, PacketIndex);

    exemptionAction = {};
    exemptionAction.ExemptionAction = static_cast<UINT8>(info.usExemptionActionType);
}
