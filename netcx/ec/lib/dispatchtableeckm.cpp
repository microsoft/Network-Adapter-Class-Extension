// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include <ExecutionContextDispatch.h>
#include <NdisStatisticalIoctls.h>
#include <IExecutionContext.hpp>
#include "ExecutionContext.hpp"
#include "HistogramHub.h"
#include "ExecutionContextStatistics.hpp"
#include "ExecutionContextTask.h"
#include "KWaitTask.hpp"
#include "DispatcherSubsystem.hpp"

void
UserModeExecutionContextNotify(
    NET_EXECUTION_CONTEXT ExecutionContext
);

void
UserModeExecutionContextRegisterPoll(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll
);

void
UserModeExecutionContextUnregisterPoll(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll
);

void
UserModeExecutionContextRegisterNotification(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFICATION * Notification
);

void
UserModeExecutionContextUnregisterNotification(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFICATION * Notification
);

void
UserModeExecutionContextQueueTask(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_TASK * Task
);

static
NET_EXECUTION_CONTEXT_CREATE
    ExecutionContextCreate;

static
NET_EXECUTION_CONTEXT_DESTROY
    ExecutionContextDestroy;

static
NET_EXECUTION_CONTEXT_REGISTER_POLL
    ExecutionContextRegisterPoll;

static
NET_EXECUTION_CONTEXT_UNREGISTER_POLL
    ExecutionContextUnregisterPoll;

static
NET_EXECUTION_CONTEXT_CHANGE_POLL_FUNCTION
    ExecutionContextChangePollFunction;

static
NET_EXECUTION_CONTEXT_REGISTER_NOTIFICATION
    ExecutionContextRegisterNotification;

static
NET_EXECUTION_CONTEXT_UNREGISTER_NOTIFICATION
    ExecutionContextUnregisterNotification;

static
NET_EXECUTION_CONTEXT_SET_WORKER_THREAD_AFFINITY
    ExecutionContextSetWorkerThreadAffinity;

static
NET_EXECUTION_CONTEXT_SET_AFFINITY
    ExecutionContextSetAffinity;

static
NET_EXECUTION_CONTEXT_RUN_SYNCHRONOUS_TASK
    ExecutionContextRunSynchronousTask;

static
NET_EXECUTION_CONTEXT_QUEUE_TASK
    ExecutionContextQueueTask;

static
NET_EXECUTION_CONTEXT_NOTIFY
    ExecutionContextNotify;

static
NET_EXECUTION_CONTEXT_COLLECT_HISTOGRAMS_IOCTL
    ExecutionContextCollectHistogramsIoctl;

static
NET_EXECUTION_CONTEXT_RESET_HISTOGRAMS
    ExecutionContextResetHistograms;

static
NET_EXECUTION_CONTEXT_NOTIFY_EX
    ExecutionContextNotifyEx;

NET_EXECUTION_CONTEXT_REFERENCE_BY_HANDLE
    ExecutionContextReferenceByHandle;

NET_EXECUTION_CONTEXT_DEREFERENCE
    ExecutionContextDereference;

static
NET_EXECUTION_CONTEXT_GET_UM_FILE_HANDLE
    ExecutionContextGetUmFileHandle;

NET_EXECUTION_CONTEXT_DISPATCH ExecutionContextDispatch =
{
    sizeof(NET_EXECUTION_CONTEXT_DISPATCH),
    ExecutionContextCreate,
    ExecutionContextDestroy,
    ExecutionContextRegisterPoll,
    ExecutionContextUnregisterPoll,
    ExecutionContextChangePollFunction,
    ExecutionContextRegisterNotification,
    ExecutionContextUnregisterNotification,
    ExecutionContextSetWorkerThreadAffinity,
    ExecutionContextSetAffinity,
    ExecutionContextRunSynchronousTask,
    ExecutionContextNotify,
    ExecutionContextCollectHistogramsIoctl,
    ExecutionContextResetHistograms,
    ExecutionContextNotifyEx,
    ExecutionContextQueueTask,
    ExecutionContextReferenceByHandle,
    ExecutionContextDereference,
    ExecutionContextGetUmFileHandle
};

_Use_decl_annotations_
NET_EXECUTION_CONTEXT_DISPATCH const *
GetExecutionContextDispatch(
    void
)
{
    return &ExecutionContextDispatch;
}

