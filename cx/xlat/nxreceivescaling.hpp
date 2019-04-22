// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KArray.h>

#include "NxRxXlat.hpp"

class NxTranslationApp;

class NxReceiveScaling :
    public NxNonpagedAllocation<'RxrN'>
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxReceiveScaling(
        NxTranslationApp & App,
        Rtl::KArray<wistd::unique_ptr<NxRxXlat>, NonPagedPoolNx> const & Queues,
        NET_CLIENT_ADAPTER_RECEIVE_SCALING_DISPATCH const & Dispatch
    ) noexcept;

    _IRQL_requires_(PASSIVE_LEVEL)
    size_t
    GetNumberOfQueues(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Configure(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetParameters(
        _In_ NDIS_OID_REQUEST const & Request
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    SetIndirectionEntries(
        _In_ NDIS_OID_REQUEST const & Request
    );

private:

    struct AffinitizedQueue
    {
        NxRxXlat *
            Queue = nullptr;

        size_t
            QueueId = 0;

        GROUP_AFFINITY
            Affinity = {};
    };

    struct TranslatedIndirectionEntries
    {
        NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRY
            Entries[NDIS_RSS_INDIRECTION_TABLE_MAX_SIZE_REVISION_1];

        UINT32
            Restore[NDIS_RSS_INDIRECTION_TABLE_MAX_SIZE_REVISION_1];
    };

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetEnabled(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetDisabled(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetHashSecretKey(
        void
    );

    _IRQL_requires_(DISPATCH_LEVEL)
    NTSTATUS
    SetIndirectionEntries(
        _In_ size_t NumberOfEntries,
        _In_ size_t NumberOfRetries,
        _In_ TranslatedIndirectionEntries & TranslatedEntries
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    size_t
    EnumerateProcessor(
        _In_ UINT16 Group,
        _In_ UINT8 Number
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    size_t
    EnumerateProcessor(
        _In_ PROCESSOR_NUMBER const & Processor
    ) const;

    _IRQL_requires_(DISPATCH_LEVEL)
    NxRxXlat *
    GetAffinitizedQueue(
        size_t Index
    ) const;

    _Requires_lock_held_(this->m_receiveScalingLock)
    _IRQL_requires_(DISPATCH_LEVEL)
    void
    SetAffinitizedQueue(
        size_t Index,
        NxRxXlat * Queue,
        GROUP_AFFINITY const & Affinity
    );

    _IRQL_requires_(DISPATCH_LEVEL)
    NxRxXlat *
    MapAffinitizedQueue(
        _In_ size_t Index,
        _In_ GROUP_AFFINITY const & Affinity
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    EvaluateEnable(
        NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    bool
    EvaluateDisable(
        NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    EvaluateHashSecretKey(
        NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
    );

    KSpinLock
        m_receiveScalingLock;

    NxTranslationApp &
        m_app;

    Rtl::KArray<wistd::unique_ptr<NxRxXlat>, NonPagedPoolNx> const &
        m_queues;

    NET_CLIENT_ADAPTER_RECEIVE_SCALING_DISPATCH const &
        m_dispatch;

    Rtl::KArray<AffinitizedQueue, NonPagedPoolNx>
        m_affinitizedQueues;

    Rtl::KArray<size_t, NonPagedPoolNx>
        m_indirectionTable;

    bool
        m_enabled = false;

    UINT32
        m_hashFunction = 0;

    UINT32
        m_hashType = 0;

    UINT8
        m_hashSecretKey[40] = {};

    size_t
        m_maxGroupProcessorCount = 0;

    size_t
        m_numberOfQueues = 0;

    size_t
        m_minProcessorIndex = ~0ULL;

    size_t
        m_maxProcessorIndex = ~0ULL;

    PROCESSOR_NUMBER
        m_defaultProcessor = {};

};

