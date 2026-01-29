// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <NxNew.hpp>
#include <NetClientApi.h>

/// Per-adapter context implemented by an app
struct INxApp : public NxNonpagedAllocation<'pAxN'>
{
    /// Called by netadapter to stop & destroy an app
    _IRQL_requires_(PASSIVE_LEVEL)
    virtual ~INxApp() = default;

    _IRQL_requires_(PASSIVE_LEVEL)
    virtual
    NTSTATUS
    TxPeerDemuxTableInitialize(
        _In_ size_t MaxPeers
    ) = 0;

    _IRQL_requires_(PASSIVE_LEVEL)
    virtual
    void
    TxPeerDemuxTableAddPeer(
        _In_ NET_CLIENT_EUI48_ADDRESS const* Address
    ) = 0;

    _IRQL_requires_(PASSIVE_LEVEL)
    virtual
    void
    TxPeerDemuxTableRemovePeer(
        _In_ NET_CLIENT_EUI48_ADDRESS const* Address
    ) = 0;

    _IRQL_requires_(PASSIVE_LEVEL)
    virtual
    void
    SetUseWiFiCxDemuxCallback(
        _In_ bool UseDemuxCallback
    ) = 0;
};

/// Global singleton implemented by an app to create instances of the datapath
struct INxAppFactory
{
    virtual
    ~INxAppFactory() = default;

    /// Called by netadapter to start an app
    _IRQL_requires_(PASSIVE_LEVEL)
    virtual
    NTSTATUS
    CreateApp(
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
        _Out_ void ** ClientContext,
        _Out_ NET_CLIENT_CONTROL_DISPATCH const ** ClientDispatch
        ) = 0;
};

