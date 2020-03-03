// Copyright (C) Microsoft Corporation. All rights reserved.
#include "Nx.hpp"

#include "NxAdapterExtension.tmh"
#include "NxAdapterExtension.hpp"
#include "NxAdapter.hpp"
#include "version.hpp"

_Use_decl_annotations_
MediaExtensionType
MediaExtensionTypeFromClientGlobals(
    WDF_DRIVER_GLOBALS * const ClientDriverGlobals
)
{
    auto const & driverName = ClientDriverGlobals->DriverName;
    auto const maxClientNameCch = ARRAYSIZE(driverName);

    if (_strnicmp(&driverName[0], "mbbcx", maxClientNameCch) == 0)
    {
        return MediaExtensionType::Mbb;
    }
    else if(_strnicmp(&driverName[0], "wificx", maxClientNameCch) == 0)
    {
        return MediaExtensionType::Wifi;
    }

    return MediaExtensionType::None;
}

_Use_decl_annotations_
NxAdapterExtension::NxAdapterExtension(
    AdapterExtensionInit const *ExtensionConfig
)
    : m_oidPreprocessCallback(ExtensionConfig->OidRequestPreprocessCallback)
    , m_extensionType(ExtensionConfig->PrivateGlobals->ExtensionType)
{
    if (ExtensionConfig->NdisPmCapabilities.Size > 0)
    {
        RtlCopyMemory(
            &m_ndisPmCapabilities,
            &ExtensionConfig->NdisPmCapabilities,
            ExtensionConfig->NdisPmCapabilities.Size);
    }
}

_Use_decl_annotations_
void
NxAdapterExtension::InvokeOidPreprocessCallback(
    NETADAPTER Adapter,
    NDIS_OID_REQUEST * Request,
    DispatchContext * Context
) const
{
    if (m_oidPreprocessCallback)
    {
        auto dispatchContext = static_cast<WDFCONTEXT>(Context);
        m_oidPreprocessCallback(Adapter, Request, dispatchContext);
    }
    else
    {
        auto nxAdapter = GetNxAdapterFromHandle(Adapter);
        nxAdapter->DispatchOidRequest(Request, Context);
    }
}

NET_ADAPTER_NDIS_PM_CAPABILITIES const &
NxAdapterExtension::GetNdisPmCapabilities(
    void
) const
{
    return m_ndisPmCapabilities;
}
