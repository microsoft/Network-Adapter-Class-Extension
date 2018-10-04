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
    Initialize(
        void
        );

private:

    NxTranslationApp &
        m_app;

    NET_CLIENT_ADAPTER_OFFLOAD_DISPATCH const &
        m_dispatch;

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
        m_activeChecksumCapabilities = {};

    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES
        m_activeLsoCapabilities = {};

    //
    // Methods to translate the offload capabilities between different 
    // NDIS and NetAdapter representations
    //

    _IRQL_requires_(PASSIVE_LEVEL)
    NDIS_TCP_IP_CHECKSUM_OFFLOAD
    TranslateChecksumCapabilities(
        _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const &Capabilities
        ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
    TranslateChecksumCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
        ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES
    TranslateLsoCapabilities(
        _In_ NDIS_OFFLOAD_PARAMETERS const &OffloadParameters
        ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    TranslateLsoCapabilities(
        _In_ NET_CLIENT_OFFLOAD_LSO_CAPABILITIES const &Capabilities,
        _Out_ NDIS_TCP_LARGE_SEND_OFFLOAD_V1 &NdisLsoV1Capabilities,
        _Out_ NDIS_TCP_LARGE_SEND_OFFLOAD_V2 &NdisLsoV2Capabilities
        ) const;

    //
    // Methods which are compiled in kernel mode only
    //

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS 
    SendNdisTaskOffloadStatusIndication(
        _In_ NDIS_OFFLOAD &offloadCapabilities
        ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetNdisMiniportOffloadAttributes(
        _In_ NDIS_OFFLOAD &hardwareCaps,
        _In_ NDIS_OFFLOAD &defaultCaps
        ) const;
};


