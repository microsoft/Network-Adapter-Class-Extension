// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

namespace Layer3
{

bool
ParseLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

bool
ParseIPv4orIPv6Layout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

/*
 * Protocol Numbers
 * https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
 */
enum class Protocol : UINT8
{
    ICMPv4 = 0x01,
    TCP = 0x06,
    UDP = 0x11,
    IPv6Fragment = 0x2c,
    ICMPv6 = 0x3a,
    IPv6DestOptions = 0x3c,
};

union IPv4
{
    struct {
        UINT8
            HeaderLength:4;
        UINT8
            Version:4;
        UINT8
            DifferentiatedServices;
        UINT16
            TotalLength;
        UINT32
            Unused0;
        UINT8
            TimeToLive;
        UINT8
            Protocol;
        UINT16
            Checksum;
        UINT32
            Unused1;
        UINT32
            Unused2;
    } Header;

    static_assert(sizeof(Header) == 20U);

    UINT8
        Value[sizeof(Header)];

};

bool
ParseIPv4Layout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

/*
 * Next Header Types
 * IPv6 Extension Header Types
 * https://www.iana.org/assignments/ipv6-parameters/ipv6-parameters.xhtml
 */
enum class NextHeader : UINT8
{
    ICMPv4 = Protocol::ICMPv4,
    TCP = Protocol::TCP,
    UDP = Protocol::UDP,
    IPv6FragmentHeader = Protocol::IPv6Fragment, // extension header
    ICMPv6 = Protocol::ICMPv6,
    IPv6DestOptions = Protocol::IPv6DestOptions,
    NoNextHeader = 59,
};

union IPv6
{
    struct {
        union
        {
            struct {
                UINT8
                    Reserved1:4;
                UINT8
                    Version:4;
                UINT16
                    Reserved2;
            } VersionClassFlow;

            UINT32
                Reserved3;
        };

        UINT16
            Length;
        UINT8
            NextHeader;
        UINT8
            HopLimit;
        UINT64
            SourceAddress[2];
        UINT64
            DestinationAddress[2];
    } Header;

    static_assert(sizeof(Header) == 40U);

    UINT8
        Value[sizeof(Header)];
};

union IPv6Extension
{
    struct {
        UINT8
            NextHeader;
        UINT8
            Length;
        UINT16
            Reserved0;
        UINT32
            Reserved1;
    } Header;

    static_assert(sizeof(Header) == 8U);

    UINT8
        Value[sizeof(Header)];
};

bool
ParseIPv6Layout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

}
