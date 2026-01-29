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
    /*
     * special case check for NetPacketLayer2TypeNull to abandon parsing
     * earlier due to regression introduced described in comment below.
     */
    if (Packet->Layout.Layer2Type == NetPacketLayer2TypeNull)
    {
        return true;
    }

    if (Packet->Layout.Layer2Type == NetPacketLayer2TypeUnspecified)
    {
        return false;
    }

    if (Packet->Layout.Layer2HeaderLength != 0U)
    {
        if (BytesRemaining < Packet->Layout.Layer2HeaderLength)
        {
            return false;
        }

        Offset += Packet->Layout.Layer2HeaderLength;
        BytesRemaining -= Packet->Layout.Layer2HeaderLength;

        return true;
    }

    /*
     * the following comment describes incorrect behavior. it is being
     * retained with the code that is also incorrect. the impact of the
     * code is that if parsing of Layer2 layout fails it destroys the
     * driver provided Layer3{Type,HeaderLength} which is wrong.
     *
     * we cannot remove this code right now as a driver may have taken
     * a dependency on the inappropriate initialization.
     */
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
        /*
         * Packet->Layout = {}; is incorrect here. the translator should not
         * assume that the driver hasn't provided valid Layer{3,4} metadata
         * just because they indicated Layer2 was unspecified.
         *
         * since a driver may have taken a dependency on this incorrect
         * initialization (instead not supplying their own initialization)
         * we cannot remove the code without risking a compatibility break.
         *
         * the correct behavior here is to Packet->Layout.Layer2Length = 0
         * and then request a layer2 parse.
         */
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

static
AddressType
NxGetEthernetAddressType(
    _In_ const BYTE * L2HeaderStart,
    _In_ size_t BytesRemaining
)
{
    static UINT8 const broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    if (BytesRemaining < sizeof(broadcast))
    {
        return AddressType::Unicast;
    }

    if (! (*L2HeaderStart & 0x01))
    {
        return AddressType::Unicast;
    }

    if (! RtlEqualMemory(&broadcast, L2HeaderStart, sizeof(broadcast)))
    {
        return AddressType::Multicast;
    }

    return AddressType::Broadcast;
}


_Use_decl_annotations_
AddressType
NxInitializePacketLayout(
    NDIS_MEDIUM MediaType,
    const void *Payload,
    size_t PayloadSize,
    NET_PACKET* Packet
)
{
    const BYTE *buffer = static_cast<const BYTE *>(Payload);
    size_t bytesRemaining = PayloadSize;

    AddressType destinationType = AddressType::Unicast;

    if (Packet->Layout.Layer2Type == NetPacketLayer2TypeUnspecified)
    {
        /*
         * Packet->Layout = {}; is incorrect here. the translator should not
         * assume that the driver hasn't provided valid Layer{3,4} metadata
         * just because they indicated Layer2 was unspecified.
         *
         * since a driver may have taken a dependency on this incorrect
         * initialization (instead not supplying their own initialization)
         * we cannot remove the code without risking a compatibility break.
         *
         * the correct behavior here is to Packet->Layout.Layer2Length = 0
         * and then request a layer2 parse.
         */
        Packet->Layout = {};
        Packet->Layout.Layer2Type = TranslateMedium(MediaType);
    }

    if (Packet->Layout.Layer2Type == NetPacketLayer2TypeEthernet)
    {
        destinationType = NxGetEthernetAddressType(buffer, bytesRemaining);
    }

    size_t offset = 0U;

    // Initially should check if client driver already filled layout info.
    bool preCheck = true;

    if (! ParseLayer2LayoutIfNeeded(Packet, buffer, offset, bytesRemaining, preCheck))
    {
        return destinationType;
    }

    if (! ParseLayer3LayoutIfNeeded(Packet, buffer, offset, bytesRemaining, preCheck))
    {
        return destinationType;
    }

    ParseLayer4LayoutIfNeeded(Packet, buffer, offset, bytesRemaining, preCheck);

    return destinationType;
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
