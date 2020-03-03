// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

struct NX_PRIVATE_GLOBALS;

#define ADAPTER_EXTENSION_INIT_SIGNATURE 'IEAN'
struct AdapterExtensionInit
{
    ULONG InitSignature = ADAPTER_EXTENSION_INIT_SIGNATURE;

    // Private globals of the driver that registered this extension
    NX_PRIVATE_GLOBALS *PrivateGlobals = nullptr;
    PFN_NET_ADAPTER_PRE_PROCESS_NET_REQUEST NetRequestPreprocessCallback = nullptr;
};

class NxAdapterExtension
{
public:

    NxAdapterExtension(
        _In_ AdapterExtensionInit const *ExtensionConfig
    );

    void
    InvokeOidPreprocessCallback(
        _In_ NETADAPTER Adapter,
        _In_ NETREQUEST Request
    );

private:

    PFN_NET_ADAPTER_PRE_PROCESS_NET_REQUEST m_oidPreprocessCallback = nullptr;
};
