// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KArray.h>
#include <NetClientTypes.h>

#include "NxTxXlat.hpp"
#include "TxPeerAddressDemux.hpp"
#include "TxUserPriorityDemux.hpp"
#include "TxWmmInfoDemux.hpp"

class NxTranslationApp;

class TxScaling
    : public INxNblTx
    , public NxNonpagedAllocation<'TxtN'>
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    TxScaling(
        NxTranslationApp & App,
        INxNblDispatcher * NblDispatcher,
        Rtl::KArray<wistd::unique_ptr<NxTxXlat>, NonPagedPoolNx> const & Queues
    ) noexcept;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION const & Configuration
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    size_t
    GetNumberOfQueues(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    GenerateDemuxProperties(
        _In_ NET_BUFFER_LIST const * NetBufferList,
        _In_ Rtl::KArray<NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY> & Properties
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SendNetBufferLists(
        _In_ NET_BUFFER_LIST * Nbl,
        _In_ ULONG PortNumber,
        _In_ ULONG NblCount,
        _In_ ULONG SendFlags
    ) override;

private:

    NxTranslationApp &
        m_app;

    INxNblDispatcher *
        m_nblDispatcher = nullptr;

    size_t
        m_numberOfQueues = 1U;

    Rtl::KArray<wistd::unique_ptr<NxTxXlat>, NonPagedPoolNx> const &
        m_queues;

    Rtl::KArray<wistd::unique_ptr<NxTxDemux>, NonPagedPoolNx>
        m_demux;
};

