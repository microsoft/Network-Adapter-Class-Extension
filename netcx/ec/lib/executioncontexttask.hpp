// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <kwaitevent.h>
#include <ExecutionContextTask.h>

class ExecutionContextTask
{
    friend class
        ExecutionContext;

public:

    _IRQL_requires_max_(PASSIVE_LEVEL)
    ExecutionContextTask(
        _In_ void * TaskContext,
        _In_ PFN_EXECUTION_CONTEXT_TASK TaskFn,
        _In_ bool SignalCompletion = true
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    virtual
    ~ExecutionContextTask(
        void
    ) = default;

    _IRQL_requires_max_(PASSIVE_LEVEL)
    virtual
    void
    WaitForCompletion(
        void
    );

private:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    static
    ExecutionContextTask *
    FromLink(
        _In_ LIST_ENTRY * Link
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    AddToList(
        _In_ LIST_ENTRY * ListHead
    );

private:

    void * const
        m_context;

    PFN_EXECUTION_CONTEXT_TASK const
        m_taskFn;

    LIST_ENTRY
        m_linkage = {};

    bool const
        m_signalCompletion;

    KAutoEvent
        m_completed;
};
