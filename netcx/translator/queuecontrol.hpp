// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KWorkItem.h>
#include <KLockHolder.h>

#include "NxRxXlat.hpp"
#include "NxTxXlat.hpp"

class RxScaling;

struct QueueControlSynchronize
    : public NxNonpagedAllocation<'uQoC'>
{
    _IRQL_requires_max_(DISPATCH_LEVEL)
    virtual
    void
    SignalQueueStateChange(
        void
    ) = 0;
};

class QueueControl
    : public QueueControlSynchronize
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    QueueControl(
        _In_ NxTranslationApp & App,
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
        _In_ Rtl::KArray<wistd::unique_ptr<NxTxXlat>, NonPagedPoolNx> & TxQueues,
        _In_ Rtl::KArray<wistd::unique_ptr<NxRxXlat>, NonPagedPoolNx> & RxQueues
    ) noexcept;

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    SynchronizeDatapathStart(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    SynchronizeDatapathStop(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    CreateQueues(
        _In_ size_t TxQueueCount,
        _In_ size_t RxQueueCount
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    ProvisionOnDemandQueues(
        _In_ size_t TxQueueCount
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    CreateRxScalingQueues(
        wistd::unique_ptr<RxScaling> & RxScaling
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    StartQueues(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    CancelQueues(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    StopQueues(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    DestroyQueues(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SignalQueueStateChange(
        void
    ) override;

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    DestroyOnDemandQueue(
        size_t Index
    );

private:

    enum class DatapathState : LONG {
        Started,
        Stopping,
        Stopped,
    };

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetDatapathState(
        DatapathState
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    SynchronizeDatapathWork(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    bool
    SynchronizeDatapathState(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    bool
    IsDatapathStateSynchronized(
        void
    );

    NxTranslationApp &
        m_app;

    NET_CLIENT_DISPATCH const *
        m_dispatch;

    NET_CLIENT_ADAPTER
        m_adapter;

    NET_CLIENT_ADAPTER_DISPATCH const *
        m_adapterDispatch;

    _Interlocked_
    volatile
    DatapathState
        m_state = DatapathState::Stopped;

    KPushLock
        m_workSyncLock;

    KCoalescingWorkItem<QueueControl>
        m_work;

    KSpinLock
        m_workQueueLock;

    KWaitEvent
        m_event;

    Rtl::KArray<wistd::unique_ptr<NxTxXlat>, NonPagedPoolNx> &
        m_txQueues;

    Rtl::KArray<wistd::unique_ptr<NxRxXlat>, NonPagedPoolNx> &
        m_rxQueues;

};

