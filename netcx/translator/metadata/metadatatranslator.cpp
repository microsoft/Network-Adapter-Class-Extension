// Copyright (C) Microsoft Corporation. All rights reserved.

#ifdef _KERNEL_MODE
#include <ntddk.h>
#include <ntassert.h>
#endif

#include <ndis.h>
#include <netiodef.h>
#include <wil/common.h>

#include "MetadataTranslator.hpp"
#include "NxRxNblContext.hpp"

#include <net/ring.h>
#include <net/fragment.h>
#include <net/checksum.h>
#include <net/rsc.h>
#include <net/gso.h>
#include <net/virtualaddress.h>
#include <net/wifi/exemptionaction.h>
#include <net/ieee8021q.h>
#include <net/packethash.h>

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

    COALEASCING_FALLBACK_INFO rscFallbackInfo = {};
    rscFallbackInfo.Info.Layer2Type = packet.Layout.Layer2Type;
    rscFallbackInfo.Info.Layer2HeaderLength = packet.Layout.Layer2HeaderLength;

    auto rxNblContext = GetRxContextFromNbl(&NetBufferList);
	rxNblContext->CoaleascingFallbackInfo.Value = rscFallbackInfo.Value;

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
void
RxMetadataTranslator::TranslatePacketIeee8021q(
    NET_BUFFER_LIST & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Ieee8021q.Enabled)
    {
        return;
    }

    auto const ieee8021q = NetExtensionGetPacketIeee8021Q(&m_extensions.Extension.Ieee8021q, PacketIndex);
    NDIS_NET_BUFFER_LIST_8021Q_INFO info = {};

    info.TagHeader.UserPriority = ieee8021q->PriorityCodePoint;
    info.TagHeader.VlanId = ieee8021q->VlanIdentifier;

    NetBufferList.NetBufferListInfo[Ieee8021QNetBufferListInfo] = info.Value;
}

_Use_decl_annotations_
void
RxMetadataTranslator::TranslatePacketHash(
    NET_BUFFER_LIST & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Hash.Enabled)
    {
        return;
    }

    auto const packetHash = NetExtensionGetPacketHash(&m_extensions.Extension.Hash, PacketIndex);

    ULONG hashType = 0;

    auto const ipv4Flags = NetPacketHashProtocolTypeIPv4 | NetPacketHashProtocolTypeIPv4Options;
    if (WI_IsAnyFlagSet(packetHash->ProtocolType, ipv4Flags))
    {
        if (WI_IsFlagSet(packetHash->ProtocolType, NetPacketHashProtocolTypeTcp))
        {
            hashType = NDIS_HASH_TCP_IPV4;
        }
        else if (WI_IsFlagSet(packetHash->ProtocolType, NetPacketHashProtocolTypeUdp))
        {
            hashType = NDIS_HASH_UDP_IPV4;
        }
        else
        {
            hashType = NDIS_HASH_IPV4;
        }
    }

    if (WI_IsFlagSet(packetHash->ProtocolType, NetPacketHashProtocolTypeIPv6))
    {
        if (WI_IsFlagSet(packetHash->ProtocolType, NetPacketHashProtocolTypeTcp))
        {
            hashType = NDIS_HASH_TCP_IPV6;
        }
        else if (WI_IsFlagSet(packetHash->ProtocolType, NetPacketHashProtocolTypeUdp))
        {
            hashType = NDIS_HASH_UDP_IPV6;
        }
        else
        {
            hashType = NDIS_HASH_IPV6;
        }
    }

    if (WI_IsFlagSet(packetHash->ProtocolType, NetPacketHashProtocolTypeIPv6Extensions))
    {
        if (WI_IsFlagSet(packetHash->ProtocolType, NetPacketHashProtocolTypeTcp))
        {
            hashType = NDIS_HASH_TCP_IPV6_EX;
        }
        else if (WI_IsFlagSet(packetHash->ProtocolType, NetPacketHashProtocolTypeUdp))
        {
            hashType = NDIS_HASH_UDP_IPV6_EX;
        }
        else
        {
            hashType = NDIS_HASH_IPV6_EX;
        }
    }

    if (hashType)
    {
        NET_BUFFER_LIST_SET_HASH_TYPE(&NetBufferList, hashType);
        NET_BUFFER_LIST_SET_HASH_FUNCTION(&NetBufferList, NdisHashFunctionToeplitz);
        NET_BUFFER_LIST_SET_HASH_VALUE(&NetBufferList, packetHash->HashValue);
    }
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

    auto & checksum = *NetExtensionGetPacketChecksum(&m_extensions.Extension.Checksum, PacketIndex);

    checksum = TranslatePacketChecksum(NetBufferList);
}

NET_PACKET_CHECKSUM
TxMetadataTranslator::TranslatePacketChecksum(
    NET_BUFFER_LIST const & NetBufferList
) const
{
    auto const & info = *reinterpret_cast<NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const *>(
        &NetBufferList.NetBufferListInfo[TcpIpChecksumNetBufferListInfo]);

    NET_PACKET_CHECKSUM checksum = {};
    if (info.Transmit.IsIPv4 && info.Transmit.IpHeaderChecksum)
    {
        checksum.Layer3 = NetPacketTxChecksumActionRequired;
    }

    if ((info.Transmit.IsIPv4 || info.Transmit.IsIPv6) &&
        (info.Transmit.UdpChecksum || info.Transmit.TcpChecksum))
    {
        checksum.Layer4 = NetPacketTxChecksumActionRequired;
    }

    return checksum;
}

