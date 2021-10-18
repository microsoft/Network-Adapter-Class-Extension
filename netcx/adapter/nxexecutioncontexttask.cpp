// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxExecutionContext.hpp"
#include "NxExecutionContextTask.hpp"
#include "NxExecutionContextTask.tmh"

struct DeferredDeleteContext
{
    NxExecutionContextTask *
        Task;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DeferredDeleteContext, GetDeferredDeleteContext);

static
EVT_WDF_WORKITEM
    EvtTaskDeferredDelete;

void
EvtExecutionContextTask(
    void * TaskContext
)
{
    auto nxExecutionContextTask = static_cast<NxExecutionContextTask *>(TaskContext);

    nxExecutionContextTask->TaskHandler();

    auto taskHandle = nxExecutionContextTask->GetHandle();

    // Release the reference on the task object, this may invoke the destructor if the object
    // is running down
    WdfObjectDereferenceWithTag(taskHandle, EvtExecutionContextTask);
}

NxExecutionContextTask::NxExecutionContextTask(
    NETEXECUTIONCONTEXT NetExecutionContextHandle,
    PFN_NET_EXECUTION_CONTEXT_TASK EvtTask
)
    : ExecutionContextTask(this, EvtExecutionContextTask, false)
    , m_handle(static_cast<NETEXECUTIONCONTEXTTASK>(WdfObjectContextGetObject(this)))
    , m_callback(EvtTask)
    , m_executionContext(NetExecutionContextHandle)
{
    // These references are taken away when the task object is being deleted, after it is run down
    WdfObjectReferenceWithTag(m_handle, this);
    WdfObjectReferenceWithTag(m_executionContext, this);

    // Initial task state is 'completed'
    m_taskCompleted.Set();
}

_Use_decl_annotations_
NTSTATUS
NxExecutionContextTask::Initialize(
    void
)
{
    // Create a work item to use when we need to defer deletion. The parent of this work
    // item can't be the task nor the EC as we would not be able to queue it from the task
    // object's EvtContextCleanup callback
    WDF_WORKITEM_CONFIG config;
    WDF_WORKITEM_CONFIG_INIT(&config, EvtTaskDeferredDelete);

    auto executionContext = GetExecutionContextFromHandle(m_executionContext);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DeferredDeleteContext);
    attributes.ParentObject = executionContext->GetDevice();

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfWorkItemCreate(
            &config,
            &attributes,
            &m_deferredDelete));

    auto context = GetDeferredDeleteContext(m_deferredDelete.get());
    context->Task = this;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
EvtTaskDeferredDelete(
    WDFWORKITEM WorkItem
)
{
    auto context = GetDeferredDeleteContext(WorkItem);
    context->Task->OnDelete();
}

_Use_decl_annotations_
void
NxExecutionContextTask::OnDelete(
    void
)
{
    auto const currentThread = KeGetCurrentThread();

    // If the task is being deleted from the task callback function we need to queue an work item
    // to finish the operation
    if (currentThread == m_taskThread)
    {
        LogInfo(
            FLAG_GENERAL,
            "Task object %p is being deleted from the task callback, "
            "will defer operation to work item %p",
            m_handle,
            m_deferredDelete.get());

        WdfWorkItemEnqueue(m_deferredDelete.get());
    }
    else
    {
        LogInfo(
            FLAG_GENERAL,
            "Task object %p is being deleted from outside the task callback, "
            "will wait for completion and release object references inline",
            m_handle);

        WaitForCompletion();

        // Release the references acquired in the constructor
        WdfObjectDereferenceWithTag(m_executionContext, this);
        WdfObjectDereferenceWithTag(m_handle, this);
    }
}

_Use_decl_annotations_
NETEXECUTIONCONTEXTTASK
NxExecutionContextTask::GetHandle(
    void
)
{
    return m_handle;
}

_Use_decl_annotations_
void
NxExecutionContextTask::TaskHandler(
    void
)
{
    KAcquireSpinLock lock { m_lock };
    m_taskQueued = false;
    lock.Release();

    // Forward to client driver's task function.
    m_taskThread = KeGetCurrentThread();
    m_callback(m_handle);
    m_taskThread = nullptr;

    lock.Acquire();
    m_taskCompleted.Set();
    lock.Release();
}

_Use_decl_annotations_
void
NxExecutionContextTask::WaitForCompletion(
    void
)
{
    m_taskCompleted.Wait();
}

_Use_decl_annotations_
void
NxExecutionContextTask::Enqueue(
    void
)
{
    KAcquireSpinLock lock { m_lock };

    m_taskCompleted.Clear();
    m_taskQueued = true;

    // This reference will be released after the task is executed
    WdfObjectReferenceWithTag(m_handle, EvtExecutionContextTask);

    lock.Release();

    auto ec = GetExecutionContextFromHandle(m_executionContext);
    ec->QueueTask(*this);
}

_Use_decl_annotations_
bool
NxExecutionContextTask::IsTaskQueued(
    void
)
{
    KAcquireSpinLock lock { m_lock };
    return m_taskQueued;
}
