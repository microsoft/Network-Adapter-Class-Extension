// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

namespace Layer4
{

union TCP
{
    struct {
        UINT16
            SourcePort;
        UINT16
            DestinationPort;
        UINT32
            SequenceNumber;
        UINT32
            AcknowledgementNumber;
        union {
            struct {
                UINT8
                    Reserved:4;
                UINT8
                    DataOffset:4;
            } DataOffset;
            struct {
                UINT16
                    Reserved:7;
                UINT16
                    ControlBits:6;
                UINT16
                    ExplicitCongestionControl:3;
            } ControlBits;
            UINT8
                Bytes[2];
        };
        UINT16
            Window;
        UINT16
            Checksum;
        UINT16
            UrgentPointer;
    } Header;

    static_assert(sizeof(Header) == 20U);

    UINT8
        Value[sizeof(Header)];

};

union UDP
{
    struct {
        UINT16
            SourcePort;
        UINT16
            DestinationPort;
        UINT16
            Length;
        UINT16
            Checksum;
    } Header;

    static_assert(sizeof(Header) == 8U);

    UINT8
        Value[sizeof(Header)];

};

bool
ParseLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

bool
ParseTcpLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

bool
ParseUdpLayout(
    _Inout_ NET_PACKET_LAYOUT & Layout,
    _In_ UINT8 const * Pointer,
    _Inout_ size_t & Offset,
    _Inout_ size_t & Length
);

}
