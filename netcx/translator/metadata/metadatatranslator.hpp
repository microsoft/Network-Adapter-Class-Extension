// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/ringcollection.h>

#include <net/packet.h>

#include <net/checksumtypes.h>
#include <net/lsotypes.h>
#include <net/rsctypes.h>

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
        NET_PACKET const & Packet
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

    void
    TranslatePacketChecksum(
        _In_ NET_BUFFER_LIST const & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketLso(
        _In_ NET_BUFFER_LIST const & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketLsoComplete(
        _In_ NET_BUFFER_LIST & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

    void
    TranslatePacketWifiExemptionAction(
        _In_ NET_BUFFER_LIST const & NetBufferList,
        _In_ UINT32 const PacketIndex
    ) const;

private:

    NET_RING_COLLECTION const &
        m_rings;

    TxExtensions const &
        m_extensions;
};

