// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxTranslationAppFactory is the main entrypoint for the
    NBL-to-NET_PACKET translation app.

--*/

#pragma once

#include <NxApp.hpp>

class NxTranslationAppFactory : public INxAppFactory
{
public:

    _IRQL_requires_(PASSIVE_LEVEL)
    virtual
    ~NxTranslationAppFactory(
        void
        );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
        );

    //
    // INxAppFactory
    //

    _IRQL_requires_(PASSIVE_LEVEL)
    virtual
    NTSTATUS
    CreateApp(
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
        _Out_ void ** ClientContext,
        _Out_ NET_CLIENT_CONTROL_DISPATCH const ** ClientDispatch
        ) override;
};

