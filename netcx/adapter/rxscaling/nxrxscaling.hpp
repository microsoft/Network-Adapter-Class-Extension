// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

struct _NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES;
typedef _NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES;

class NxRxScaling
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxRxScaling(
        _In_ NETADAPTER AdapterHandle
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetCapabilities(
        _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const & Capabilities
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    GetCapabilities(
        _Out_ NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES * Capabilities
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Enable(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Disable(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetHashSecretKey(
        _In_ NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const * HashSecretKey
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    SetIndirectionEntries(
        _Inout_ NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES * Entries
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetHashInfo(
        _In_ NET_CLIENT_RECEIVE_SCALING_HASH_INFO const * HashInfo
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CheckHashInfoCapabilities(
        _In_ NET_CLIENT_RECEIVE_SCALING_HASH_INFO const * HashInfo
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    bool
    IsSupported(
        void
    );

    bool
        RssKeywordEnabled = false;

    NDIS_RECEIVE_SCALE_CAPABILITIES *
        RecvScaleCapabilities = nullptr;

    ULONG
        DefaultNumberOfQueues = 0;

private:

    NETADAPTER
        m_adapter = WDF_NO_HANDLE;

    NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES
        m_capabilities = {};

    NDIS_RECEIVE_SCALE_CAPABILITIES
        m_recvScaleCapabilities = {};

    NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE
        m_hashType = NetAdapterReceiveScalingHashTypeNone;

    NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE
        m_protocolType = NetAdapterReceiveScalingProtocolTypeNone;

    bool
        m_enabled = false;
};
