// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <ExecutionContext.h>
#include <ExecutionContextPoll.h>
#include <ExecutionContextNotification.h>
#include <ExecutionContextTask.h>

struct DECLSPEC_NOVTABLE IExecutionContext
{
    virtual
    ~IExecutionContext() = default;

    virtual
    void
    RegisterPoll(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll
    ) = 0;

    virtual
    void
    UnregisterPoll(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll
    ) = 0;

    virtual
    void
    ChangePollFunction(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll,
        _In_ PFN_EXECUTION_CONTEXT_POLL PollFn
    ) = 0;

    virtual
    void
    RegisterNotification(
        _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
    ) = 0;

    virtual
    void
    UnregisterNotification(
        _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
    ) = 0;

    virtual
    void
    QueueTask(
        _In_ EXECUTION_CONTEXT_TASK * Task
    ) = 0;

    virtual
    void
    Notify(
        void
    ) = 0;

    virtual
    void
    NotifyEx(
        _In_ EXECUTION_CONTEXT_NOTIFY_PARAMETERS * Parameters
    ) = 0;

    virtual
    void
    SetAffinity(
        _In_ const PROCESSOR_NUMBER & Processor
    ) = 0;
};
