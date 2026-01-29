// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <kwaitevent.h>
#include <ExecutionContextTask.h>

inline
void
HandleKWaitTask(void *);

/*
Description:
    This class is used to manipulate an EXECUTION_CONTEXT_TASK instance, adding to it a safe
    way to wait for the task completion
    
Usage:
    Instanciate it using the same parameters required to create a EXECUTION_CONTEXT_TASK
    instance, use GetTask() to queue it to EC and block the code execution until it gets executed
    using WaitForCompletion()

    Control memory lifecycle, make sure to not free or unstack the memory used to store
    the instance and its contents until the task is finished
--*/
class KWaitTask
{
public:

    KWaitTask(
        void * Context,
        PFN_EXECUTION_CONTEXT_TASK Fn):
        m_context(Context),
        m_fn(Fn)
    {
        INITIALIZE_EXECUTION_CONTEXT_TASK(
            &m_task,
            this,
            HandleKWaitTask
        );
    }
    
    EXECUTION_CONTEXT_TASK * GetTask()
    {
        return &m_task;
    }

    void WaitForCompletion(){
        m_completed.Wait();
    }

    void ExecuteTask(){
        m_fn(m_context);
        m_completed.Set();
    }

private:

    EXECUTION_CONTEXT_TASK
        m_task;

    KAutoEvent
        m_completed;

    void*
        m_context;

    PFN_EXECUTION_CONTEXT_TASK
        m_fn;
    
};

inline
void
HandleKWaitTask(void * Context)
{
    auto kWaitTask = reinterpret_cast<KWaitTask *>(Context);
    kWaitTask->ExecuteTask();
}

template <typename LambdaType>
class LambdaTask : public KWaitTask
{
public:

    LambdaTask(
        LambdaType Lambda
    )
        : KWaitTask{ this, TaskThunk }
        , m_lambda{ Lambda }
    {
    }

    static
    void
    TaskThunk(
        void * Context
    )
    {
        static_cast<LambdaTask *>(Context)->m_lambda();
    }

private:

    LambdaType
        m_lambda{};
};
