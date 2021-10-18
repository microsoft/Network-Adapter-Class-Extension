// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndis/oidrequest.h>
#include <ndis/statusconvert.h>
#include <ndis/encapsulationconfig.h>

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
    if (IsChecksumOffloadV1Used())
    {
        InitializeChecksumV1();
    }
    else
    {
        InitializeTxChecksum();
        InitializeRxChecksum();
    }

    InitializeGso();
    InitializeRsc();

    // Construct the NDIS_OFFLOAD structure encapsulating all offloads
    NDIS_OFFLOAD hardwareCapabilities = {};
    TranslateHardwareCapabilities(hardwareCapabilities);
    NDIS_OFFLOAD defaultCapabilities = {};
    TranslateActiveCapabilities(defaultCapabilities);

    return SetNdisMiniportOffloadAttributes(hardwareCapabilities, defaultCapabilities);
}

_Use_decl_annotations_
void
NxTaskOffload::TranslateHardwareCapabilities(
    NDIS_OFFLOAD & HardwareCapabilities
) const
{
    NDIS_TCP_LARGE_SEND_OFFLOAD_V1 ndisLsoV1Capabilities = {};
    NDIS_TCP_LARGE_SEND_OFFLOAD_V2 ndisLsoV2Capabilities = {};
    NDIS_UDP_SEGMENTATION_OFFLOAD ndisUsoCapabilities = {};

    TranslateLsoCapabilities(
        m_softwareGsoCapabilities,
        ndisLsoV1Capabilities,
        ndisLsoV2Capabilities);

    TranslateUsoCapabilities(
        m_softwareGsoCapabilities,
        ndisUsoCapabilities);

    NDIS_TCP_RECV_SEG_COALESCE_OFFLOAD ndisRscCapabilities = {};

    TranslateRscCapabilities(
        m_softwareRscCapabilities,
        ndisRscCapabilities);

    if (IsChecksumOffloadV1Used())
    {
        HardwareCapabilities = {
            {
                NDIS_OBJECT_TYPE_OFFLOAD,
                NDIS_OFFLOAD_REVISION_6,
                NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_6
            },
            TranslateChecksumCapabilities(
                m_hardwareChecksumCapabilities),
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
    }
    else
    {
        HardwareCapabilities = {
            {
                NDIS_OBJECT_TYPE_OFFLOAD,
                NDIS_OFFLOAD_REVISION_6,
                NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_6
            },
            TranslateChecksumCapabilities(
                m_softwareTxChecksumCapabilities,
                m_softwareTxChecksumCapabilities,
                m_softwareTxChecksumCapabilities,
                m_hardwareRxChecksumCapabilities),
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
    }
}

_Use_decl_annotations_
void
NxTaskOffload::TranslateActiveCapabilities(
    NDIS_OFFLOAD & ActiveCapabilities
) const
{
    NDIS_TCP_LARGE_SEND_OFFLOAD_V1 ndisLsoV1Capabilities = {};
    NDIS_TCP_LARGE_SEND_OFFLOAD_V2 ndisLsoV2Capabilities = {};
    NDIS_UDP_SEGMENTATION_OFFLOAD ndisUsoCapabilities = {};

    TranslateLsoCapabilities(
        m_activeLsoCapabilities,
        ndisLsoV1Capabilities,
        ndisLsoV2Capabilities);

    TranslateUsoCapabilities(
        m_activeUsoCapabilities,
        ndisUsoCapabilities);

    NDIS_TCP_RECV_SEG_COALESCE_OFFLOAD ndisRscCapabilities = {};

    TranslateRscCapabilities(
        m_activeRscCapabilities,
        ndisRscCapabilities);

    if (IsChecksumOffloadV1Used())
    {
        ActiveCapabilities = {
            {
                NDIS_OBJECT_TYPE_OFFLOAD,
                NDIS_OFFLOAD_REVISION_6,
                NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_6
            },
            TranslateChecksumCapabilities(
                m_activeChecksumCapabilities),
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
    }
    else
    {
        ActiveCapabilities = {
            {
                NDIS_OBJECT_TYPE_OFFLOAD,
                NDIS_OFFLOAD_REVISION_6,
                NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_6
            },
            TranslateChecksumCapabilities(
                m_activeTxIPv4ChecksumCapabilities,
                m_activeTxTcpChecksumCapabilities,
                m_activeTxUdpChecksumCapabilities,
                m_activeRxChecksumCapabilities),
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
    }
}

_Use_decl_annotations_
void
NxTaskOffload::InitializeTxChecksum(
    void
)
{
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txChecksumHardwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES),
    };

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txIPv4ChecksumDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES)
    };

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txTcpChecksumDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES)
    };

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txUdpChecksumDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES)
    };

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txChecksumSoftwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES),
    };

    m_dispatch.GetTxChecksumHardwareCapabilities(m_app.GetAdapter(), &txChecksumHardwareCapabilities);
    m_hardwareTxChecksumCapabilities = txChecksumHardwareCapabilities;
    m_dispatch.GetTxChecksumSoftwareCapabilities(m_app.GetAdapter(), &txChecksumSoftwareCapabilities);
    m_softwareTxChecksumCapabilities = txChecksumSoftwareCapabilities;
    m_dispatch.GetTxIPv4ChecksumDefaultCapabilities(m_app.GetAdapter(), &txIPv4ChecksumDefaultCapabilities);
    m_activeTxIPv4ChecksumCapabilities = txIPv4ChecksumDefaultCapabilities;
    m_dispatch.GetTxTcpChecksumDefaultCapabilities(m_app.GetAdapter(), &txTcpChecksumDefaultCapabilities);
    m_activeTxTcpChecksumCapabilities = txTcpChecksumDefaultCapabilities;
    m_dispatch.GetTxUdpChecksumDefaultCapabilities(m_app.GetAdapter(), &txUdpChecksumDefaultCapabilities);
    m_activeTxUdpChecksumCapabilities = txUdpChecksumDefaultCapabilities;

    m_dispatch.SetTxChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeTxIPv4ChecksumCapabilities,
        &m_activeTxTcpChecksumCapabilities,
        &m_activeTxUdpChecksumCapabilities);
}

