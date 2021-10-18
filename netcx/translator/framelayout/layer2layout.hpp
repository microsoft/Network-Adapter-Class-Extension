// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

namespace Layer2
{

bool
ParseLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

bool
ParseNullLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

enum class EthernetType : UINT16
{
    IPv4 = 0x0800,
    IPv6 = 0x86dd,
};

union Ethernet
{
    struct
    {
        UINT8
            DestinationAddress[6];
        UINT8
            SourceAddress[6];
        UINT16
            Type;
    } Header;

    static_assert(sizeof(Header) == 14U);

    UINT8
        Bytes[sizeof(Header)];
};

bool
ParseEthernetLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

union Ieee80211
{
    struct
    {
        UINT8
            Protocol:2;
        UINT8
            Type:2;
        UINT8
            SubType:4;
        UINT8
            ToDs:1;
        UINT8
            FromDs:1;
        UINT8
            MoreFrag:1;
        UINT8
            Retry:1;
        UINT8
            PowerManagement:1;
        UINT8
            MoreData:1;
        UINT8
            ProtectedFrame:1;
        UINT8
            Order:1;
        UINT16
            DurationId;
        UINT8
            Address1[6];
        UINT8
            Address2[6];
        UINT8
            Address3[6];
        UINT16
            SequenceControl;
    } Header;

    static_assert(sizeof(Header) == 24U);

    UINT8
        Bytes[sizeof(Header)];
};

//
// These IEEE 802.11 header size related constants are calculated based on
// header structures defined in onecore/net/published/inc/80211hdr.w
//
// Normal header size of IEEE 80211 data frame is 24 bytes, unless...
// 1) when both ToDs and FromDs are set, we need to add a 6-bytes Address4 field
#define DOT11_DATA_ADDRESS4_SIZE 6U

// 2) if SubType has DOT11_DATA_SUBTYPE_QOS_FLAG set, we need to add 2-bytes QoS control field
#define DOT11_DATA_SUBTYPE_QOS_FLAG 0x8
#define DOT11_DATA_QOS_CONTROL_SIZE 2U

bool
ParseIeee80211Layout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

union Ieee8022Llc
{
    struct
    {
        UINT8
            DestinationServiceAccessPoint;
        UINT8
            SourceServiceAccessPoint;
        UINT8
            Control;
        UINT8
            Oui[3];
        UINT16
            Type;
    } Header;

    static_assert(sizeof(Header) == 8U);

    UINT8
        Bytes[sizeof(Header)];
};

bool
ParseIeee8022LlcLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

}
