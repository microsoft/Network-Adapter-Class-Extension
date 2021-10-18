// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "Nx.hpp"

#include <kspinlock.h>
#include <kwaitevent.h>

#include "ExecutionContextTask.hpp"


// Wraps what the client driver want to run as task, so that we can
// maintain EC and EcTask's reference count:
//     1) increment/decrement when the task is queued/finished.
//     2) increment/decrement when waiting task completion.
class NxExecutionContextTask
    : public ExecutionContextTask
{

public:

    NxExecutionContextTask(
        _In_ NETEXECUTIONCONTEXT NetExecutionContextHandle,
        _In_ PFN_NET_EXECUTION_CONTEXT_TASK EvtTask
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    OnDelete(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NETEXECUTIONCONTEXTTASK
    GetHandle(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Enqueue(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    WaitForCompletion(
        void
    ) override;

    _IRQL_requires_max_(PASSIVE_LEVEL)
    bool
    IsTaskQueued(
        void
    );

private:

    NETEXECUTIONCONTEXTTASK const
        m_handle;

    wil::unique_wdf_work_item
        m_deferredDelete;

    // What the driver really wants to run.
    PFN_NET_EXECUTION_CONTEXT_TASK
        m_callback;

    KSpinLock
        m_lock;

    _Guarded_by_(m_lock)
    KWaitEvent
        m_taskCompleted;

    // To prevent requeueing unfinished task.
    _Guarded_by_(m_lock)
    bool
        m_taskQueued = false;

    // Used to dereference the EC where the task is running.
    NETEXECUTIONCONTEXT const
        m_executionContext;

    // Used to detect if WdfObjectDelete is being called from task function
    PKTHREAD
        m_taskThread = nullptr;

    // Protect TaskHandler from external use by
    // making EvtExecutionContextTask a friend function.
    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    TaskHandler(
        void
    );

    friend void EvtExecutionContextTask(void * TaskContext);

};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxExecutionContextTask, GetNxExecutionContextTaskFromHandle);
