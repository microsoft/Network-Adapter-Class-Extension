// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements the driver-global state for the NBL-to-NET_PACKET translation
    app in NetAdapterCx.

--*/

#pragma once

#include "NxApp.hpp"
#include "NxTxXlat.hpp"
#include "NxRxXlat.hpp"

struct ReceiveScaleMapping
{

    PROCESSOR_NUMBER
        Number;

    Rtl::KArray<UINT16>
        HashIndexes;

    size_t
        QueueIndex = ~0U;

    ULONG
        QueueId = ~0U;

    bool
        QueueCreated = false;

};

class NxTranslationApp :
    public INxApp
{

public:

    NxTranslationApp(
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
        ) noexcept;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CreateDatapath(
        void
        );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    DestroyDatapath(
        void
        );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReceiveScalingParametersSet(
        _In_ NDIS_OID_REQUEST & Request
        );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReceiveScalingIndirectionTableEntriesSet(
        _In_ NDIS_OID_REQUEST & Request
        );

private:

    PAGED
    NTSTATUS
    StartTx(
        void
        );

    PAGED
    NTSTATUS
    StartRx(
        void
        );

    bool
    ReceiveScalingEvaluateDisable(
        _In_ NDIS_RECEIVE_SCALE_PARAMETERS const & Parameters
        );

    NTSTATUS
    ReceiveScalingEvaluateEnable(
        _In_ NDIS_RECEIVE_SCALE_PARAMETERS const & Parameters
        );

    NTSTATUS
    NxTranslationApp::ReceiveScalingEvaluateIndirectionEntries(
        _In_ PROCESSOR_NUMBER const * ProcessorNumbers,
        _In_ size_t Count
        );

    KSpinLock
        m_translatorLock;

    Rtl::KArray<ReceiveScaleMapping>
        m_receiveScaleMappings;

    wistd::unique_ptr<NxTxXlat>
        m_txQueue;

    Rtl::KArray<wistd::unique_ptr<NxRxXlat>>
        m_rxQueues;

    NET_CLIENT_DISPATCH const *
        m_dispatch = nullptr;

    NET_CLIENT_ADAPTER
        m_adapter;

    NET_CLIENT_ADAPTER_DISPATCH const *
        m_adapterDispatch = nullptr;

    INxNblDispatcher *
        m_NblDispatcher = nullptr;

    NxNblRx
        m_rxBufferReturn;

};

