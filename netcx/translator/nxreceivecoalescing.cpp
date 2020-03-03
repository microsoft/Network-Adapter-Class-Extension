// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxReceiveCoalescing.tmh"
#include "NxReceiveCoalescing.hpp"

#include <net/rsc.h>

_Use_decl_annotations_
NDIS_RSC_NBL_INFO
NxTranslateRxPacketRsc(
    NET_PACKET const * packet,
    NET_EXTENSION const * rscExtension,
    UINT32 packetIndex
)
{
    NDIS_RSC_NBL_INFO rsc = {};

    // We currently support RSC only for TCP
    if (packet->Layout.Layer4Type != NetPacketLayer4TypeTcp)
        return rsc;

    auto const rscExt = NetExtensionGetPacketRsc(rscExtension, packetIndex);

    rsc.Info.CoalescedSegCount = rscExt->TCP.CoalescedSegmentCount;
    
    if (rscExt->TCP.CoalescedSegmentCount > 0)
    {
        rsc.Info.DupAckCount = rscExt->TCP.DuplicateAckCount;
    }

    return rsc;
}

_Use_decl_annotations_
void *
NxTranslateRxPacketRscTimestamp(
    NET_PACKET const * packet,
    NET_EXTENSION const * rscTimestampExtension,
    UINT32 packetIndex
)
{
    // We currently support RSC only for TCP
    if (packet->Layout.Layer4Type != NetPacketLayer4TypeTcp)
        return 0;

    auto const rscTimpstampExt = NetExtensionGetPacketRscTimestamp(rscTimestampExtension, packetIndex);

    return (PVOID) rscTimpstampExt->TCP.RscTcpTimestampDelta;
}