_Use_decl_annotations_
void
NxTaskOffload::InitializeRxChecksum(
    void
)
{
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES rxChecksumHardwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES)
    };

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES rxChecksumDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES)
    };

    m_dispatch.GetRxChecksumHardwareCapabilities(m_app.GetAdapter(), &rxChecksumHardwareCapabilities);
    m_hardwareRxChecksumCapabilities = rxChecksumHardwareCapabilities;
    m_dispatch.GetRxChecksumDefaultCapabilities(m_app.GetAdapter(), &rxChecksumDefaultCapabilities);
    m_activeRxChecksumCapabilities = rxChecksumDefaultCapabilities;

    m_dispatch.SetRxChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeRxChecksumCapabilities);
}

_Use_decl_annotations_
void
NxTaskOffload::InitializeGso(
    void
)
{
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES gsoHardwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_GSO_CAPABILITIES)
    };


    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES gsoSoftwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_GSO_CAPABILITIES),
    };

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES lsoDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_GSO_CAPABILITIES)
    };

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES usoDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_GSO_CAPABILITIES)
    };

    m_dispatch.GetGsoHardwareCapabilities(m_app.GetAdapter(), &gsoHardwareCapabilities);
    m_hardwareGsoCapabilities = gsoHardwareCapabilities;
    m_dispatch.GetGsoSoftwareCapabilities(m_app.GetAdapter(), &gsoSoftwareCapabilities);
    m_softwareGsoCapabilities = gsoSoftwareCapabilities;
    m_dispatch.GetLsoDefaultCapabilities(m_app.GetAdapter(), &lsoDefaultCapabilities);
    m_activeLsoCapabilities = lsoDefaultCapabilities;
    m_dispatch.GetUsoDefaultCapabilities(m_app.GetAdapter(), &usoDefaultCapabilities);
    m_activeUsoCapabilities = usoDefaultCapabilities;

    m_dispatch.SetGsoActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeLsoCapabilities,
        &m_activeUsoCapabilities);
}

