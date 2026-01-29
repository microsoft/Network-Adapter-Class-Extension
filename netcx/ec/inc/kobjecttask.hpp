// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <ExecutionContextTask.h>

template <typename T, void (T::*Method)(void)>
class KObjectTask
{
public:

    _IRQL_requires_(PASSIVE_LEVEL)
    KObjectTask(
        _In_ T& Context
    )
        : m_context(Context)
    {
        INITIALIZE_EXECUTION_CONTEXT_TASK(
            &m_task,
            this,
            TaskThunk);
    }

    _IRQL_requires_(DISPATCH_LEVEL)
    EXECUTION_CONTEXT_TASK
    GetUnderlyingEcTask(
        void
    )
    {
        return m_task;
    }

private:

    _IRQL_requires_(DISPATCH_LEVEL)
    static
    void
    TaskThunk(
        _In_opt_ void* Context
    )
    {
        auto objectTask = static_cast<KObjectTask *>(Context);
        (objectTask->m_context.*Method)();
    }

private:

    T&
        m_context;

    EXECUTION_CONTEXT_TASK
        m_task;
};
