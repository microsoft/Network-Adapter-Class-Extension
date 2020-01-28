// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndis/oidrequesttypes.h>
#include <ndis/status_p.h>

#include "NxOffload.tmh"
#include "NxOffload.hpp"

#include "NxTranslationApp.hpp"

_Use_decl_annotations_
NxTaskOffload::NxTaskOffload(
    NxTranslationApp & App,
    NET_CLIENT_ADAPTER_OFFLOAD_DISPATCH const & Dispatch
) noexcept :
    m_app(App),
    m_dispatch(Dispatch)
{
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::Initialize(
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
    m_dispatch.GetChecksumDefaultCapabilities(m_app.GetAdapter(), &checksumDefaultCapabilities);
    m_activeChecksumCapabilities = checksumDefaultCapabilities;

    m_dispatch.SetChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeChecksumCapabilities);

    //
    // LSO hardware and default capabilities to indicate to NDIS
    //

    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES lsoHardwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_LSO_CAPABILITIES)
    };

    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES lsoDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_LSO_CAPABILITIES)
    };

    m_dispatch.GetLsoHardwareCapabilities(m_app.GetAdapter(), &lsoHardwareCapabilities);
    m_dispatch.GetLsoDefaultCapabilities(m_app.GetAdapter(), &lsoDefaultCapabilities);
    m_activeLsoCapabilities = lsoDefaultCapabilities;

    m_dispatch.SetLsoActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeLsoCapabilities);

    //
    // RSC hardware and default capabilities to indicate to NDIS
    //

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscHardwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES),
    };

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES)
    };

    m_dispatch.GetRscHardwareCapabilities(m_app.GetAdapter(), &rscHardwareCapabilities);
    m_dispatch.GetRscDefaultCapabilities(m_app.GetAdapter(), &rscDefaultCapabilities);
    m_activeRscCapabilities = rscDefaultCapabilities;

    m_dispatch.SetRscActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeRscCapabilities);

    //
    // Construct the NDIS_OFFLOAD structure encapsulating all offloads
    //

    NDIS_TCP_LARGE_SEND_OFFLOAD_V1 ndisLsoV1Capabilities = {};
    NDIS_TCP_LARGE_SEND_OFFLOAD_V2 ndisLsoV2Capabilities = {};

    TranslateLsoCapabilities(
        lsoHardwareCapabilities,
        ndisLsoV1Capabilities,
        ndisLsoV2Capabilities);

    NDIS_TCP_RECV_SEG_COALESCE_OFFLOAD ndisRscCapabilities = {};

    TranslateRscCapabilities(
        rscHardwareCapabilities,
        ndisRscCapabilities);

    NDIS_OFFLOAD hardwareCapabilties = {
        {
            NDIS_OBJECT_TYPE_OFFLOAD,
            NDIS_OFFLOAD_REVISION_5,
            NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_5
        },
        TranslateChecksumCapabilities(checksumHardwareCapabilities),
        ndisLsoV1Capabilities,
        {},
        ndisLsoV2Capabilities,
        0,
        {},
        ndisRscCapabilities
    };

    ndisLsoV1Capabilities = {};
    ndisLsoV2Capabilities = {};

    TranslateLsoCapabilities(
        lsoDefaultCapabilities,
        ndisLsoV1Capabilities,
        ndisLsoV2Capabilities);

    ndisRscCapabilities = {};

    TranslateRscCapabilities(
        rscDefaultCapabilities,
        ndisRscCapabilities
        );

    NDIS_OFFLOAD defaultCapabilties = {
        {
            NDIS_OBJECT_TYPE_OFFLOAD,
            NDIS_OFFLOAD_REVISION_5,
            NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_5
        },
        TranslateChecksumCapabilities(checksumDefaultCapabilities),
        ndisLsoV1Capabilities,
        {},
        ndisLsoV2Capabilities,
        0,
        {},
        ndisRscCapabilities
    };

    return SetNdisMiniportOffloadAttributes(hardwareCapabilties, defaultCapabilties); 
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::CheckActiveCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    // Get*DefaultCapabilities() provides the intersection of hardware capabilities
    // advertised by the client driver and the capabilities stored in the registry
    // through advanced keywords. We need to ensure that we do not change the 
    // capabilities when it is not consistent with the default values.

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES checksumCapabilities = {};
    m_dispatch.GetChecksumDefaultCapabilities(m_app.GetAdapter(), &checksumCapabilities);

    if ((checksumCapabilities.IPv4 == FALSE &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE) ||
        (checksumCapabilities.IPv4 == TRUE &&
        OffloadParameters.IPv4Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if ((checksumCapabilities.Tcp == FALSE && 
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE) ||
        ((checksumCapabilities.Tcp == TRUE) && 
        (OffloadParameters.TCPIPv4Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED ||
        OffloadParameters.TCPIPv6Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED)))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if ((checksumCapabilities.Udp == FALSE && 
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE) ||
        ((checksumCapabilities.Udp == TRUE) && 
        (OffloadParameters.UDPIPv4Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED ||
        OffloadParameters.UDPIPv6Checksum == NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED)))
    {
        return STATUS_NOT_SUPPORTED;
    }

    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES lsoCapabilities = {};
    m_dispatch.GetLsoDefaultCapabilities(m_app.GetAdapter(), &lsoCapabilities);

    if ((lsoCapabilities.IPv4 == FALSE && 
        OffloadParameters.LsoV1 == NDIS_OFFLOAD_PARAMETERS_LSOV1_ENABLED &&
        OffloadParameters.LsoV2IPv4 == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED) ||
        ((lsoCapabilities.IPv4 == TRUE) && 
        (OffloadParameters.LsoV1 == NDIS_OFFLOAD_PARAMETERS_LSOV1_DISABLED ||
        OffloadParameters.LsoV2IPv4 == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED)))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if ((lsoCapabilities.IPv6 == FALSE &&
        OffloadParameters.LsoV2IPv6 == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED) ||
        (lsoCapabilities.IPv6 == TRUE &&
        OffloadParameters.LsoV2IPv6 == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED))
    {
        return STATUS_NOT_SUPPORTED;
    }

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscCapabilities = {};
    m_dispatch.GetRscDefaultCapabilities(m_app.GetAdapter(), &rscCapabilities);

    if ((rscCapabilities.IPv4 == FALSE && 
        OffloadParameters.RscIPv4 == NDIS_OFFLOAD_PARAMETERS_RSC_ENABLED) ||
        (rscCapabilities.IPv4 == TRUE && 
        OffloadParameters.RscIPv4 == NDIS_OFFLOAD_PARAMETERS_RSC_DISABLED))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if ((rscCapabilities.IPv6 == FALSE && 
        OffloadParameters.RscIPv6 == NDIS_OFFLOAD_PARAMETERS_RSC_ENABLED) ||
        (rscCapabilities.IPv6 == TRUE && 
        OffloadParameters.RscIPv6 == NDIS_OFFLOAD_PARAMETERS_RSC_DISABLED))
    {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::SetActiveCapabilities(
    NDIS_OID_REQUEST const & Request
)
{
    auto const buffer = static_cast<unsigned char const *>(Request.DATA.SET_INFORMATION.InformationBuffer);
    auto const length = Request.DATA.SET_INFORMATION.InformationBufferLength;

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        length < NDIS_SIZEOF_OFFLOAD_PARAMETERS_REVISION_3,
        "InformationBufferLength too small.");

    auto const parameters = reinterpret_cast<NDIS_OFFLOAD_PARAMETERS const *>(buffer);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CheckActiveCapabilities(*parameters),
        "OID_TCP_OFFLOAD_PARAMETERS is trying to change capabilities to be inconsistent with the hardware and registry capabilities");

    return SetActiveCapabilities(
        TranslateChecksumCapabilities(*parameters),
        TranslateLsoCapabilities(*parameters),
        TranslateRscCapabilities(*parameters)
        );
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::SetNdisMiniportOffloadAttributes(
    NDIS_OFFLOAD &hardwareCapabilties,
    NDIS_OFFLOAD &defaultCapabilties
) const
{
#ifdef _KERNEL_MODE

    const NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES adapterOffload = {
        {
            NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES,
            NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1,
            NDIS_SIZEOF_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1
        },
        &defaultCapabilties,
        &hardwareCapabilties
    };

    NDIS_STATUS status = NdisMSetMiniportAttributes(m_app.GetProperties().NdisAdapterHandle,(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&adapterOffload);

    CX_RETURN_IF_NOT_NT_SUCCESS(NdisConvertNdisStatusToNtStatus(status));

#else

    UNREFERENCED_PARAMETER(hardwareCapabilties);
    UNREFERENCED_PARAMETER(defaultCapabilties);

#endif // _KERNEL_MODE

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS 
NxTaskOffload::SendNdisTaskOffloadStatusIndication(NDIS_OFFLOAD &offloadCapabilities) const
{
#ifdef _KERNEL_MODE

    NDIS_HANDLE handle = m_app.GetProperties().NdisAdapterHandle;

    NDIS_STATUS_INDICATION statusIndication;
    RtlZeroMemory(&statusIndication, sizeof(NDIS_STATUS_INDICATION));

    statusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    statusIndication.Header.Size = NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
    statusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;

    statusIndication.SourceHandle = handle;
    statusIndication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
    statusIndication.StatusBuffer = &offloadCapabilities;
    statusIndication.StatusBufferSize = sizeof(offloadCapabilities);

    NdisMIndicateStatusEx(handle, &statusIndication);

#else

    UNREFERENCED_PARAMETER(offloadCapabilities);

#endif // _KERNEL_MODE

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

    const ULONG encapsulation = NDIS_ENCAPSULATION_NULL |
        NDIS_ENCAPSULATION_IEEE_802_3 |
        NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q |
        NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q_IN_OOB |
        NDIS_ENCAPSULATION_IEEE_LLC_SNAP_ROUTED;

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
NET_CLIENT_OFFLOAD_LSO_CAPABILITIES
NxTaskOffload::TranslateLsoCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES translatedCapabilties = m_activeLsoCapabilities;

    if (OffloadParameters.LsoV1 == NDIS_OFFLOAD_PARAMETERS_LSOV1_ENABLED ||
        OffloadParameters.LsoV2IPv4 == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED)
    {
        translatedCapabilties.IPv4 = TRUE;
    }  
    else if (OffloadParameters.LsoV1 == NDIS_OFFLOAD_PARAMETERS_LSOV1_DISABLED &&
        OffloadParameters.LsoV2IPv4 == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED)
    {
        translatedCapabilties.IPv4 = FALSE;
    }

    if (OffloadParameters.LsoV2IPv6 == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED)
    {
        translatedCapabilties.IPv6 = TRUE;
    }
    else if (OffloadParameters.LsoV2IPv6 == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED)
    {
        translatedCapabilties.IPv6 = FALSE;
    }

    return translatedCapabilties;
}

_Use_decl_annotations_
void
NxTaskOffload::TranslateLsoCapabilities(
    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES const &Capabilities,
    NDIS_TCP_LARGE_SEND_OFFLOAD_V1 &NdisLsoV1Capabilities,
    NDIS_TCP_LARGE_SEND_OFFLOAD_V2 &NdisLsoV2Capabilities
) const
{
    const ULONG encapsulation = NDIS_ENCAPSULATION_NULL |
        NDIS_ENCAPSULATION_IEEE_802_3 |
        NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q |
        NDIS_ENCAPSULATION_IEEE_802_3_P_AND_Q_IN_OOB |
        NDIS_ENCAPSULATION_IEEE_LLC_SNAP_ROUTED;

    if (Capabilities.IPv4)
    {
        NdisLsoV1Capabilities.IPv4.Encapsulation = encapsulation;
        NdisLsoV1Capabilities.IPv4.MaxOffLoadSize = static_cast<ULONG>(Capabilities.MaximumOffloadSize);
        NdisLsoV1Capabilities.IPv4.MinSegmentCount = static_cast<ULONG>(Capabilities.MinimumSegmentCount);
        NdisLsoV1Capabilities.IPv4.TcpOptions  = NDIS_OFFLOAD_SUPPORTED;
        NdisLsoV1Capabilities.IPv4.IpOptions  = NDIS_OFFLOAD_SUPPORTED; 

        NdisLsoV2Capabilities.IPv4.Encapsulation = encapsulation;
        NdisLsoV2Capabilities.IPv4.MaxOffLoadSize = static_cast<ULONG>(Capabilities.MaximumOffloadSize);
        NdisLsoV2Capabilities.IPv4.MinSegmentCount = static_cast<ULONG>(Capabilities.MinimumSegmentCount);
    }

    if (Capabilities.IPv6)
    {
        NdisLsoV2Capabilities.IPv6.Encapsulation = encapsulation;
        NdisLsoV2Capabilities.IPv6.MaxOffLoadSize = static_cast<ULONG>(Capabilities.MaximumOffloadSize);
        NdisLsoV2Capabilities.IPv6.MinSegmentCount = static_cast<ULONG>(Capabilities.MinimumSegmentCount);
        NdisLsoV2Capabilities.IPv6.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
        NdisLsoV2Capabilities.IPv6.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED; 
    }
}

_Use_decl_annotations_
NET_CLIENT_OFFLOAD_RSC_CAPABILITIES
NxTaskOffload::TranslateRscCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES translatedCapabilities = m_activeRscCapabilities;

    if (OffloadParameters.RscIPv4 != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)
    {
        translatedCapabilities.IPv4 = OffloadParameters.RscIPv4 == NDIS_OFFLOAD_PARAMETERS_RSC_ENABLED;
    }

    if (OffloadParameters.RscIPv6 != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE)
    {
        translatedCapabilities.IPv6 = OffloadParameters.RscIPv6 == NDIS_OFFLOAD_PARAMETERS_RSC_ENABLED;
    }

    return translatedCapabilities;
}

_Use_decl_annotations_
void
NxTaskOffload::TranslateRscCapabilities(
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const &Capabilities,
    NDIS_TCP_RECV_SEG_COALESCE_OFFLOAD &NdisRscCapabilities
) const
{
    NdisRscCapabilities.IPv4.Enabled = Capabilities.IPv4;
    NdisRscCapabilities.IPv6.Enabled = Capabilities.IPv6;
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::SetActiveCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const & ChecksumCapabilities,
    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES const & LsoCapabilities,
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const & RscCapabilities
)
{
    m_dispatch.SetChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &ChecksumCapabilities);

    m_activeChecksumCapabilities = ChecksumCapabilities;

    m_dispatch.SetLsoActiveCapabilities(
        m_app.GetAdapter(),
        &LsoCapabilities);

    m_activeLsoCapabilities = LsoCapabilities;

    m_dispatch.SetRscActiveCapabilities(
        m_app.GetAdapter(),
        &RscCapabilities);

    m_activeRscCapabilities = RscCapabilities;

    //
    // For all offloads construct the NDIS_OFFLOAD structure to send to NDIS
    //

    NDIS_TCP_LARGE_SEND_OFFLOAD_V1 ndisLsoV1Capabilities = {};
    NDIS_TCP_LARGE_SEND_OFFLOAD_V2 ndisLsoV2Capabilities = {};

    TranslateLsoCapabilities(
        LsoCapabilities,
        ndisLsoV1Capabilities,
        ndisLsoV2Capabilities
        );

    NDIS_TCP_RECV_SEG_COALESCE_OFFLOAD ndisRscCapabilities = {};

    TranslateRscCapabilities(
        RscCapabilities,
        ndisRscCapabilities);

    NDIS_OFFLOAD offloadCapabilties = {
        {
            NDIS_OBJECT_TYPE_OFFLOAD,
            NDIS_OFFLOAD_REVISION_5,
            NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_5
        },
        TranslateChecksumCapabilities(ChecksumCapabilities),
        ndisLsoV1Capabilities,
        {},
        ndisLsoV2Capabilities,
        0,
        {},
        ndisRscCapabilities
    };

    return SendNdisTaskOffloadStatusIndication(offloadCapabilties);
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::SetEncapsulation(
    NDIS_OID_REQUEST const & Request
)
{
    auto const buffer = static_cast<unsigned char const *>(Request.DATA.SET_INFORMATION.InformationBuffer);
    auto const length = Request.DATA.SET_INFORMATION.InformationBufferLength;

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        length < NDIS_SIZEOF_OFFLOAD_ENCAPSULATION_REVISION_1,
        "InformationBufferLength too small.");

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        ! (buffer + length > buffer),
        "InformationBuffer + InformationBufferLength results in integer overflow.");

    auto const encapsulation = reinterpret_cast<NDIS_OFFLOAD_ENCAPSULATION const *>(buffer);
    auto const offloadsSupported = (IsChecksumOffloadSupported() || IsLsoOffloadSupported() || IsRscOffloadSupported());

    if (!offloadsSupported)
    {
        //
        // Offloads are not supported
        //

        return STATUS_NOT_SUPPORTED;
    }

    if (encapsulation->IPv4.Enabled == NDIS_OFFLOAD_SET_OFF && 
        encapsulation->IPv6.Enabled == NDIS_OFFLOAD_SET_OFF)
    {
        return SetActiveCapabilities(
            {
                sizeof(NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES),
                FALSE,
                FALSE,
                FALSE
            },
            {
                sizeof(NET_CLIENT_OFFLOAD_LSO_CAPABILITIES),
                FALSE,
                FALSE,
                m_activeLsoCapabilities.MaximumOffloadSize,
                m_activeLsoCapabilities.MinimumSegmentCount,
            },
            {
                sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES),
                FALSE,
                FALSE,
                FALSE
            });
    }

    if (encapsulation->IPv4.Enabled == NDIS_OFFLOAD_SET_ON && 
        encapsulation->IPv6.Enabled == NDIS_OFFLOAD_SET_ON)
    {
        return SetActiveCapabilities(m_activeChecksumCapabilities,
        m_activeLsoCapabilities,
        m_activeRscCapabilities
        );
    }

    if (encapsulation->IPv6.Enabled == NDIS_OFFLOAD_SET_OFF)
    {
        return SetActiveCapabilities(
            m_activeChecksumCapabilities,
            {
                sizeof(NET_CLIENT_OFFLOAD_LSO_CAPABILITIES),
                m_activeLsoCapabilities.IPv4,
                false,
                m_activeLsoCapabilities.MaximumOffloadSize,
                m_activeLsoCapabilities.MinimumSegmentCount
            },
            {
                sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES),
                m_activeRscCapabilities.IPv4,
                false,
                m_activeRscCapabilities.Timestamp
            });
    }

    if (encapsulation->IPv4.Enabled == NDIS_OFFLOAD_SET_OFF)
    {
        return SetActiveCapabilities(
            {
                sizeof(NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES),
                false,
                m_activeChecksumCapabilities.Tcp,
                m_activeChecksumCapabilities.Udp
            },
            {
                sizeof(NET_CLIENT_OFFLOAD_LSO_CAPABILITIES),
                false,
                m_activeLsoCapabilities.IPv6,
                m_activeLsoCapabilities.MaximumOffloadSize,
                m_activeLsoCapabilities.MinimumSegmentCount
            },
            {
                sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES),
                false,
                m_activeRscCapabilities.IPv6,
                m_activeRscCapabilities.Timestamp
            });
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
bool
NxTaskOffload::IsChecksumOffloadSupported(
    void
) const
{
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES checksumHardwareCapabilities = {};
    m_dispatch.GetChecksumHardwareCapabilities(m_app.GetAdapter(), &checksumHardwareCapabilities);

    return checksumHardwareCapabilities.IPv4 ||
        checksumHardwareCapabilities.Tcp ||
        checksumHardwareCapabilities.Udp;
}

_Use_decl_annotations_
bool
NxTaskOffload::IsLsoOffloadSupported(
    void
) const
{
    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES lsoHardwareCapabilities = {};
    m_dispatch.GetLsoHardwareCapabilities(m_app.GetAdapter(), &lsoHardwareCapabilities);

    return lsoHardwareCapabilities.IPv4 || lsoHardwareCapabilities.IPv6;
}

_Use_decl_annotations_
bool
NxTaskOffload::IsRscOffloadSupported(
    void
) const
{
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscHardwareCapabilities = {};
    m_dispatch.GetRscHardwareCapabilities(m_app.GetAdapter(), &rscHardwareCapabilities);

    return rscHardwareCapabilities.IPv4 || rscHardwareCapabilities.IPv6;
}
