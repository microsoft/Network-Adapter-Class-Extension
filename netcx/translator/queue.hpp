// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <ExecutionContextDispatch.h>

#include <KWaitEvent.h>

class NxTranslationApp;

class Queue
    : public NxNonpagedAllocation<'uQrT'>
{

public:

    static
    EVT_EXECUTION_CONTEXT_TASK
        EcTaskCancel;

    static
    EVT_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED
        EvtSetNblNotification;

    _IRQL_requires_(PASSIVE_LEVEL)
    Queue(
        NxTranslationApp & App,
        size_t QueueId,
        PFN_EXECUTION_CONTEXT_POLL EvtPollQueueStarted,
        PFN_EXECUTION_CONTEXT_POLL EvtPollQueueStopping
    ) noexcept;

    virtual
    ~Queue(
        void
    ) = default;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    size_t
    GetQueueId(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Start(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsStarted(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Cancel(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    bool
    IsStopping(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Stop(
        void
    );

protected:

    NxTranslationApp &
        m_app;

    NET_CLIENT_QUEUE
        m_queue = nullptr;

    NET_CLIENT_QUEUE_DISPATCH const *
        m_queueDispatch = nullptr;

    //
    // Executing Tx/Rx datapath logic as polling work in EC.
    //

    NET_EXECUTION_CONTEXT
        m_executionContext = nullptr;

    NET_EXECUTION_CONTEXT_DISPATCH const *
        m_executionContextDispatch = nullptr;

    EXECUTION_CONTEXT_POLL
        m_executionContextPoll;

    EXECUTION_CONTEXT_NOTIFICATION
        m_nblNotification;

    _Interlocked_ volatile
    LONG
        m_nblNotificationEnabled = 0;

    PFN_EXECUTION_CONTEXT_POLL const
        m_pollQueueStarted;

    PFN_EXECUTION_CONTEXT_POLL const
        m_pollQueueStopping;

    //
    // To signal the completion of client queue Stop()
    //
    KWaitEvent
        m_readyToStop;

private:

    bool
        m_started = false;

    bool
        m_stopping = false;

    size_t const
        m_queueId = SIZE_T_MAX;
};

