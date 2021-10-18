// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "ExecutionContextTask.hpp"

_Use_decl_annotations_
ExecutionContextTask *
ExecutionContextTask::FromLink(
    LIST_ENTRY * Link
)
{
    return CONTAINING_RECORD(
        Link,
        ExecutionContextTask,
        m_linkage);
}

_Use_decl_annotations_
ExecutionContextTask::ExecutionContextTask(
    void * TaskContext,
    PFN_EXECUTION_CONTEXT_TASK TaskFunction,
    bool SignalCompletion
)
    : m_context(TaskContext)
    , m_taskFn(TaskFunction)
    , m_signalCompletion(SignalCompletion)
{
    InitializeListHead(&m_linkage);
}

_Use_decl_annotations_
bool
ExecutionContextTask::AddToList(
    LIST_ENTRY * ListHead
)
{
    if (!IsListEmpty(&m_linkage))
    {
        return false;
    }

    InsertTailList(ListHead, &m_linkage);

    return true;
}

_Use_decl_annotations_
void
ExecutionContextTask::WaitForCompletion(
    void
)
{
    NT_FRE_ASSERT(m_signalCompletion);
    m_completed.Wait();
}
