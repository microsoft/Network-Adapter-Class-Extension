// Copyright (C) Microsoft Corporation. All rights reserved.

#include <ndis.h>
#include <ndis/nblchain.h>
#include <rsclib.h>

#include "Coalescing.hpp"
#include "NxRxNblContext.hpp"

_Use_decl_annotations_
void
ReceiveSegmentCoalescing::Initialize(
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

// Determine if software RSC for a NBL is needed.
static
bool
ShouldCoalesceNetBufferList(
    _In_ NET_BUFFER_LIST const * NetBufferList,
    _In_ bool Ipv4HardwareCapabilities,
    _In_ bool Ipv6HardwareCapabilities,
    _Out_ UCHAR * IpHeaderOffset
)
{
    // Don't coalesce if device has hardware capabilities.
    if (NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_IPV4) &&
        Ipv4HardwareCapabilities)
    {
        return false;
    }

    if (NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_IPV6) &&
        Ipv6HardwareCapabilities)
    {
        return false;
    }

    // Don't coalesce vlan frames.
    if (NDIS_GET_NET_BUFFER_LIST_VLAN_ID(NetBufferList) != 0)
    {
        return false;
    }

    auto const & fallbackInfo = GetCoalescingFallbackInfo(NetBufferList);
    ULONG const layer2Type = fallbackInfo->Info.Layer2Type;

    // Only try to coalesce Ethernet || Ieee80211 frames.
    if (layer2Type != NetPacketLayer2TypeEthernet &&
        layer2Type != NetPacketLayer2TypeIeee80211)
    {
        return false;
    }

    // Only try to coalesce IPV4 || IPV6 packets.
    if (! NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_IPV4) &&
        ! NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_IPV6))
    {
        return false;
    }

    // Only try to coalesce TCP || UDP segments.
    if (! NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_TCP) &&
        ! NdisTestNblFlag(NetBufferList, NDIS_NBL_FLAGS_IS_UDP))
    {
        return false;
    }

    *IpHeaderOffset = fallbackInfo->Info.Layer2HeaderLength;

    return true;
}

_Use_decl_annotations_
void
ReceiveSegmentCoalescing::PerformReceiveSegmentCoalescing(
    NET_BUFFER_LIST * & NblChain,
    bool Ipv4HardwareCapabilities,
    bool Ipv6HardwareCapabilities,
    NET_BUFFER_LIST * & OutputNblChainTail,
    ULONG & NumberOfNbls
)
{
    NET_BUFFER_LIST * currentNbl = NblChain;
    NET_BUFFER_LIST * nextNbl = NblChain;

    NET_BUFFER_LIST * OutputNblChainHead = nullptr;
    NET_BUFFER_LIST ** tailNbl = &OutputNblChainHead;
    NumberOfNbls = 0U;

    // Just for reset RSCLIB_STATS
    RSCLIBReadAndResetStats(&m_rsclibContext, &m_rscStats);

    while (currentNbl)
    {
        nextNbl = currentNbl->Next;
        UCHAR ipHeaderOffset = 0U;

        if (ShouldCoalesceNetBufferList(
            currentNbl,
            Ipv4HardwareCapabilities,
            Ipv6HardwareCapabilities,
            &ipHeaderOffset))
        {
            NT_FRE_ASSERT(ipHeaderOffset > 0U);

            RSCLIBCoalesceNBL(
                &m_rsclibContext,
                ipHeaderOffset,
                RSCLIB_FLAG_ALLOW_DEFERRED_CHECKSUM,
                0,
                currentNbl,
                &tailNbl,
                &NumberOfNbls);
        }
        else
        {
            RSCLIBFlushContext(&m_rsclibContext, &tailNbl, &NumberOfNbls);

            *tailNbl = currentNbl;
            tailNbl = &currentNbl->Next;
            currentNbl->Next = nullptr;
            ++NumberOfNbls;
        }
        currentNbl = nextNbl;
    }

    RSCLIBFlushContext(&m_rsclibContext, &tailNbl, &NumberOfNbls);

    RSCLIBReadAndResetStats(&m_rsclibContext, &m_rscStats);

    NblChain = OutputNblChainHead;
    OutputNblChainTail = NdisLastNblInNblChain(NblChain);
}

_Use_decl_annotations_
void
ReceiveSegmentCoalescing::PerformReceiveSegmentUncoalescing(
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
        auto const coalescedUdpSegments = NET_BUFFER_LIST_UDP_COALESCED_SEG_COUNT(currentNbl);
        auto const wasNblCoalesced = coalescedTcpSegments > 1 || coalescedUdpSegments > 1;

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
ReceiveSegmentCoalescing::GetNblsCoalescedCount(
    void
) const
{
    return m_rscStats.CoalescedSegments;
}

_Use_decl_annotations_
USHORT
ReceiveSegmentCoalescing::GetScusGeneratedCount(
    void
) const
{
    return m_rscStats.GeneratedSCUs;
}

_Use_decl_annotations_
ULONG
ReceiveSegmentCoalescing::GetBytesCoalescedCount(
    void
) const
{
    return m_rscStats.CoalescedTransportPayloadBytes;
}
