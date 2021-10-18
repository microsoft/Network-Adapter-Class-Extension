// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxPacketLayout.hpp"
#include "NxPacketLayout.tmh"

#include <net/ring.h>
#include <net/fragment.h>
#include <net/virtualaddress_p.h>
#include <netiodef.h>

#include "framelayout/Layer2Layout.hpp"
#include "framelayout/Layer3Layout.hpp"
#include "framelayout/Layer4Layout.hpp"

_Use_decl_annotations_
NET_PACKET_LAYER2_TYPE
TranslateMedium(
    NDIS_MEDIUM Medium
)
{
    switch (Medium)
    {

    case NdisMedium802_3:
        return NetPacketLayer2TypeEthernet;

    case NdisMediumNative802_11:
        return NetPacketLayer2TypeIeee80211;

    case NdisMediumIP:
    case NdisMediumWiMAX:
    case NdisMediumWirelessWan:
        return NetPacketLayer2TypeNull;

    }

    return NetPacketLayer2TypeUnspecified;
}

static
bool
ParseLayer2LayoutIfNeeded(
    _Inout_ NET_PACKET * Packet,
    _In_reads_bytes_(BytesRemaining) UCHAR const * Buffer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & BytesRemaining,
    _Inout_ bool & PreCheck
)
{
    if (PreCheck &&
        Packet->Layout.Layer2Type != NetPacketLayer2TypeUnspecified &&
        Packet->Layout.Layer2HeaderLength != 0U)
    {
        Offset += Packet->Layout.Layer2HeaderLength;
        BytesRemaining -= Packet->Layout.Layer2HeaderLength;

        return true;
    }

    if (Packet->Layout.Layer2Type == NetPacketLayer2TypeUnspecified ||
        BytesRemaining == 0U)
    {
        return false;
    }

    // Since layer 2 info is not present, no need to check if client driver
    // filled layer 3/4 layout info.
    PreCheck = false;
    Packet->Layout.Layer3Type = NetPacketLayer3TypeUnspecified;
    Packet->Layout.Layer3HeaderLength = 0U;

    return Layer2::ParseLayout(Packet->Layout, Buffer, Offset, BytesRemaining);
}

static
bool
ParseLayer3LayoutIfNeeded(
    _Inout_ NET_PACKET * Packet,
    _In_reads_bytes_(BytesRemaining) UCHAR const * Buffer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & BytesRemaining,
    _Inout_ bool & PreCheck
)
{
    if (PreCheck &&
        Packet->Layout.Layer3Type != NetPacketLayer3TypeUnspecified &&
        Packet->Layout.Layer3Type != NetPacketLayer3TypeIPv4UnspecifiedOptions &&
        Packet->Layout.Layer3Type != NetPacketLayer3TypeIPv6UnspecifiedExtensions &&
        Packet->Layout.Layer3HeaderLength != 0U)
    {
        Offset += Packet->Layout.Layer3HeaderLength;
        BytesRemaining -= Packet->Layout.Layer3HeaderLength;

        return true;
    }

    if (BytesRemaining == 0U)
    {
        return false;
    }

    // Since layer 3 info is not present, no need to check if client driver
    // filled layer 4 layout info.
    PreCheck = false;
    Packet->Layout.Layer4Type = NetPacketLayer4TypeUnspecified;
    Packet->Layout.Layer4HeaderLength = 0U;

    return Layer3::ParseLayout(Packet->Layout, Buffer, Offset, BytesRemaining);
}

static
bool
ParseLayer4LayoutIfNeeded(
    _Inout_ NET_PACKET * Packet,
    _In_reads_bytes_(BytesRemaining) UCHAR const * Buffer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & BytesRemaining,
    _In_ bool PreCheck
)
{
    if (PreCheck &&
        Packet->Layout.Layer4Type != NetPacketLayer4TypeUnspecified &&
        Packet->Layout.Layer4HeaderLength != 0U)
    {
        Offset += Packet->Layout.Layer4HeaderLength;
        BytesRemaining -= Packet->Layout.Layer4HeaderLength;

        return true;
    }

    if (BytesRemaining == 0U)
    {
        return false;
    }

    return Layer4::ParseLayout(Packet->Layout, Buffer, Offset, BytesRemaining);
}

_Use_decl_annotations_
void
NxGetPacketLayout(
    NDIS_MEDIUM MediaType,
    NET_RING_COLLECTION const * Descriptor,
    NET_EXTENSION const & VirtualAddressExtension,
    NET_PACKET * Packet,
    size_t PayloadBackfill
)
{
    NT_ASSERT(Packet->FragmentCount != 0);

    auto fr = NetRingCollectionGetFragmentRing(Descriptor);
    auto const fragment = NetRingGetFragmentAtIndex(fr, Packet->FragmentIndex);
    auto const virtualAddress = NetExtensionGetFragmentVirtualAddress(
        &VirtualAddressExtension, Packet->FragmentIndex);

    // Skip the backfill space
    auto buffer = (UCHAR const*)virtualAddress->VirtualAddress + fragment->Offset + PayloadBackfill;
    auto bytesRemaining = (size_t)fragment->ValidLength - PayloadBackfill;

    if (Packet->Layout.Layer2Type == NetPacketLayer2TypeUnspecified)
    {
        Packet->Layout = {};
        Packet->Layout.Layer2Type = TranslateMedium(MediaType);
    }

    size_t offset = 0U;

    // Initially should check if client driver already filled layout info.
    bool preCheck = true;

    if (! ParseLayer2LayoutIfNeeded(Packet, buffer, offset, bytesRemaining, preCheck))
    {
        return;
    }

    if (! ParseLayer3LayoutIfNeeded(Packet, buffer, offset, bytesRemaining, preCheck))
    {
        return;
    }

    ParseLayer4LayoutIfNeeded(Packet, buffer, offset, bytesRemaining, preCheck);
}

_Use_decl_annotations_
AddressType
NxGetPacketAddressType(
    NET_PACKET_LAYER2_TYPE Layer2Type,
    NET_BUFFER const & NetBuffer
)
{
    static UINT8 const broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    auto const pointer = static_cast<UINT8 const *>(
        MmGetSystemAddressForMdlSafe(NetBuffer.CurrentMdl, LowPagePriority | MdlMappingNoExecute));
    size_t const offset = NetBuffer.CurrentMdlOffset;

    switch (Layer2Type)
    {

    case NetPacketLayer2TypeEthernet:
        if (! (pointer[offset] & 0x01))
        {
            return AddressType::Unicast;
        }

        if (! RtlEqualMemory(&broadcast, &pointer[offset], sizeof(broadcast)))
        {
            return AddressType::Multicast;
        }

        return AddressType::Broadcast;

    case NetPacketLayer2TypeNull:
        return AddressType::Unicast;

    }

    return AddressType::Unicast;
}
