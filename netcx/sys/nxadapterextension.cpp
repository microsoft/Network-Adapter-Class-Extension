// Copyright (C) Microsoft Corporation. All rights reserved.
#include "Nx.hpp"

#include "NxAdapterExtension.tmh"
#include "NxAdapterExtension.hpp"

_Use_decl_annotations_
NxAdapterExtension::NxAdapterExtension(
    AdapterExtensionInit const *ExtensionConfig
) :
    m_oidPreprocessCallback(ExtensionConfig->NetRequestPreprocessCallback)
{
}

_Use_decl_annotations_
VOID
NxAdapterExtension::InvokeOidPreprocessCallback(
    NETADAPTER Adapter,
    NETREQUEST Request
)
{
    if (m_oidPreprocessCallback)
    {
        m_oidPreprocessCallback(Adapter, Request);
    }
}
