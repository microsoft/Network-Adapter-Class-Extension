// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "UserModeExecutionContext.hpp"

EXTERN_C
NET_EXECUTION_CONTEXT_CREATE
    ExecutionContextCreate;

EXTERN_C
NET_EXECUTION_CONTEXT_DESTROY
    ExecutionContextDestroy;

EXTERN_C
NET_EXECUTION_CONTEXT_REGISTER_POLL
    ExecutionContextRegisterPoll;

EXTERN_C
NET_EXECUTION_CONTEXT_UNREGISTER_POLL
    ExecutionContextUnregisterPoll;

EXTERN_C
NET_EXECUTION_CONTEXT_REGISTER_NOTIFICATION
    ExecutionContextRegisterNotification;

EXTERN_C
NET_EXECUTION_CONTEXT_UNREGISTER_NOTIFICATION
    ExecutionContextUnregisterNotification;

EXTERN_C
NET_EXECUTION_CONTEXT_QUEUE_TASK
    ExecutionContextQueueTask;

EXTERN_C
NET_EXECUTION_CONTEXT_NOTIFY
    ExecutionContextNotify;

EXTERN_C
NET_EXECUTION_CONTEXT_NOTIFY_EX
    ExecutionContextNotifyEx;

UserModeExecutionContext *
ExecutionContextFromHandle(NET_EXECUTION_CONTEXT Handle)
{
    return reinterpret_cast<UserModeExecutionContext *>(Handle);
}

_Use_decl_annotations_
NTSTATUS
ExecutionContextCreate(
    EXECUTION_CONTEXT_CONFIGURATION const * Configuration,
    NET_EXECUTION_CONTEXT * ExecutionContext
)
{
    UNREFERENCED_PARAMETER(Configuration);

    auto executionContext = wil::make_unique_nothrow<UserModeExecutionContext>();

    if (!executionContext)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS ntStatus = executionContext->Initialize();

    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    *ExecutionContext = reinterpret_cast<NET_EXECUTION_CONTEXT>(executionContext.release());

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
ExecutionContextDestroy(
    NET_EXECUTION_CONTEXT ExecutionContext
)
{
    delete ExecutionContextFromHandle(ExecutionContext);
}

_Use_decl_annotations_
void
ExecutionContextRegisterPoll(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll
)
{
    ExecutionContextFromHandle(ExecutionContext)->RegisterPoll(Poll);
}

_Use_decl_annotations_
void
ExecutionContextUnregisterPoll(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll
)
{
    ExecutionContextFromHandle(ExecutionContext)->UnregisterPoll(Poll);
}

_Use_decl_annotations_
void
ExecutionContextRegisterNotification(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    ExecutionContextFromHandle(ExecutionContext)->RegisterNotification(Notification);
}

_Use_decl_annotations_
void
ExecutionContextUnregisterNotification(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    ExecutionContextFromHandle(ExecutionContext)->UnregisterNotification(Notification);
}

_Use_decl_annotations_
void
ExecutionContextQueueTask(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_TASK * Task
)
{
    ExecutionContextFromHandle(ExecutionContext)->QueueTask(Task);
}

_Use_decl_annotations_
void
ExecutionContextNotify(
    NET_EXECUTION_CONTEXT ExecutionContext
)
{
    ExecutionContextFromHandle(ExecutionContext)->Notify();
}

_Use_decl_annotations_
void
ExecutionContextNotifyEx(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFY_PARAMETERS * Parameters
)
{
    UNREFERENCED_PARAMETER(Parameters);
    ExecutionContextFromHandle(ExecutionContext)->Notify();
}

_Use_decl_annotations_
HANDLE
ExecutionContextGetUmFileHandle(
    NET_EXECUTION_CONTEXT ExecutionContext
)
{
    return ExecutionContextFromHandle(ExecutionContext)->GetHandle();
}

NET_EXECUTION_CONTEXT_DISPATCH Dispatch =
{
    sizeof(Dispatch),
    ExecutionContextCreate,
    ExecutionContextDestroy,
    ExecutionContextRegisterPoll,
    ExecutionContextUnregisterPoll,
    nullptr,
    ExecutionContextRegisterNotification,
    ExecutionContextUnregisterNotification,
    nullptr,
    nullptr,
    nullptr,
    ExecutionContextNotify,
    nullptr,
    nullptr,
    ExecutionContextNotifyEx,
    ExecutionContextQueueTask,
    nullptr,
    nullptr,
    ExecutionContextGetUmFileHandle
};

NET_EXECUTION_CONTEXT_DISPATCH const*
GetExecutionContextDispatch(
    void
)
{
    return &Dispatch;
}