_Use_decl_annotations_
NET_PACKET_GSO
TxMetadataTranslator::TranslatePacketGso(
    NET_BUFFER_LIST const & NetBufferList
) const
{
    NET_PACKET_GSO gso = {};

    if (NetBufferList.NetBufferListInfo[TcpLargeSendNetBufferListInfo] != 0)
    {
        auto const & info = *reinterpret_cast<NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO const *>(
            &NetBufferList.NetBufferListInfo[TcpLargeSendNetBufferListInfo]);

        switch (info.Transmit.Type)
        {
        case NDIS_TCP_LARGE_SEND_OFFLOAD_V1_TYPE:
            gso.TCP.Mss = info.LsoV1Transmit.MSS;
            break;

        case NDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE:
            gso.TCP.Mss = info.LsoV2Transmit.MSS;
            break;
        }
    }
    else if (NetBufferList.NetBufferListInfo[UdpSegmentationOffloadInfo] != 0)
    {
        auto const & info = *reinterpret_cast<NDIS_UDP_SEGMENTATION_OFFLOAD_NET_BUFFER_LIST_INFO const *>(
            &NetBufferList.NetBufferListInfo[UdpSegmentationOffloadInfo]);

        gso.UDP.Mss = info.Transmit.MSS;
    }

    return gso;
}

_Use_decl_annotations_
void
TxMetadataTranslator::TranslatePacketGso(
    NET_BUFFER_LIST const & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Gso.Enabled)
    {
        return;
    }

    auto & gso = *NetExtensionGetPacketGso(&m_extensions.Extension.Gso, PacketIndex);

    gso = TranslatePacketGso(NetBufferList);

}

_Use_decl_annotations_
void
TxMetadataTranslator::TranslatePacketGsoComplete(
    NET_BUFFER_LIST & NetBufferList,
    UINT32 const PacketIndex
) const
{
    auto const packet = NetRingGetPacketAtIndex(NetRingCollectionGetPacketRing(&m_rings), PacketIndex);

    if (packet->Layout.Layer4Type != NetPacketLayer4TypeTcp &&
        packet->Layout.Layer4Type != NetPacketLayer4TypeUdp)
    {
        return;
    }

    if (packet->Layout.Layer4Type == NetPacketLayer4TypeTcp)
    {
        auto & info = *reinterpret_cast<NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO *>(
            &NetBufferList.NetBufferListInfo[TcpLargeSendNetBufferListInfo]);

        // MSS info needs to be checked because NDIS_TCP_LARGE_SEND_OFFLOAD_V1_TYPE's value is zero
        if (info.Transmit.Type == NDIS_TCP_LARGE_SEND_OFFLOAD_V1_TYPE && info.LsoV1Transmit.MSS > 0U)
        {
            // NBL that contains the LSO OOB data contains only a single NET_BUFFER.
            info.LsoV1TransmitComplete.TcpPayload = NetBufferList.FirstNetBuffer->DataLength;

            auto const payloadOffset = packet->Layout.Layer2HeaderLength
                + packet->Layout.Layer3HeaderLength
                + packet->Layout.Layer4HeaderLength;

            NT_FRE_ASSERT(static_cast<UINT32>(payloadOffset) <= info.LsoV1TransmitComplete.TcpPayload);

            info.LsoV1TransmitComplete.TcpPayload -= payloadOffset;
        }
        else if (info.Transmit.Type == NDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE)
        {
            // NDIS requires miniport driver to set Reserved to zero
            info.LsoV2TransmitComplete.Reserved = 0U;
        }
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

    auto const info = static_cast<DOT11_EXTSTA_SEND_CONTEXT const *>(
        NetBufferList.NetBufferListInfo[MediaSpecificInformation]);
    auto & exemptionAction = *WifiExtensionGetExemptionAction(&m_extensions.Extension.WifiExemptionAction, PacketIndex);

    exemptionAction = {};

    if (info == nullptr)
    {
        return;
    }

    exemptionAction.ExemptionAction = static_cast<UINT8>(info->usExemptionActionType);
}

_Use_decl_annotations_
void
TxMetadataTranslator::TranslatePacketIeee8021q(
    NET_BUFFER_LIST const & NetBufferList,
    UINT32 const PacketIndex
) const
{
    if (! m_extensions.Extension.Ieee8021q.Enabled)
    {
        return;
    }

    auto & ieee8021q = *NetExtensionGetPacketIeee8021Q(&m_extensions.Extension.Ieee8021q, PacketIndex);

    ieee8021q = TranslatePacketIeee8021q(NetBufferList);
}

NET_PACKET_IEEE8021Q
TxMetadataTranslator::TranslatePacketIeee8021q(
    NET_BUFFER_LIST const & NetBufferList
) const
{
    auto const & info = *reinterpret_cast<NDIS_NET_BUFFER_LIST_8021Q_INFO const *>(
        &NetBufferList.NetBufferListInfo[Ieee8021QNetBufferListInfo]);

    NET_PACKET_IEEE8021Q ieee8021q = {};
    ieee8021q.TxTagging;

    if (info.TagHeader.UserPriority)
    {
        ieee8021q.PriorityCodePoint = info.TagHeader.UserPriority;
        ieee8021q.TxTagging |= NetPacketTxIeee8021qActionFlagPriorityRequired;
    }

    if (info.TagHeader.VlanId)
    {
        ieee8021q.VlanIdentifier = info.TagHeader.VlanId;
        ieee8021q.TxTagging |= NetPacketTxIeee8021qActionFlagVlanRequired;
    }

    return ieee8021q;
}