_Use_decl_annotations_
NTSTATUS
ExecutionContextCreate(
    EXECUTION_CONTEXT_CONFIGURATION const * Configuration,
    NET_EXECUTION_CONTEXT * NetExecutionContext
)
{
    if (Configuration->Size < sizeof(EXECUTION_CONTEXT_CONFIGURATION))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    wistd::unique_ptr<ExecutionContext> executionContext;

    auto ntStatus =
        ExecutionContext::Create(
            &Configuration->ClientFriendlyName,
            Configuration->ClientIdentifier,
            executionContext
        );

    if (STATUS_SUCCESS != ntStatus)
    {
        return ntStatus;
    }

    *NetExecutionContext = reinterpret_cast<NET_EXECUTION_CONTEXT>(
        static_cast<IExecutionContext*>(executionContext.release()));

    return ntStatus;
}

_Use_decl_annotations_
void
ExecutionContextDestroy(
    NET_EXECUTION_CONTEXT ExecutionContext
)
{
    auto executionContext = reinterpret_cast<::IExecutionContext *>(ExecutionContext);
    delete executionContext;
}

_Use_decl_annotations_
void
ExecutionContextRegisterPoll(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->RegisterPoll(Poll);
}

_Use_decl_annotations_
void
ExecutionContextUnregisterPoll(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->UnregisterPoll(Poll);
}

_Use_decl_annotations_
void
ExecutionContextChangePollFunction(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll,
    PFN_EXECUTION_CONTEXT_POLL PollFn
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);

    executionContext->ChangePollFunction(
        Poll,
        PollFn);
}

_Use_decl_annotations_
void
ExecutionContextRegisterNotification(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->RegisterNotification(Notification);
}

_Use_decl_annotations_
void
ExecutionContextUnregisterNotification(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->UnregisterNotification(Notification);
}

_Use_decl_annotations_
void
ExecutionContextSetWorkerThreadAffinity(
    NET_EXECUTION_CONTEXT ExecutionContext,
    GROUP_AFFINITY const * Affinity
)
{
    UNREFERENCED_PARAMETER((ExecutionContext, Affinity));
}

_Use_decl_annotations_
void
ExecutionContextSetAffinity(
    NET_EXECUTION_CONTEXT ExecutionContext,
    PROCESSOR_NUMBER const * Affinity
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->SetAffinity(*Affinity);
}

_Use_decl_annotations_
void
ExecutionContextRunSynchronousTask(
    NET_EXECUTION_CONTEXT ExecutionContext,
    void * Context,
    PFN_EXECUTION_CONTEXT_TASK TaskFn
)
{
    KWaitTask task{Context, TaskFn};

    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->QueueTask(task.GetTask());
    task.WaitForCompletion();
}

_Use_decl_annotations_
void
ExecutionContextQueueTask(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_TASK * Task
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->QueueTask(Task);
}

_Use_decl_annotations_
void
ExecutionContextNotify(
    NET_EXECUTION_CONTEXT ExecutionContext
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->Notify();
}

_Use_decl_annotations_
void
ExecutionContextNotifyEx(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFY_PARAMETERS * Parameters
)
{
    auto executionContext = reinterpret_cast<IExecutionContext *>(ExecutionContext);
    executionContext->NotifyEx(Parameters);
}

_Use_decl_annotations_
NTSTATUS
ExecutionContextCollectHistogramsIoctl(
    UCHAR *out,
    ULONG cbOut,
    ULONG *bytesNeeded
)
{
    auto & hub = GetGlobalEcStatistics();
    auto collectHistogramsOut = reinterpret_cast<NDIS_COLLECT_HISTOGRAM_OUT *>(out);

    return hub.Serialize(collectHistogramsOut, cbOut, *bytesNeeded);
}

_Use_decl_annotations_
void
ExecutionContextResetHistograms(
    void
)
{
    auto & hub = GetGlobalEcStatistics();
    hub.ResetHistogramsValues();
}

#ifndef _KERNEL_MODE

NTSTATUS
ExecutionContextReferenceByHandle(
    _In_ HANDLE EcUmHandle,
    _Out_ NET_EXECUTION_CONTEXT * EcKmHandle
)
{
    UNREFERENCED_PARAMETER(EcUmHandle);
    UNREFERENCED_PARAMETER(EcKmHandle);
    return 0;
}

void
ExecutionContextDereference(
    _In_ NET_EXECUTION_CONTEXT EcKmHandle
)
{
    UNREFERENCED_PARAMETER(EcKmHandle);
}

#endif

HANDLE
ExecutionContextGetUmFileHandle(
    _In_ NET_EXECUTION_CONTEXT EcKmHandle
)
{
    // UM File Handle only exists in UM counterpart of the UMDF version of ExecutionContext;
    // Since dispatch table is shared between KMDF and UMDF ECs, leaving this unimlemented 
    // in KMDF version
    UNREFERENCED_PARAMETER(EcKmHandle);
    return 0;
}
