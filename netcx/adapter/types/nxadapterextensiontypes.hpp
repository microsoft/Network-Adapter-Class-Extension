// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxPrivateGlobals.hpp"

class NxAdapterNdisPmCapabilities
{
public:

    // Fields from version 2.0
    ULONG
        Size = {};

    ULONG
        MediaSpecificWakeUpEvents = {};

    // Fields from version 2.1
    ULONG
        SupportedProtocolOffloads = {};

    NxAdapterNdisPmCapabilities(
        void
    ) = default;

    NxAdapterNdisPmCapabilities(
        _In_ NX_PRIVATE_GLOBALS const & ClientGlobals,
        _In_ NET_ADAPTER_NDIS_PM_CAPABILITIES const & Capabilities
    );

    explicit
    operator bool(
        void
    ) const;

    void
    Assign(
        _In_ NX_PRIVATE_GLOBALS const & ClientGlobals,
        _In_ NET_ADAPTER_NDIS_PM_CAPABILITIES const & Capabilities
    );

};

