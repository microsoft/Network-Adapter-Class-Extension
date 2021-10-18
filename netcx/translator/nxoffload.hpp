// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KArray.h>

class NxTranslationApp;

class NxTaskOffload :
    public NxNonpagedAllocation<'RxrN'>
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxTaskOffload(
        _In_ NxTranslationApp & App,
        _In_ NET_CLIENT_ADAPTER_OFFLOAD_DISPATCH const & Dispatch
    ) noexcept;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetActiveCapabilities(
        _In_ NDIS_OID_REQUEST const & Request
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetEncapsulation(
        _In_ NDIS_OID_REQUEST const & Request
        );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsActiveRscIPv4Enabled(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsActiveRscIPv6Enabled(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    PauseOffloadCapabilities(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    ResumeOffloadCapabilities(
        void
    );

private:

    NxTranslationApp &
        m_app;

    NET_CLIENT_ADAPTER_OFFLOAD_DISPATCH const &
        m_dispatch;

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
        m_hardwareRxChecksumCapabilities = {};

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
        m_activeRxChecksumCapabilities = {};

    // We universally override hardware's TCPIP task offload capabilities, but still obey registry keywords.
    // Overriding operations include:
    // 1. Override hardware offload capabilities to be all enabled, then indicate to NDIS.
    //    We cache hardware's true hardware offload capabilities as m_hardwareXxxCapabilities.
    // 2. Intersect registry keywords with the overridden hardware capabilities,
    //    then indicate to NDIS as default capabilities, stored as m_activeXxxCapabilities.
    // 3. When NDIS sets activeXxxCapabilities, we dispatch modified activeXxxCapabilities to adapter
    //    based on the unoverridden m_hardwareXxxCapabilities:
    //    If m_hardwareXxxCapabilities.flag is set, we dispatch activeXxxCapabilities.flag
    //    If m_hardwareXxxCapabilities.flag is NOT set, we dispatch m_hardwareXxxCapabilities.flag
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        m_hardwareTxChecksumCapabilities = {};

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
        m_hardwareGsoCapabilities = {};

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES
        m_hardwareRscCapabilities = {};

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        m_activeTxIPv4ChecksumCapabilities = {};

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        m_activeTxTcpChecksumCapabilities = {};

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        m_activeTxUdpChecksumCapabilities = {};

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
        m_activeLsoCapabilities = {};

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
        m_activeUsoCapabilities = {};

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES
        m_activeRscCapabilities = {};

    // m_softwareXxxCapabilities is the superset of m_hardwareXxxCapabilities and the software
    // fallback capabilities for the offload. it's also called the hardware override capabilities.
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        m_softwareTxChecksumCapabilities = {};

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
        m_softwareGsoCapabilities = {};

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES
        m_softwareRscCapabilities = {};

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    InitializeTxChecksum(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    InitializeRxChecksum(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    InitializeGso(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    InitializeRsc(
        void
    );

    //
    // Methods to translate the offload capabilities between different
    // NDIS and NetAdapter representations
    //

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    TranslateHardwareCapabilities(
        _Out_ NDIS_OFFLOAD & HardwareCapabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    TranslateActiveCapabilities(
        _Out_ NDIS_OFFLOAD & ActiveCapabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NDIS_TCP_IP_CHECKSUM_OFFLOAD
    TranslateChecksumCapabilities(
        _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const &Capabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NDIS_TCP_IP_CHECKSUM_OFFLOAD
    TranslateChecksumCapabilities(
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const &TxIPv4Capabilities,
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const &TxTcpCapabilities,
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const &TxUdpCapabilities,
        _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const &RxCapabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
    TranslateChecksumCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    TranslateChecksumCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters,
        _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * TxIPv4Capabilities,
        _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * TxTcpCapabilities,
        _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * TxUdpCapabilities,
        _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * RxCapabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
    TranslateLsoCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    TranslateLsoCapabilities(
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const &Capabilities,
        _Out_ NDIS_TCP_LARGE_SEND_OFFLOAD_V1 &NdisLsoV1Capabilities,
        _Out_ NDIS_TCP_LARGE_SEND_OFFLOAD_V2 &NdisLsoV2Capabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
    TranslateUsoCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    TranslateUsoCapabilities(
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const &Capabilities,
        _Out_ NDIS_UDP_SEGMENTATION_OFFLOAD &NdisUsoCapabilties
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES
    TranslateRscCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    TranslateRscCapabilities(
        _In_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const &Capabilities,
        _Out_ NDIS_TCP_RECV_SEG_COALESCE_OFFLOAD &NdisRscCapabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CheckActiveCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CheckActiveTxChecksumCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CheckActiveRxChecksumCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CheckActiveGsoCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CheckActiveRscCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    //
    // Methods which are compiled in kernel mode only
    //

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SendNdisTaskOffloadStatusIndication(
        _In_ NDIS_OFFLOAD & OffloadCapabilities,
        _In_ bool IsHardwareConfig
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetNdisMiniportOffloadAttributes(
        _In_ NDIS_OFFLOAD &hardwareCaps,
        _In_ NDIS_OFFLOAD &defaultCaps
        ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetActiveCapabilities(
        _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const & ChecksumCapabilities,
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const & LsoCapabilities,
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const & UsoCapabilities,
        _In_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const & RscCapabilities
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetActiveCapabilities(
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const & TxIpv4ChecksumCapabilities,
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const & TxTcpChecksumCapabilities,
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const & TxUdpChecksumCapabilities,
        _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const & RxChecksumCapabilities,
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const & LsoCapabilities,
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const & UsoCapabilities,
        _In_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const & RscCapabilities
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetActiveRscCapabilities(
        _In_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES RscCapabilities
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetActiveGsoCapabilities(
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES LsoCapabilities,
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES UsoCapabilities
    );

// The following functions are used when the checksum capability released in Netcx 2.0
// is advertised. They are implemented in NxOffloadV1.cpp

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetActiveTxChecksumCapabilities(
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES TxIpv4ChecksumCapabilities,
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES TxTcpChecksumCapabilities,
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES TxUdpChecksumCapabilities
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    bool
    IsChecksumOffloadV1Used(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    InitializeChecksumV1(
        void
    );

private:

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CheckActiveChecksumV1Capabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
    ) const;

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
        m_activeChecksumCapabilities = {};

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
        m_hardwareChecksumCapabilities = {};


//  End of NxOffloadV1 functions

};


