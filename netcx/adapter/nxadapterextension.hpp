// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxRequest.hpp"

struct NX_PRIVATE_GLOBALS;

enum class MediaExtensionType
{
    None = 0,
    Mbb,
    Wifi,
};

#define ADAPTER_EXTENSION_INIT_SIGNATURE 'IEAN'
struct AdapterExtensionInit
{
    ULONG InitSignature = ADAPTER_EXTENSION_INIT_SIGNATURE;

    // Private globals of the driver that registered this extension
    NX_PRIVATE_GLOBALS *PrivateGlobals = nullptr;

    PFN_NET_ADAPTER_PRE_PROCESS_OID_REQUEST OidRequestPreprocessCallback = nullptr;

    NET_ADAPTER_NDIS_PM_CAPABILITIES NdisPmCapabilities = {};
};

MediaExtensionType
MediaExtensionTypeFromClientGlobals(
    _In_ WDF_DRIVER_GLOBALS * const ClientDriverGlobals
);

class NxAdapterExtension
{
public:

    NxAdapterExtension(
        _In_ AdapterExtensionInit const *ExtensionConfig
    );

    void
    InvokeOidPreprocessCallback(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_OID_REQUEST * Request,
        _In_ DispatchContext * Context
    ) const;

    NET_ADAPTER_NDIS_PM_CAPABILITIES const &
    GetNdisPmCapabilities(
        void
    ) const;

private:

    MediaExtensionType
        m_extensionType = MediaExtensionType::None;

    PFN_NET_ADAPTER_PRE_PROCESS_OID_REQUEST m_oidPreprocessCallback = nullptr;

    NET_ADAPTER_NDIS_PM_CAPABILITIES
        m_ndisPmCapabilities = {};

};
