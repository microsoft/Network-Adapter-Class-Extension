// Copyright (C) Microsoft Corporation. All rights reserved.

#include <ndis.h>
#include <ndis/nblchain.h>
#include <rsclib.h>

#include "Coalescing.hpp"
#include "NxRxNblContext.hpp"

_Use_decl_annotations_
ReceiveSegmentCoalescingContext::ReceiveSegmentCoalescingContext(
    void
)
{
    RSCLIBInitializeContext(static_cast<void *>(this), &m_rsclibContext);
}

static
COALEASCING_FALLBACK_INFO const *
GetCoalescingFallbackInfo(
    _In_ NET_BUFFER_LIST const * NetBufferList
)
{
    auto rxNblContext = GetRxContextFromNbl(const_cast<NET_BUFFER_LIST *>(NetBufferList));

    return &rxNblContext->CoaleascingFallbackInfo;
}

//
// Determine if a NBL can be coalesced by the software RSC.
//
bool
ReceiveSegmentCoalescingBatch::IsNetBufferListCoalescable(
    _In_ NET_BUFFER_LIST const * NetBufferList,
    _Out_ UCHAR * IpHeaderOffset
)
{
    // Don't coalesce if device has hardware capabilities.
    if (NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_UDP) &&
        m_isHardwareUdpRscSupported)
    {
        return false;
    }

    if (NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_TCP))
    {
        if (NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_IPV4) &&
            m_isIpv4HardwareRscSupported)
        {
            return false;
        }

        if (NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_IPV6) &&
            m_isIpv6HardwareRscSupported)
        {
            return false;
        }
    }

    // Don't coalesce vlan frames.
    if (NDIS_GET_NET_BUFFER_LIST_VLAN_ID(NetBufferList) != 0)
    {
        return false;
    }

    auto const & fallbackInfo = GetCoalescingFallbackInfo(NetBufferList);
    ULONG const layer2Type = fallbackInfo->Info.Layer2Type;

    // Only try to coalesce Ethernet || Ieee80211 || raw IP (for MBB) frames.
    auto const isEthernet = layer2Type == NetPacketLayer2TypeEthernet;
    auto const isWiFi = layer2Type == NetPacketLayer2TypeIeee80211;
    auto const isWwan = (layer2Type == NetPacketLayer2TypeNull) && (m_mediaType == NdisMediumWirelessWan);
    auto const isCoalescablePacket = isEthernet || isWiFi || isWwan;
    if (!isCoalescablePacket)
    {
        return false;
    }

    // Only try to coalesce IPV4 || IPV6 packets.
    if (! NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_IPV4) &&
        ! NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_IPV6))
    {
        return false;
    }

    // Only try to coalesce TCP or UDP segments.
    if (! NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_TCP) &&
        ! NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_UDP))
    {
        return false;
    }

    *IpHeaderOffset = fallbackInfo->Info.Layer2HeaderLength;

    return true;
}

_Use_decl_annotations_
ReceiveSegmentCoalescingBatch::ReceiveSegmentCoalescingBatch(
    ReceiveSegmentCoalescingContext & RsclibContext,
    bool IsRscEnabled,
    bool IsIpv4HardwareRscSupported,
    bool IsIpv6HardwareRscSupported,
    bool IsUdpRscEnabled,
    bool IsHardwareUdpRscSupported,
    NDIS_MEDIUM MediaType
) : m_pRsclibContext(&RsclibContext.m_rsclibContext)
  , m_isIpv4HardwareRscSupported(IsIpv4HardwareRscSupported)
  , m_isIpv6HardwareRscSupported(IsIpv6HardwareRscSupported)
  , m_isHardwareUdpRscSupported(IsHardwareUdpRscSupported)
  , m_coalesceFlags(RSCLIB_FLAG_ALLOW_DEFERRED_CHECKSUM)
  , m_mediaType(MediaType)
{
    //
    // If the hardware supports coalescing for both IPv4 and IPv6 TCP packets
    // and UDP packets, that means hardware has attempted coalescing already,
    // don't bother to do it again in software
    //
    if (m_isIpv4HardwareRscSupported && m_isIpv6HardwareRscSupported && m_isHardwareUdpRscSupported)
    {
        m_shouldCoalesce = false;
    }
    else
    {
        if (m_isIpv4HardwareRscSupported && m_isIpv6HardwareRscSupported)
        {
            m_coalesceFlags |= RSCLIB_FLAG_SKIP_TCP;
        }
        if (m_isHardwareUdpRscSupported)
        {
            m_coalesceFlags |= RSCLIB_FLAG_SKIP_UDP;
        }
        //
        // we only attempt coalescing in software if the stack has activated the RSC offload
        //
        m_shouldCoalesce = IsRscEnabled || IsUdpRscEnabled;
    }

    //
    // Just for reset RSCLIB_STATS
    //
    RSCLIBReadAndResetStats(m_pRsclibContext, &m_rscStats);
}

