// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KArray.h>

#include "NxRxXlat.hpp"

class NxTranslationApp;

class RxScaling
    : public NxNonpagedAllocation<'RxrN'>
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    RxScaling(
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
        _Inout_ NDIS_OID_REQUEST & Request
    );

private:

    struct AffinitizedQueue
    {
        NxRxXlat *
            Queue = nullptr;

        size_t
            QueueId = 0;

        PROCESSOR_NUMBER
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

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetHashInfo(
        _In_ ULONG HashFunction,
        _In_ ULONG HashType
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    static
    void
    TranslateHashInfo(
        _In_ ULONG HashFunction,
        _In_ ULONG HashType,
        _Out_ NET_CLIENT_ADAPTER_RECEIVE_SCALING_HASH_TYPE * const NetClientHashType,
        _Out_ NET_CLIENT_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE * const NetClientProtocolType
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
        PROCESSOR_NUMBER const & Affinity
    );

    _IRQL_requires_(DISPATCH_LEVEL)
    NxRxXlat *
    MapAffinitizedQueue(
        _In_ size_t Index,
        _In_ PROCESSOR_NUMBER const & Affinity
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
        NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters,
        const size_t BufferSize
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    EvaluateHashInfo(
        NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
    );

    bool
    IsContained(
        _In_ size_t const BufferSize,
        _In_ size_t const ElementOffset,
        _In_ size_t const ElementSize
    );

    bool
    IsContained(
        _In_ size_t const BufferSize,
        _In_ size_t const ElementOffset,
        _In_ size_t const ElementSize,
        _In_ size_t const ElementCount
    );

    template<typename T>
    T const *
    TryGetArray(
        _In_ void const * Buffer,
        _In_ size_t const BufferSize,
        _In_ size_t const ElementOffset,
        _In_ size_t const ElementCount
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    EvaluateIndirectionTableEntries(
        NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
    );

    _IRQL_requires_(DISPATCH_LEVEL)
    bool
    UpdateIndirectionTableEntryStatus(
        _In_ size_t const NumberOfEntries,
        _In_ TranslatedIndirectionEntries const & TranslatedEntries,
        _Out_ NDIS_RSS_SET_INDIRECTION_ENTRY * Entries
    );

    bool
        m_configured = false;

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
        m_minProcessorIndex = SIZE_T_MAX;

    size_t
        m_maxProcessorIndex = SIZE_T_MAX;

    PROCESSOR_NUMBER
        m_defaultProcessor = {};

    size_t
        m_numberIndirectionTableEntries = 0;
};

