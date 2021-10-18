// Copyright (C) Microsoft Corporation. All rights reserved.

// This file contains implementation to handle the checksum offload APIs released in Netcx 2.0.
// Do not change the functions in this file unless there is a bug in Netcx 2.0.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndis/oidrequest.h>

#include "NxOffload.hpp"
#include "NxOffloadV1.tmh"

#include "NxTranslationApp.hpp"

_Use_decl_annotations_
void
NxTaskOffload::InitializeChecksumV1(
    void
)
{
    //
    // Checksum hardware and default capabilities to indicate to NDIS
    //

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES checksumHardwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES),
    };

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES checksumDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES)
    };

    m_dispatch.GetChecksumHardwareCapabilities(m_app.GetAdapter(), &checksumHardwareCapabilities);
    m_hardwareChecksumCapabilities = checksumHardwareCapabilities;
    m_dispatch.GetChecksumDefaultCapabilities(m_app.GetAdapter(), &checksumDefaultCapabilities);
    m_activeChecksumCapabilities = checksumDefaultCapabilities;

    m_dispatch.SetChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeChecksumCapabilities);
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::CheckActiveChecksumV1Capabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    // Get*DefaultCapabilities() provides the intersection of hardware capabilities
    // advertised by the client driver and the capabilities stored in the registry
    // through advanced keywords. We need to ensure that we do not change the
    // capabilities when it is not consistent with the default values.

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES checksumCapabilities = {};
    m_dispatch.GetChecksumDefaultCapabilities(m_app.GetAdapter(), &checksumCapabilities);

    if (checksumCapabilities.IPv4 == FALSE &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (checksumCapabilities.Tcp == FALSE &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (checksumCapabilities.Udp == FALSE &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)
    {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
NxTaskOffload::TranslateChecksumCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES translatedCapabilties = m_activeChecksumCapabilities;

    if (OffloadParameters.IPv4Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED)
    {
        translatedCapabilties.IPv4 = FALSE;
    }
    else if (OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)
    {
        translatedCapabilties.IPv4 = TRUE;
    }

    if (OffloadParameters.TCPIPv4Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv6Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED)
    {
        translatedCapabilties.Tcp = FALSE;
    }
    else if (OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE ||
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)
    {
        translatedCapabilties.Tcp = TRUE;
    }

    if (OffloadParameters.UDPIPv4Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv6Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED)
    {
        translatedCapabilties.Udp = FALSE;
    }
    else if (OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE ||
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)
    {
        translatedCapabilties.Udp = TRUE;
    }

    return translatedCapabilties;
}

_Use_decl_annotations_
NDIS_TCP_IP_CHECKSUM_OFFLOAD
NxTaskOffload::TranslateChecksumCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const &Capabilities
) const
{
    NDIS_TCP_IP_CHECKSUM_OFFLOAD translatedCapabilties = {};

    const ULONG encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    translatedCapabilties.IPv4Transmit.Encapsulation = encapsulation;
    translatedCapabilties.IPv4Receive.Encapsulation = encapsulation;
    translatedCapabilties.IPv6Receive.Encapsulation = encapsulation;
    translatedCapabilties.IPv6Transmit.Encapsulation = encapsulation;

    if (Capabilities.IPv4)
    {
        translatedCapabilties.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv4Transmit.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv4Receive.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (Capabilities.Tcp)
    {
        translatedCapabilties.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv4Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv4Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv6Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv6Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv6Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv6Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv6Transmit.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv6Receive.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (Capabilities.Udp)
    {
        translatedCapabilties.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv6Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv6Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv6Transmit.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv6Receive.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    return translatedCapabilties;
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::SetActiveCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const & ChecksumCapabilities,
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const & LsoCapabilities,
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const & UsoCapabilities,
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const & RscCapabilities
)
{
    m_dispatch.SetChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &ChecksumCapabilities);

    m_activeChecksumCapabilities = ChecksumCapabilities;

    SetActiveGsoCapabilities(LsoCapabilities,
        UsoCapabilities);

    SetActiveRscCapabilities(RscCapabilities);

    //
    // For all offloads construct the NDIS_OFFLOAD structure to send to NDIS
    //

    NDIS_TCP_LARGE_SEND_OFFLOAD_V1 ndisLsoV1Capabilities = {};
    NDIS_TCP_LARGE_SEND_OFFLOAD_V2 ndisLsoV2Capabilities = {};
    NDIS_UDP_SEGMENTATION_OFFLOAD ndisUsoCapabilities = {};

    TranslateLsoCapabilities(
        LsoCapabilities,
        ndisLsoV1Capabilities,
        ndisLsoV2Capabilities);

    TranslateUsoCapabilities(
        UsoCapabilities,
        ndisUsoCapabilities);

    NDIS_TCP_RECV_SEG_COALESCE_OFFLOAD ndisRscCapabilities = {};

    TranslateRscCapabilities(
        RscCapabilities,
        ndisRscCapabilities);

    NDIS_OFFLOAD offloadCapabilities = {
        {
            NDIS_OBJECT_TYPE_OFFLOAD,
            NDIS_OFFLOAD_REVISION_6,
            NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_6
        },
        TranslateChecksumCapabilities(ChecksumCapabilities),
        ndisLsoV1Capabilities,
        {},
        ndisLsoV2Capabilities,
        0,
        {},
        ndisRscCapabilities,
        {},
        {},
        {},
        {},
        ndisUsoCapabilities
    };

    return SendNdisTaskOffloadStatusIndication(offloadCapabilities, false);
}

_Use_decl_annotations_
bool
NxTaskOffload::IsChecksumOffloadV1Used(
    void
) const
{
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES checksumHardwareCapabilities = {};
    m_dispatch.GetChecksumHardwareCapabilities(m_app.GetAdapter(), &checksumHardwareCapabilities);

    return checksumHardwareCapabilities.IPv4 ||
        checksumHardwareCapabilities.Tcp ||
        checksumHardwareCapabilities.Udp;
}
