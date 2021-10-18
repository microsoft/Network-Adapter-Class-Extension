// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

DECLARE_HANDLE( CHECKSUMLIB_SL_HANDLE );
DECLARE_HANDLE( CHECKSUMLIB_BL_HANDLE );

struct _NET_BUFFER;
typedef struct _NET_BUFFER NET_BUFFER;

struct _NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO;
typedef struct _NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO;

class Checksum
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    ~Checksum(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsIpv4ChecksumValid(
        _In_reads_(Length) UINT8 const * Buffer,
        _In_ size_t Length
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsUdpChecksumValid(
        _In_reads_(Length) UINT8 const *  Buffer,
        _In_ size_t Length
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsTcpChecksumValid(
        _In_reads_(Length) UINT8 const *  Buffer,
        _In_ size_t Length
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    CalculateChecksum(
        _Inout_ NET_BUFFER * Buffer,
        _In_ NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const & ChecksumInfo,
        _In_ NET_PACKET_LAYOUT const & PacketLayout
    );

private:

    CHECKSUMLIB_BL_HANDLE
        m_batchingLibContext = nullptr;

    CHECKSUMLIB_SL_HANDLE
        m_segLibContext = nullptr;

};
