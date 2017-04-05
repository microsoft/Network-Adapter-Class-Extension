/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxTranslationApp.hpp

Abstract:

    Implements the driver-global state for the NBL-to-NET_PACKET translation
    app in NetAdapterCx.

--*/

#pragma once

#include "NxApp.hpp"
#include "NxTxXlat.hpp"
#include "NxRxXlat.hpp"

class NxTranslationApp : public INxApp
{
    wistd::unique_ptr<NxTxXlat> m_txQueue;
    wistd::unique_ptr<NxRxXlat> m_rxQueue;

    PAGED NTSTATUS StartTx(_In_ INxAdapter * adapter);
    PAGED NTSTATUS StartRx(_In_ INxAdapter * adapter);

public:

    //
    // INxApp
    //
    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS Start(_In_ INxAdapter * adapter) override;
};
