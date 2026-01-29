// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>
#include <KLoaderTypes.h>
#include <KMacros.h>

#include <ExecutionContext.h>
#include <ExecutionContextDispatch.h>
#include <ExecutionContextTask.h>

class ExecutionContextWrapper : public NONPAGED_OBJECT<'xtCE'>
{
public:

    ExecutionContextWrapper(
        _In_ GUID const * ClientIdentifier = nullptr,
        _In_ EXECUTION_CONTEXT_RUNTIME_KNOBS const * RuntimeMutableKnobs = nullptr
    );

    ~ExecutionContextWrapper();

    _No_competing_thread_
    NTSTATUS
    Initialize(
        _In_opt_ UNICODE_STRING const* clientFriendlyName = nullptr
    );

    _IRQL_requires_max_(HIGH_LEVEL)
    void
    Notify();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SetAffinity(
        _In_ PROCESSOR_NUMBER const* Affinity
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    RegisterPoll(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UnregisterPoll(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    QueueTask(
        EXECUTION_CONTEXT_TASK * Task
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    ChangePollFunction(
        _Inout_ EXECUTION_CONTEXT_POLL * Poll,
        _In_ PFN_EXECUTION_CONTEXT_POLL PollFn
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    RegisterNotification(
        _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UnregisterNotification(
        _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
    );

    NET_EXECUTION_CONTEXT_DISPATCH *
    GetExecutionContextDispatch();

    NET_EXECUTION_CONTEXT
    GetHandle();

private:

    NTSTATUS
    InitializeExecutionContextHandle(
        _In_opt_ UNICODE_STRING const* clientFriendlyName = nullptr
    );

private:

    GUID const *
        m_clientIdentifier;

    EXECUTION_CONTEXT_RUNTIME_KNOBS const *
        m_runtimeMutableKnobs;

    NET_EXECUTION_CONTEXT
        m_executionContextHandle{};

    NET_EXECUTION_CONTEXT_DISPATCH
        m_dispatchTable;

};
