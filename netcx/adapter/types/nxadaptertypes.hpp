// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxPrivateGlobals.hpp"

class NxAdapterLinkLayerCapabilities
{
public:

    // Fields from version 2.0
    ULONG
        Size = {};

    ULONG64
        MaxTxLinkSpeed = {};

    ULONG64
        MaxRxLinkSpeed = {};

    NxAdapterLinkLayerCapabilities(
        void
    ) = default;

    NxAdapterLinkLayerCapabilities(
        _In_ NX_PRIVATE_GLOBALS const & ClientGlobals,
        _In_ NET_ADAPTER_LINK_LAYER_CAPABILITIES const & Capabilities
    );

    explicit
    operator bool(
        void
    ) const;

    void
    Assign(
        _In_ NX_PRIVATE_GLOBALS const & ClientGlobals,
        _In_ NET_ADAPTER_LINK_LAYER_CAPABILITIES const & Capabilities
    );

};

class NxAdapterReceiveFilterCapabilities
{
public:

    // Fields from version 2.1
    ULONG
        Size = {};

    NET_PACKET_FILTER_FLAGS
        SupportedPacketFilters = {};

    SIZE_T
        MaximumMulticastAddresses = {};

    PFN_NET_ADAPTER_SET_RECEIVE_FILTER
        EvtSetReceiveFilter = {};

    NxAdapterReceiveFilterCapabilities(
        void
    ) = default;

    NxAdapterReceiveFilterCapabilities(
        _In_ NX_PRIVATE_GLOBALS const & ClientGlobals,
        _In_ NET_ADAPTER_RECEIVE_FILTER_CAPABILITIES const & Capabilities
    );

    explicit
    operator bool(
        void
    ) const;

    void
    Assign(
        _In_ NX_PRIVATE_GLOBALS const & ClientGlobals,
        _In_ NET_ADAPTER_RECEIVE_FILTER_CAPABILITIES const & Capabilities
    );

};

