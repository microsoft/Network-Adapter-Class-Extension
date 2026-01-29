// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "ExecutionContext.hpp"

class UserModeExecutionContext
{
public:

    ~UserModeExecutionContext(
        void
    );

    NTSTATUS
    Initialize(
        void
    );

    HANDLE
    GetHandle(
        void
    ) const;

    void
    Notify(
        void
    );

    void
    RegisterPoll(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll
    );

    void
    UnregisterPoll(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll
    );

    void
    RegisterNotification(
        _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
    );

    void
    UnregisterNotification(
        _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
    );

    void
    QueueTask(
        _In_ EXECUTION_CONTEXT_TASK * Task
    );

private:

    void
    WorkerThread(
        void
    );

    void
    SendCanaryIoctl(
        void
    );

private:

    wil::unique_event_nothrow
        m_exit;

    wil::unique_event_nothrow
        m_poll;

    wil::unique_event_nothrow
        m_userThreadMonitorEvent;

    OVERLAPPED
        m_userThreadMonitorOverlapped{};

    wil::unique_handle
        m_workerThread;

    wil::unique_hfile
        m_handle;

    EXECUTION_CONTEXT_USER_ACTION
        m_userModeAction{};

    EXECUTION_CONTEXT_KERNEL_ACTION
        m_kernelModeAction{};
};