_Use_decl_annotations_
void
ReceiveSegmentCoalescingBatch::TryCoalesceOneReceiveSegment(
    NET_BUFFER_LIST * NetBufferList,
    NET_BUFFER_LIST ** & TailOfNblChain,
    ULONG & NumberOfNbls
)
{
    UCHAR ipHeaderOffset = 0U;

    m_nblsAttemptedCount++;

    if (m_shouldCoalesce)
    {
        if (IsNetBufferListCoalescable(
                NetBufferList,
                &ipHeaderOffset))
        {
            RSCLIBCoalesceNBL(
                m_pRsclibContext,
                ipHeaderOffset,
                m_coalesceFlags,
                0,
                NetBufferList,
                &TailOfNblChain,
                &NumberOfNbls);

            return;
        }
        else
        {
            RSCLIBFlushContext(m_pRsclibContext, &TailOfNblChain, &NumberOfNbls);
        }
    }

    *TailOfNblChain = NetBufferList;
    TailOfNblChain = &NetBufferList->Next;
    NetBufferList->Next = nullptr;
    ++NumberOfNbls;
}

_Use_decl_annotations_
void
ReceiveSegmentCoalescingBatch::FinalizeCoalescing(
    NET_BUFFER_LIST ** & TailOfNblChain,
    ULONG & NumberOfNbls
)
{
    if (m_shouldCoalesce)
    {
        RSCLIBFlushContext(m_pRsclibContext, &TailOfNblChain, &NumberOfNbls);
        RSCLIBReadAndResetStats(m_pRsclibContext, &m_rscStats);
    }
}

_Use_decl_annotations_
void
ReceiveSegmentCoalescingContext::PerformReceiveSegmentUncoalescing(
    NET_BUFFER_LIST * & NblChain,
    NET_BUFFER_LIST * & OutputNblChainTail,
    ULONG & NumberOfNbls
)
{
    NET_BUFFER_LIST * currentNbl = NblChain;
    OutputNblChainTail = nullptr;

    NumberOfNbls = 0U;
    ULONG tmpNumberOfUncoalescedNbls = 0;

    while (currentNbl)
    {
        auto const coalescedTcpSegments = NET_BUFFER_LIST_COALESCED_SEG_COUNT(currentNbl);
        auto const coalescedUdpPackets = NET_BUFFER_LIST_UDP_COALESCED_SEG_COUNT(currentNbl);
        auto const wasNblCoalesced = coalescedTcpSegments > 1 || coalescedUdpPackets > 1;

        if (wasNblCoalesced &&
            RscLibExUtilMatchCoalescedNBLCookie(currentNbl, static_cast<const void *>(this)))
        {
            RscLibExUncoalesceNBL(
                currentNbl,
                static_cast<const void *>(this),
                &OutputNblChainTail,
                &tmpNumberOfUncoalescedNbls);
            NumberOfNbls += tmpNumberOfUncoalescedNbls;
        }
        else
        {
            OutputNblChainTail = currentNbl;
            ++NumberOfNbls;
        }

        currentNbl = OutputNblChainTail->Next;
    }
}

_Use_decl_annotations_
USHORT
ReceiveSegmentCoalescingBatch::GetNblsCoalescedCount(
    void
) const
{
    return m_rscStats.CoalescedSegments;
}

_Use_decl_annotations_
USHORT
ReceiveSegmentCoalescingBatch::GetScusGeneratedCount(
    void
) const
{
    return m_rscStats.GeneratedSCUs;
}

_Use_decl_annotations_
ULONG
ReceiveSegmentCoalescingBatch::GetBytesCoalescedCount(
    void
) const
{
    return m_rscStats.CoalescedTransportPayloadBytes;
}

_Use_decl_annotations_
ULONG
ReceiveSegmentCoalescingBatch::GetNblsAttemptedCount(
    void
) const
{
    return m_nblsAttemptedCount;
}
