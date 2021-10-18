// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/ringcollection.h>

#include <net/packet.h>

#include <net/checksumtypes.h>
#include <net/gsotypes.h>
#include <net/rsctypes.h>
#include <net/ieee8021qtypes.h>

#include "NxExtensions.hpp"

class RxMetadataTranslator
{

public:

    RxMetadataTranslator(
        _In_ NET_RING_COLLECTION const & Rings,
        _In_ RxExtensions const & Extensions
    );

    void
    TranslatePacketChecksum(
        _In_ NET_BUFFER_LIST & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketRsc(
        _In_ NET_BUFFER_LIST & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketRscTimestamp(
        _In_ NET_BUFFER_LIST & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketLayout(
        _In_ NET_BUFFER_LIST & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    UINT16
    ParseLayer3Type(
        _In_ NET_PACKET const & Packet
    ) const;

    void
    TranslatePacketIeee8021q(
        _In_ NET_BUFFER_LIST & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketHash(
        _In_ NET_BUFFER_LIST & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

private:

    NET_RING_COLLECTION const &
        m_rings;

    RxExtensions const &
        m_extensions;
};

class TxMetadataTranslator
{

public:

    TxMetadataTranslator(
        _In_ NET_RING_COLLECTION const & Rings,
        _In_ TxExtensions const & Extensions
    );

    NET_PACKET_CHECKSUM
    TranslatePacketChecksum(
        _In_ NET_BUFFER_LIST const & NetBufferList
    ) const;

    void
    TranslatePacketChecksum(
        _In_ NET_BUFFER_LIST const & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    NET_PACKET_GSO
    TranslatePacketGso(
        _In_ NET_BUFFER_LIST const & NetBufferList
    ) const;

    void
    TranslatePacketGso(
        _In_ NET_BUFFER_LIST const & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketGsoComplete(
        _In_ NET_BUFFER_LIST & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketWifiExemptionAction(
        _In_ NET_BUFFER_LIST const & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    NET_PACKET_IEEE8021Q
    TranslatePacketIeee8021q(
        _In_ NET_BUFFER_LIST const & NetBufferList
    ) const;

    void
    TranslatePacketIeee8021q(
        _In_ NET_BUFFER_LIST const & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

private:

    NET_RING_COLLECTION const &
        m_rings;

    TxExtensions const &
        m_extensions;
};

