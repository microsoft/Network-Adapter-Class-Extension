// Copyright (C) Microsoft Corporation. All rights reserved.
#include "Nx.hpp"

#include "NxAdapterExtension.hpp"
#include "NxAdapter.hpp"
#include "version.hpp"

#include "NxAdapterExtension.tmh"

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
    , m_directOidPreprocessCallback(ExtensionConfig->DirectOidRequestPreprocessCallback)
    , m_directOidPreprocessCallbackEx(ExtensionConfig->DirectOidPreprocessCallbackEx)
    , m_extensionGlobals(ExtensionConfig->PrivateGlobals)
{
    // Either *one* callback can be set, or none
    NT_FRE_ASSERT(!(ExtensionConfig->DirectOidRequestPreprocessCallback && ExtensionConfig->DirectOidPreprocessCallbackEx));

    if (ExtensionConfig->NdisPmCapabilities.Size > 0)
    {
        RtlCopyMemory(
            &m_ndisPmCapabilities,
            &ExtensionConfig->NdisPmCapabilities,
            ExtensionConfig->NdisPmCapabilities.Size);
    }

    if (ExtensionConfig->PowerPolicyCallbacks.Size > 0)
    {
        RtlCopyMemory(
            &m_powerPolicyCallbacks,
            &ExtensionConfig->PowerPolicyCallbacks,
            ExtensionConfig->PowerPolicyCallbacks.Size);
    }
}

NX_PRIVATE_GLOBALS const *
NxAdapterExtension::GetPrivateGlobals(
    void
) const
{
    return m_extensionGlobals;
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

_Use_decl_annotations_
NTSTATUS
NxAdapterExtension::InvokeDirectOidPreprocessCallback(
    NETADAPTER Adapter,
    NDIS_OID_REQUEST * Request,
    DispatchContext * Context
) const
{
    if (m_directOidPreprocessCallbackEx)
    {
        auto dispatchContext = static_cast<WDFCONTEXT>(Context);
        return m_directOidPreprocessCallbackEx(Adapter, Request, dispatchContext);
    }
    else if (m_directOidPreprocessCallback)
    {
        auto dispatchContext = static_cast<WDFCONTEXT>(Context);
        m_directOidPreprocessCallback(Adapter, Request, dispatchContext);

        return STATUS_PENDING;
    }

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    return nxAdapter->DispatchDirectOidRequest(Request, Context);
}

void
NxAdapterExtension::SetNdisPmCapabilities(
    NET_ADAPTER_NDIS_PM_CAPABILITIES const * Capabilities
)
{
    RtlCopyMemory(
        &m_ndisPmCapabilities,
        Capabilities,
        Capabilities->Size);
}

NET_ADAPTER_NDIS_PM_CAPABILITIES const &
NxAdapterExtension::GetNdisPmCapabilities(
    void
) const
{
    return m_ndisPmCapabilities;
}

_Use_decl_annotations_
void
NxAdapterExtension::UpdateNdisPmParameters(
    NETADAPTER Adapter,
    NDIS_PM_PARAMETERS * PmParameters
) const
{
    if (m_powerPolicyCallbacks.EvtUpdateNdisPmParameters != nullptr)
    {
        m_powerPolicyCallbacks.EvtUpdateNdisPmParameters(Adapter, PmParameters);
    }
}