_Use_decl_annotations_
void
NxTaskOffload::InitializeRsc(
    void
)
{
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscHardwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES),
    };

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscSoftwareCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES),
    };

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscDefaultCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES)
    };

    m_dispatch.GetRscHardwareCapabilities(m_app.GetAdapter(), &rscHardwareCapabilities);
    m_hardwareRscCapabilities = rscHardwareCapabilities;
    m_dispatch.GetRscSoftwareCapabilities(m_app.GetAdapter(), &rscSoftwareCapabilities);
    m_softwareRscCapabilities = rscSoftwareCapabilities;
    m_dispatch.GetRscDefaultCapabilities(m_app.GetAdapter(), &rscDefaultCapabilities);
    m_activeRscCapabilities = rscDefaultCapabilities;

    m_dispatch.SetRscActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeRscCapabilities);
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::CheckActiveCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    if (IsChecksumOffloadV1Used())
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            CheckActiveChecksumV1Capabilities(OffloadParameters),
            "Checksum V1 offload parameters are invalid");
    }
    else
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            CheckActiveTxChecksumCapabilities(OffloadParameters),
            "Tx Checksum offload parameters are invalid");

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            CheckActiveRxChecksumCapabilities(OffloadParameters),
            "Rx Checksum offload parameters are invalid");
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CheckActiveGsoCapabilities(OffloadParameters),
        "Gso offload parameters are invalid");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CheckActiveRscCapabilities(OffloadParameters),
        "Rsc offload parameters are invalid");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::CheckActiveTxChecksumCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    // Get*DefaultCapabilities() provides the intersection of hardware capabilities
    // advertised by the client driver and the capabilities stored in the registry
    // through advanced keywords. We need to ensure that we do not change the
    // capabilities when it is not consistent with the default values.

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txIPv4ChecksumCapabilities = {};
    m_dispatch.GetTxIPv4ChecksumDefaultCapabilities(m_app.GetAdapter(), &txIPv4ChecksumCapabilities);

    if (WI_IsFlagClear(txIPv4ChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions) &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txTcpChecksumCapabilities = {};
    m_dispatch.GetTxTcpChecksumDefaultCapabilities(m_app.GetAdapter(), &txTcpChecksumCapabilities);

    if (WI_IsFlagClear(txTcpChecksumCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions) &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txUdpChecksumCapabilities = {};
    m_dispatch.GetTxUdpChecksumDefaultCapabilities(m_app.GetAdapter(), &txUdpChecksumCapabilities);

    if (WI_IsFlagClear(txUdpChecksumCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp) &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::CheckActiveRxChecksumCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES rxChecksumCapabilities = {};
    m_dispatch.GetRxChecksumDefaultCapabilities(m_app.GetAdapter(), &rxChecksumCapabilities);

    if (rxChecksumCapabilities.IPv4 == FALSE &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.IPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (rxChecksumCapabilities.Tcp == FALSE &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.TCPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED &&
        OffloadParameters.TCPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (rxChecksumCapabilities.Udp == FALSE &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_NO_CHANGE &&
        OffloadParameters.UDPIPv4Checksum != NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED &&
        OffloadParameters.UDPIPv6Checksum != NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::CheckActiveGsoCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES lsoCapabilities = {};
    m_dispatch.GetLsoDefaultCapabilities(m_app.GetAdapter(), &lsoCapabilities);

    if (WI_IsFlagSet(lsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(lsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions) &&
        WI_IsFlagSet(lsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions) &&
        (OffloadParameters.LsoV1 == NDIS_OFFLOAD_PARAMETERS_LSOV1_ENABLED ||
        OffloadParameters.LsoV2IPv4 == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(lsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions) &&
        WI_IsFlagClear(lsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions) &&
        OffloadParameters.LsoV2IPv6 == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES usoCapabilities = {};
    m_dispatch.GetUsoDefaultCapabilities(m_app.GetAdapter(), &usoCapabilities);

    auto const tcpFlags = NetClientAdapterOffloadLayer4FlagTcpNoOptions |
        NetClientAdapterOffloadLayer4FlagTcpWithOptions;

    if (WI_IsAnyFlagSet(usoCapabilities.Layer4Flags, tcpFlags))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(usoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions) &&
        WI_IsFlagClear(usoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp) &&
        OffloadParameters.UdpSegmentation.IPv4 == NDIS_OFFLOAD_PARAMETERS_USO_ENABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(usoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions) &&
        WI_IsFlagClear(usoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp) &&
        OffloadParameters.UdpSegmentation.IPv6 == NDIS_OFFLOAD_PARAMETERS_USO_ENABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::CheckActiveRscCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscCapabilities = {};
    m_dispatch.GetRscDefaultCapabilities(m_app.GetAdapter(), &rscCapabilities);

    if (rscCapabilities.IPv4 == FALSE &&
        OffloadParameters.RscIPv4 == NDIS_OFFLOAD_PARAMETERS_RSC_ENABLED)
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (rscCapabilities.IPv6 == FALSE &&
        OffloadParameters.RscIPv6 == NDIS_OFFLOAD_PARAMETERS_RSC_ENABLED)
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

    if (IsChecksumOffloadV1Used())
    {
        return SetActiveCapabilities(
            TranslateChecksumCapabilities(*parameters),
            TranslateLsoCapabilities(*parameters),
            TranslateUsoCapabilities(*parameters),
            TranslateRscCapabilities(*parameters));
    }
    else
    {
        NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txIPv4ChecksumCapabilities = {};
        NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txTcpChecksumCapabilities = {};
        NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txUdpChecksumCapabilities = {};
        NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES rxChecksumCapabilities = {};

        TranslateChecksumCapabilities(*parameters,
            &txIPv4ChecksumCapabilities,
            &txTcpChecksumCapabilities,
            &txUdpChecksumCapabilities,
            &rxChecksumCapabilities);

        return SetActiveCapabilities(txIPv4ChecksumCapabilities,
            txTcpChecksumCapabilities,
            txUdpChecksumCapabilities,
            rxChecksumCapabilities,
            TranslateLsoCapabilities(*parameters),
            TranslateUsoCapabilities(*parameters),
            TranslateRscCapabilities(*parameters));
    }
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
NxTaskOffload::SendNdisTaskOffloadStatusIndication(
    NDIS_OFFLOAD & OffloadCapabilities,
    bool IsHardwareConfig
) const
{
#ifdef _KERNEL_MODE

    NDIS_HANDLE handle = m_app.GetProperties().NdisAdapterHandle;

    NDIS_STATUS_INDICATION statusIndication;
    RtlZeroMemory(&statusIndication, sizeof(NDIS_STATUS_INDICATION));

    statusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    statusIndication.Header.Size = NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
    statusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;

    statusIndication.SourceHandle = handle;
    statusIndication.StatusBuffer = &OffloadCapabilities;
    statusIndication.StatusBufferSize = sizeof(OffloadCapabilities);

    if (IsHardwareConfig)
    {
        statusIndication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_HARDWARE_CAPABILITIES;
    }
    else
    {
        statusIndication.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
    }

    NdisMIndicateStatusEx(handle, &statusIndication);

#else

    UNREFERENCED_PARAMETER(OffloadCapabilities);
    UNREFERENCED_PARAMETER(IsHardwareConfig);

#endif // _KERNEL_MODE

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxTaskOffload::TranslateChecksumCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * TxIPv4TranslatedCapabilties,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * TxTcpTranslatedCapabilties,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * TxUdpTranslatedCapabilties,
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * RxTranslatedCapabilties
) const
{
    *TxIPv4TranslatedCapabilties = m_activeTxIPv4ChecksumCapabilities;
    *TxTcpTranslatedCapabilties = m_activeTxTcpChecksumCapabilities;
    *TxUdpTranslatedCapabilties = m_activeTxUdpChecksumCapabilities;
    *RxTranslatedCapabilties = m_activeRxChecksumCapabilities;

    auto const ipv4flags =  NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
        NetClientAdapterOffloadLayer3FlagIPv4WithOptions;

    switch (OffloadParameters.IPv4Checksum)
    {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            WI_ClearAllFlags(TxIPv4TranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->IPv4 = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            WI_SetAllFlags(TxIPv4TranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->IPv4 = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            WI_ClearAllFlags(TxIPv4TranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->IPv4 = TRUE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            WI_SetAllFlags(TxIPv4TranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->IPv4 = TRUE;
            break;
    }

    auto const tcpflags = NetClientAdapterOffloadLayer4FlagTcpNoOptions |
        NetClientAdapterOffloadLayer4FlagTcpWithOptions;

    switch (OffloadParameters.TCPIPv4Checksum | OffloadParameters.TCPIPv6Checksum)
    {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            WI_ClearAllFlags(TxTcpTranslatedCapabilties->Layer4Flags, tcpflags);
            RxTranslatedCapabilties->Tcp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            WI_SetAllFlags(TxTcpTranslatedCapabilties->Layer4Flags, tcpflags);
            RxTranslatedCapabilties->Tcp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            WI_ClearAllFlags(TxTcpTranslatedCapabilties->Layer4Flags, tcpflags);
            RxTranslatedCapabilties->Tcp = TRUE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            WI_SetAllFlags(TxTcpTranslatedCapabilties->Layer4Flags, tcpflags);
            RxTranslatedCapabilties->Tcp = TRUE;
            break;
    }

    switch (OffloadParameters.TCPIPv4Checksum)
    {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            WI_ClearAllFlags(TxTcpTranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->Tcp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            WI_SetAllFlags(TxTcpTranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->Tcp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            WI_ClearAllFlags(TxTcpTranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->Tcp = TRUE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            WI_ClearAllFlags(TxTcpTranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->Tcp = TRUE;
            break;
    }

    auto const ipv6flags = NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
        NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

    switch (OffloadParameters.TCPIPv6Checksum)
    {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            WI_ClearAllFlags(TxTcpTranslatedCapabilties->Layer3Flags, ipv6flags);
            RxTranslatedCapabilties->Tcp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            WI_SetAllFlags(TxTcpTranslatedCapabilties->Layer3Flags, ipv6flags);
            RxTranslatedCapabilties->Tcp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            WI_ClearAllFlags(TxTcpTranslatedCapabilties->Layer3Flags, ipv6flags);
            RxTranslatedCapabilties->Tcp = TRUE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            WI_SetAllFlags(TxTcpTranslatedCapabilties->Layer3Flags, ipv6flags);
            RxTranslatedCapabilties->Tcp = TRUE;
            break;
    }

    switch (OffloadParameters.UDPIPv4Checksum | OffloadParameters.UDPIPv6Checksum)
    {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            WI_ClearFlag(TxUdpTranslatedCapabilties->Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp);
            RxTranslatedCapabilties->Udp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            WI_SetFlag(TxUdpTranslatedCapabilties->Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp);
            RxTranslatedCapabilties->Udp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            WI_ClearFlag(TxUdpTranslatedCapabilties->Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp);
            RxTranslatedCapabilties->Udp = TRUE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            WI_SetFlag(TxUdpTranslatedCapabilties->Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp);
            RxTranslatedCapabilties->Udp = TRUE;
            break;
    }

    switch (OffloadParameters.UDPIPv4Checksum)
    {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            WI_ClearAllFlags(TxUdpTranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->Udp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            WI_SetAllFlags(TxUdpTranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->Udp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            WI_ClearAllFlags(TxUdpTranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->Udp = TRUE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            WI_SetAllFlags(TxUdpTranslatedCapabilties->Layer3Flags, ipv4flags);
            RxTranslatedCapabilties->Udp = TRUE;
            break;
    }

    switch (OffloadParameters.UDPIPv6Checksum)
    {
        case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
            WI_ClearAllFlags(TxUdpTranslatedCapabilties->Layer3Flags, ipv6flags);
            RxTranslatedCapabilties->Udp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
            WI_SetAllFlags(TxUdpTranslatedCapabilties->Layer3Flags, ipv6flags);
            RxTranslatedCapabilties->Udp = FALSE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
            WI_ClearAllFlags(TxUdpTranslatedCapabilties->Layer3Flags, ipv6flags);
            RxTranslatedCapabilties->Udp = TRUE;
            break;

        case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
            WI_SetAllFlags(TxUdpTranslatedCapabilties->Layer3Flags, ipv6flags);
            RxTranslatedCapabilties->Udp = TRUE;
            break;
    }
}

_Use_decl_annotations_
NDIS_TCP_IP_CHECKSUM_OFFLOAD
NxTaskOffload::TranslateChecksumCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const &TxIPv4Capabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const &TxTcpCapabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const &TxUdpCapabilities,
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const &RxCapabilities
) const
{
    NDIS_TCP_IP_CHECKSUM_OFFLOAD translatedCapabilties = {};

    const ULONG encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    translatedCapabilties.IPv4Transmit.Encapsulation = encapsulation;
    translatedCapabilties.IPv4Receive.Encapsulation = encapsulation;
    translatedCapabilties.IPv6Receive.Encapsulation = encapsulation;
    translatedCapabilties.IPv6Transmit.Encapsulation = encapsulation;

    if (WI_IsFlagSet(TxIPv4Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        translatedCapabilties.IPv4Transmit.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
    }

    if (WI_IsFlagSet(TxIPv4Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions))
    {
        translatedCapabilties.IPv4Transmit.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (WI_IsFlagSet(TxTcpCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        if (WI_IsFlagSet(TxTcpCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions))
        {
            translatedCapabilties.IPv4Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        }

        if (WI_IsFlagSet(TxTcpCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions))
        {
            translatedCapabilties.IPv4Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
        }
    }

    if (WI_IsFlagSet(TxUdpCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        if (WI_IsFlagSet(TxUdpCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
        {
            translatedCapabilties.IPv4Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
        }
    }

    if (WI_IsFlagSet(TxTcpCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
    {
        if (WI_IsFlagSet(TxTcpCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions))
        {
            translatedCapabilties.IPv6Transmit.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        }

        if (WI_IsFlagSet(TxTcpCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions))
        {
            translatedCapabilties.IPv6Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
        }
    }

    if (WI_IsFlagSet(TxUdpCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
    {
        if (WI_IsFlagSet(TxUdpCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
        {
            translatedCapabilties.IPv6Transmit.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
        }
    }

    if (WI_IsFlagSet(TxTcpCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions) ||
        WI_IsFlagSet(TxUdpCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions))
    {
        translatedCapabilties.IPv6Transmit.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (RxCapabilities.IPv4)
    {
        translatedCapabilties.IPv4Receive.IpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv4Receive.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (RxCapabilities.Tcp)
    {
        translatedCapabilties.IPv4Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv4Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv6Receive.TcpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv6Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv6Receive.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    if (RxCapabilities.Udp)
    {
        translatedCapabilties.IPv4Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;
        translatedCapabilties.IPv6Receive.UdpChecksum = NDIS_OFFLOAD_SUPPORTED;

        translatedCapabilties.IPv6Receive.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
    }

    return translatedCapabilties;
}

_Use_decl_annotations_
NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
NxTaskOffload::TranslateLsoCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES translatedLsoCapabilities = m_activeLsoCapabilities;

    // We have overridden adapter's hardware capabilities to be all enabled.
    // So LSO is supported.
    auto const ipv4Flags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
        NetClientAdapterOffloadLayer3FlagIPv4WithOptions;

    auto const ipv6Flags = NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
        NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

    if (OffloadParameters.LsoV1 == NDIS_OFFLOAD_PARAMETERS_LSOV1_DISABLED &&
        OffloadParameters.LsoV2IPv4 == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED)
    {
        WI_ClearAllFlags(translatedLsoCapabilities.Layer3Flags, ipv4Flags);
    }
    else
    {
        WI_SetAllFlags(translatedLsoCapabilities.Layer3Flags, ipv4Flags);
    }

    if (OffloadParameters.LsoV2IPv6 == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED)
    {
        WI_ClearAllFlags(translatedLsoCapabilities.Layer3Flags, ipv6Flags);
    }
    else
    {
        WI_SetAllFlags(translatedLsoCapabilities.Layer3Flags, ipv6Flags);
    }

    return translatedLsoCapabilities;
}

_Use_decl_annotations_
NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
NxTaskOffload::TranslateUsoCapabilities(
    NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
) const
{
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES translatedUsoCapabilities = m_activeUsoCapabilities;

    // We have overridden adapter's hardware capabilities to be all enabled.
    // So USO is supported.
    auto const ipv4Flags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
        NetClientAdapterOffloadLayer3FlagIPv4WithOptions;

    auto const ipv6Flags = NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
        NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

    if (OffloadParameters.UdpSegmentation.IPv4 == NDIS_OFFLOAD_PARAMETERS_USO_DISABLED)
    {
        WI_ClearAllFlags(translatedUsoCapabilities.Layer3Flags, ipv4Flags);
    }
    else
    {
        WI_SetAllFlags(translatedUsoCapabilities.Layer3Flags, ipv4Flags);
    }

    if (OffloadParameters.UdpSegmentation.IPv6 == NDIS_OFFLOAD_PARAMETERS_USO_DISABLED)
    {
        WI_ClearAllFlags(translatedUsoCapabilities.Layer3Flags, ipv6Flags);
    }
    else
    {
        WI_SetAllFlags(translatedUsoCapabilities.Layer3Flags, ipv6Flags);
    }

    return translatedUsoCapabilities;
}

_Use_decl_annotations_
void
NxTaskOffload::TranslateLsoCapabilities(
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const &Capabilities,
    NDIS_TCP_LARGE_SEND_OFFLOAD_V1 &NdisLsoV1Capabilities,
    NDIS_TCP_LARGE_SEND_OFFLOAD_V2 &NdisLsoV2Capabilities
) const
{
    const ULONG encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (WI_IsFlagSet(Capabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions) &&
        WI_IsFlagSet(Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        NdisLsoV1Capabilities.IPv4.Encapsulation = encapsulation;
        NdisLsoV1Capabilities.IPv4.MaxOffLoadSize = static_cast<ULONG>(Capabilities.MaximumOffloadSize);
        NdisLsoV1Capabilities.IPv4.MinSegmentCount = static_cast<ULONG>(Capabilities.MinimumSegmentCount);

        NdisLsoV1Capabilities.IPv4.IpOptions =
            WI_IsFlagSet(Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions)
            ? NDIS_OFFLOAD_SUPPORTED
            : NDIS_OFFLOAD_NOT_SUPPORTED;

        NdisLsoV1Capabilities.IPv4.TcpOptions =
            WI_IsFlagSet(Capabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions)
            ? NDIS_OFFLOAD_SUPPORTED
            : NDIS_OFFLOAD_NOT_SUPPORTED;

        NdisLsoV2Capabilities.IPv4.Encapsulation = encapsulation;
        NdisLsoV2Capabilities.IPv4.MaxOffLoadSize = static_cast<ULONG>(Capabilities.MaximumOffloadSize);
        NdisLsoV2Capabilities.IPv4.MinSegmentCount = static_cast<ULONG>(Capabilities.MinimumSegmentCount);
    }

    if (WI_IsFlagSet(Capabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions) &&
        WI_IsFlagSet(Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
    {
        NdisLsoV2Capabilities.IPv6.Encapsulation = encapsulation;
        NdisLsoV2Capabilities.IPv6.MaxOffLoadSize = static_cast<ULONG>(Capabilities.MaximumOffloadSize);
        NdisLsoV2Capabilities.IPv6.MinSegmentCount = static_cast<ULONG>(Capabilities.MinimumSegmentCount);

        NdisLsoV2Capabilities.IPv6.IpExtensionHeadersSupported =
            WI_IsFlagSet(Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions)
            ? NDIS_OFFLOAD_SUPPORTED
            : NDIS_OFFLOAD_NOT_SUPPORTED;

        NdisLsoV2Capabilities.IPv6.TcpOptionsSupported =
            WI_IsFlagSet(Capabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions)
            ? NDIS_OFFLOAD_SUPPORTED
            : NDIS_OFFLOAD_NOT_SUPPORTED;
    }
}

_Use_decl_annotations_
void
NxTaskOffload::TranslateUsoCapabilities(
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const &Capabilities,
    NDIS_UDP_SEGMENTATION_OFFLOAD &NdisUsoCapabilties
) const
{
    const ULONG encapsulation = NDIS_ENCAPSULATION_IEEE_802_3;

    if (WI_IsFlagSet(Capabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp) &&
        WI_IsFlagSet(Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        NdisUsoCapabilties.IPv4.Encapsulation = encapsulation;
        NdisUsoCapabilties.IPv4.MaxOffLoadSize = static_cast<ULONG>(Capabilities.MaximumOffloadSize);
        NdisUsoCapabilties.IPv4.MinSegmentCount = static_cast<ULONG>(Capabilities.MinimumSegmentCount);
    }

    if (WI_IsFlagSet(Capabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp) &&
        WI_IsFlagSet(Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
    {
        NdisUsoCapabilties.IPv6.Encapsulation = encapsulation;
        NdisUsoCapabilties.IPv6.MaxOffLoadSize = static_cast<ULONG>(Capabilities.MaximumOffloadSize);
        NdisUsoCapabilties.IPv6.MinSegmentCount = static_cast<ULONG>(Capabilities.MinimumSegmentCount);
        NdisUsoCapabilties.IPv6.IpExtensionHeadersSupported =
            WI_IsFlagSet(Capabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions)
            ? NDIS_OFFLOAD_SUPPORTED
            : NDIS_OFFLOAD_NOT_SUPPORTED;
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
void
NxTaskOffload::SetActiveGsoCapabilities(
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES LsoCapabilities,
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES UsoCapabilities
)
{
    // Sync active capabilities with NDIS.
    m_activeLsoCapabilities = LsoCapabilities;
    m_activeUsoCapabilities = UsoCapabilities;

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES lsoCapabilitiesToDispatch = m_hardwareGsoCapabilities;
    WI_ClearFlag(lsoCapabilitiesToDispatch.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp);

    // Use LsoCapabilities layer 3 flag if hardware supports that flag.
    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        WI_UpdateFlag(
            lsoCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4NoOptions,
            WI_IsFlagSet(LsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions));
    }

    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions))
    {
        WI_UpdateFlag(
            lsoCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4WithOptions,
            WI_IsFlagSet(LsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions));
    }

    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
    {
        WI_UpdateFlag(
            lsoCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv6NoExtensions,
            WI_IsFlagSet(LsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions));
    }

    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions))
    {
        WI_UpdateFlag(
            lsoCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv6WithExtensions,
            WI_IsFlagSet(LsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions));
    }

    // Use LsoCapabilities layer 4 flag if hardware supports that flag.
    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions))
    {
        WI_UpdateFlag(
            lsoCapabilitiesToDispatch.Layer4Flags,
            NetClientAdapterOffloadLayer4FlagTcpNoOptions,
            WI_IsFlagSet(LsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions));
    }

    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions))
    {
        WI_UpdateFlag(
            lsoCapabilitiesToDispatch.Layer4Flags,
            NetClientAdapterOffloadLayer4FlagTcpWithOptions,
            WI_IsFlagSet(LsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions));
    }

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES usoCapabilitiesToDispatch = m_hardwareGsoCapabilities;
    WI_ClearAllFlags(usoCapabilitiesToDispatch.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions|
        NetClientAdapterOffloadLayer4FlagTcpNoOptions);

    // Use UsoCapabilities layer 3 flag if hardware supports that flag.
    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        WI_UpdateFlag(
            usoCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4NoOptions,
            WI_IsFlagSet(UsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions));
    }

    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions))
    {
        WI_UpdateFlag(
            usoCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4WithOptions,
            WI_IsFlagSet(UsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions));
    }

    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
    {
        WI_UpdateFlag(
            usoCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv6NoExtensions,
            WI_IsFlagSet(UsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions));
    }

    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions))
    {
        WI_UpdateFlag(
            usoCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv6WithExtensions,
            WI_IsFlagSet(UsoCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions));
    }

    // Use UsoCapabilities layer 4 flag if hardware supports that flag.
    if (WI_IsFlagSet(m_hardwareGsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
    {
        WI_UpdateFlag(
            usoCapabilitiesToDispatch.Layer4Flags,
            NetClientAdapterOffloadLayer4FlagUdp,
            WI_IsFlagSet(UsoCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp));
    }

    m_dispatch.SetGsoActiveCapabilities(m_app.GetAdapter(),
        &lsoCapabilitiesToDispatch,
        &usoCapabilitiesToDispatch);
}

_Use_decl_annotations_
void
NxTaskOffload::SetActiveRscCapabilities(
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES RscCapabilities
)
{
    // Regardless of overridden or not, m_activeRscCapabilities need to be consistent with NDIS.
    m_activeRscCapabilities = RscCapabilities;

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscCapabilitiesToDispatch = m_hardwareRscCapabilities;

    // Use a RscCapabilities's IPvX value if hardware supports that.
    if (rscCapabilitiesToDispatch.IPv4)
    {
        rscCapabilitiesToDispatch.IPv4 = RscCapabilities.IPv4;
    }

    if (rscCapabilitiesToDispatch.IPv6)
    {
        rscCapabilitiesToDispatch.IPv6 = RscCapabilities.IPv6;
    }

    m_dispatch.SetRscActiveCapabilities(m_app.GetAdapter(), &rscCapabilitiesToDispatch);
}

_Use_decl_annotations_
void
NxTaskOffload::SetActiveTxChecksumCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES TxIPv4ChecksumCapabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES TxTcpChecksumCapabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES TxUdpChecksumCapabilities
)
{
    // Sync active capabilities with NDIS.
    m_activeTxIPv4ChecksumCapabilities = TxIPv4ChecksumCapabilities;
    m_activeTxTcpChecksumCapabilities = TxTcpChecksumCapabilities;
    m_activeTxUdpChecksumCapabilities = TxUdpChecksumCapabilities;

    auto const layer4Flags = NetClientAdapterOffloadLayer4FlagTcpNoOptions |
        NetClientAdapterOffloadLayer4FlagTcpWithOptions |
        NetClientAdapterOffloadLayer4FlagUdp;

    auto const ipv6Flags = NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
        NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txIPv4ChecksumCapabilitiesToDispatch = m_hardwareTxChecksumCapabilities;
    WI_ClearAllFlags(txIPv4ChecksumCapabilitiesToDispatch.Layer3Flags, ipv6Flags);
    WI_ClearAllFlags(txIPv4ChecksumCapabilitiesToDispatch.Layer4Flags, layer4Flags);

    // Use TxIPv4ChecksumCapabilities' flag if hardware supports that flag.
    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        WI_UpdateFlag(
            txIPv4ChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4NoOptions,
            WI_IsFlagSet(TxIPv4ChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions))
    {
        WI_UpdateFlag(
            txIPv4ChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4WithOptions,
            WI_IsFlagSet(TxIPv4ChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions));
    }

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txTcpChecksumCapabilitiesToDispatch = m_hardwareTxChecksumCapabilities;
    WI_ClearAllFlags(txTcpChecksumCapabilitiesToDispatch.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp);

    // Use TxTcpChecksumCapabilities' flag if hardware supports that flag.
        if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        WI_UpdateFlag(
            txTcpChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4NoOptions,
            WI_IsFlagSet(TxTcpChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions))
    {
        WI_UpdateFlag(
            txTcpChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4WithOptions,
            WI_IsFlagSet(TxTcpChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
    {
        WI_UpdateFlag(
            txTcpChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv6NoExtensions,
            WI_IsFlagSet(TxTcpChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions))
    {
        WI_UpdateFlag(
            txTcpChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv6WithExtensions,
            WI_IsFlagSet(TxTcpChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions))
    {
        WI_UpdateFlag(
            txTcpChecksumCapabilitiesToDispatch.Layer4Flags,
            NetClientAdapterOffloadLayer4FlagTcpNoOptions,
            WI_IsFlagSet(TxTcpChecksumCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions))
    {
        WI_UpdateFlag(
            txTcpChecksumCapabilitiesToDispatch.Layer4Flags,
            NetClientAdapterOffloadLayer4FlagTcpWithOptions,
            WI_IsFlagSet(TxTcpChecksumCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpWithOptions));
    }

    auto const tcpFlags = NetClientAdapterOffloadLayer4FlagTcpNoOptions |
        NetClientAdapterOffloadLayer4FlagTcpWithOptions;

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txUdpChecksumCapabilitiesToDispatch = m_hardwareTxChecksumCapabilities;
    WI_ClearAllFlags(txUdpChecksumCapabilitiesToDispatch.Layer4Flags, tcpFlags);

    // Use TxUdpChecksumCapabilities' flag if hardware supports that flag.
        if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        WI_UpdateFlag(
            txUdpChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4NoOptions,
            WI_IsFlagSet(TxUdpChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions))
    {
        WI_UpdateFlag(
            txUdpChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv4WithOptions,
            WI_IsFlagSet(TxUdpChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4WithOptions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions))
    {
        WI_UpdateFlag(
            txUdpChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv6NoExtensions,
            WI_IsFlagSet(TxUdpChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6NoExtensions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions))
    {
        WI_UpdateFlag(
            txUdpChecksumCapabilitiesToDispatch.Layer3Flags,
            NetClientAdapterOffloadLayer3FlagIPv6WithExtensions,
            WI_IsFlagSet(TxUdpChecksumCapabilities.Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv6WithExtensions));
    }

    if (WI_IsFlagSet(m_hardwareTxChecksumCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
    {
        WI_UpdateFlag(
            txUdpChecksumCapabilitiesToDispatch.Layer4Flags,
            NetClientAdapterOffloadLayer4FlagUdp,
            WI_IsFlagSet(TxUdpChecksumCapabilities.Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp));
    }

    m_dispatch.SetTxChecksumActiveCapabilities(m_app.GetAdapter(),
        &txIPv4ChecksumCapabilitiesToDispatch,
        &txTcpChecksumCapabilitiesToDispatch,
        &txUdpChecksumCapabilitiesToDispatch);
}

_Use_decl_annotations_
NTSTATUS
NxTaskOffload::SetActiveCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const & TxIPv4ChecksumCapabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const & TxTcpChecksumCapabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const & TxUdpChecksumCapabilities,
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const & RxChecksumCapabilities,
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const & LsoCapabilities,
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const & UsoCapabilities,
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const & RscCapabilities
)
{
    SetActiveTxChecksumCapabilities(TxIPv4ChecksumCapabilities,
        TxTcpChecksumCapabilities,
        TxUdpChecksumCapabilities);

    m_dispatch.SetRxChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &RxChecksumCapabilities);

    m_activeRxChecksumCapabilities = RxChecksumCapabilities;

    SetActiveGsoCapabilities(LsoCapabilities,
        UsoCapabilities);

    SetActiveRscCapabilities(RscCapabilities);

    // For all offloads construct the NDIS_OFFLOAD structure to send to NDIS
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
        m_activeRscCapabilities,
        ndisRscCapabilities);

    NDIS_OFFLOAD offloadCapabilties = {
        {
            NDIS_OBJECT_TYPE_OFFLOAD,
            NDIS_OFFLOAD_REVISION_6,
            NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_6
        },
        TranslateChecksumCapabilities(TxIPv4ChecksumCapabilities,
            TxTcpChecksumCapabilities,
            TxUdpChecksumCapabilities,
            RxChecksumCapabilities),
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

    return SendNdisTaskOffloadStatusIndication(offloadCapabilties, false);
}

_Use_decl_annotations_
void
NxTaskOffload::ResumeOffloadCapabilities(
    void
)
{
    // Let the client driver know that offloads have been resumed
    m_dispatch.SetTxChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeTxIPv4ChecksumCapabilities,
        &m_activeTxTcpChecksumCapabilities,
        &m_activeTxUdpChecksumCapabilities);

    m_dispatch.SetRxChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeRxChecksumCapabilities);

    m_dispatch.SetGsoActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeLsoCapabilities,
        &m_activeUsoCapabilities);

    m_dispatch.SetRscActiveCapabilities(
        m_app.GetAdapter(),
        &m_activeRscCapabilities);

    // Indicate to NDIS the change in hardware and active capabilities
    NDIS_OFFLOAD hardwareCapabilities = {};
    TranslateHardwareCapabilities(hardwareCapabilities);
    NDIS_OFFLOAD defaultCapabilities = {};
    TranslateActiveCapabilities(defaultCapabilities);

    SendNdisTaskOffloadStatusIndication(hardwareCapabilities, true);
    SendNdisTaskOffloadStatusIndication(defaultCapabilities, false);
}

_Use_decl_annotations_
void
NxTaskOffload::PauseOffloadCapabilities(
    void
)
{
    // Construct the NDIS_OFFLOAD structure to send to NDIS 
    NDIS_OFFLOAD offloadCapabilities = {
        {
            NDIS_OBJECT_TYPE_OFFLOAD,
            NDIS_OFFLOAD_REVISION_6,
            NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_6
        }
    };

    SendNdisTaskOffloadStatusIndication(offloadCapabilities, true);
    SendNdisTaskOffloadStatusIndication(offloadCapabilities, false);

    // Let the client driver know that offloads have been paused
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES txChecksumCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES),
    };

    m_dispatch.SetTxChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &txChecksumCapabilities,
        &txChecksumCapabilities,
        &txChecksumCapabilities);

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES rxChecksumCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES)
    };

    m_dispatch.SetRxChecksumActiveCapabilities(
        m_app.GetAdapter(),
        &rxChecksumCapabilities);

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES gsoCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_GSO_CAPABILITIES),
    };

    m_dispatch.SetGsoActiveCapabilities(
        m_app.GetAdapter(),
        &gsoCapabilities,
        &gsoCapabilities);

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES rscCapabilities = {
        sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES),
    };

    m_dispatch.SetRscActiveCapabilities(
        m_app.GetAdapter(),
        &rscCapabilities);
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

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
bool
NxTaskOffload::IsActiveRscIPv4Enabled(
        void
) const
{
    return m_activeRscCapabilities.IPv4;
}

_Use_decl_annotations_
bool
NxTaskOffload::IsActiveRscIPv6Enabled(
        void
) const
{
    return m_activeRscCapabilities.IPv6;
}
