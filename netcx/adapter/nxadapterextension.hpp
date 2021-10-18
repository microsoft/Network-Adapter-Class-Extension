// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

struct NX_PRIVATE_GLOBALS;

static const ULONG OidSignature = 'SOid';
static const ULONG DirectOidSignature = 'DOid';

struct DispatchContext
{
    size_t NextHandlerIndex = 0;
    ULONG Signature;
};

static_assert(sizeof(DispatchContext) <= sizeof(NDIS_OID_REQUEST::MiniportReserved));

enum class MediaExtensionType
{
    None = 0,
    Mbb,
    Wifi,
};

#define ADAPTER_EXTENSION_INIT_SIGNATURE 'IEAN'
struct AdapterExtensionInit
{
    ULONG
        InitSignature = ADAPTER_EXTENSION_INIT_SIGNATURE;

    // Private globals of the driver that registered this extension
    NX_PRIVATE_GLOBALS *
        PrivateGlobals = nullptr;

    PFN_NET_ADAPTER_PRE_PROCESS_OID_REQUEST
        OidRequestPreprocessCallback = nullptr;

    PFN_NET_ADAPTER_PRE_PROCESS_DIRECT_OID_REQUEST
        DirectOidRequestPreprocessCallback = nullptr;

    PFN_NETEX_ADAPTER_PREPROCESS_DIRECT_OID
        DirectOidPreprocessCallbackEx = nullptr;

    PFN_NET_ADAPTER_TX_PEER_DEMUX
        WifiTxPeerDemux = nullptr;

    NET_ADAPTER_NDIS_PM_CAPABILITIES
        NdisPmCapabilities = {};

    NET_ADAPTER_EXTENSION_POWER_POLICY_CALLBACKS
        PowerPolicyCallbacks = {};
};

MediaExtensionType
MediaExtensionTypeFromClientGlobals(
    _In_ WDF_DRIVER_GLOBALS * const ClientDriverGlobals
);

class NxAdapterExtension
{
public:

    NxAdapterExtension(
        _In_ AdapterExtensionInit const * ExtensionConfig
    );

    NX_PRIVATE_GLOBALS const *
    GetPrivateGlobals(
        void
    ) const;

    void
    InvokeOidPreprocessCallback(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_OID_REQUEST * Request,
        _In_ DispatchContext * Context
    ) const;

    NTSTATUS
    InvokeDirectOidPreprocessCallback(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_OID_REQUEST * Request,
        _In_ DispatchContext * Context
    ) const;

    void
    SetNdisPmCapabilities(
        _In_ NET_ADAPTER_NDIS_PM_CAPABILITIES const * Capabilities
    );

    NET_ADAPTER_NDIS_PM_CAPABILITIES const &
    GetNdisPmCapabilities(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UpdateNdisPmParameters(
        _In_ NETADAPTER Adapter,
        _Inout_ NDIS_PM_PARAMETERS * PmParameters
    ) const;

private:

    NX_PRIVATE_GLOBALS * const
        m_extensionGlobals;

    PFN_NET_ADAPTER_PRE_PROCESS_OID_REQUEST
        m_oidPreprocessCallback = nullptr;

    PFN_NET_ADAPTER_PRE_PROCESS_DIRECT_OID_REQUEST
        m_directOidPreprocessCallback = nullptr;

    PFN_NETEX_ADAPTER_PREPROCESS_DIRECT_OID
        m_directOidPreprocessCallbackEx = nullptr;

    NET_ADAPTER_NDIS_PM_CAPABILITIES
        m_ndisPmCapabilities = {};

    NET_ADAPTER_EXTENSION_POWER_POLICY_CALLBACKS
        m_powerPolicyCallbacks = {};
};

