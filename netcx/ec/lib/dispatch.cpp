// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include <ExecutionContextDispatch.h>
#include <NdisStatisticalIoctls.h>
#include "ExecutionContext.hpp"
#include "HistogramHub.h"
#include "ExecutionContextStatistics.hpp"

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
NET_EXECUTION_CONTEXT_NOTIFY
    ExecutionContextNotify;

static
NET_EXECUTION_CONTEXT_COLLECT_HISTOGRAMS_IOCTL
    ExecutionContextCollectHistogramsIoctl;

static
NET_EXECUTION_CONTEXT_RESET_HISTOGRAMS
    ExecutionContextResetHistograms;

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
    NET_EXECUTION_CONTEXT * ExecutionContext
)
{
    if (Configuration->Size < sizeof(EXECUTION_CONTEXT_CONFIGURATION))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    auto executionContext = 
        wil::make_unique_nothrow<::ExecutionContext>(
            Configuration->ClientIdentifier,
            Configuration->RuntimeKnobs);

    if (!executionContext)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    auto const ntStatus = 
        executionContext->Initialize(
            &Configuration->ClientFriendlyName);

    if (STATUS_SUCCESS != ntStatus)
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
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);
    delete executionContext;
}

_Use_decl_annotations_
void
ExecutionContextRegisterPoll(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll
)
{
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);
    executionContext->RegisterPoll(Poll);
}

_Use_decl_annotations_
void
ExecutionContextUnregisterPoll(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_POLL * Poll
)
{
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);
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
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);

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
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);
    executionContext->RegisterNotification(Notification);
}

_Use_decl_annotations_
void
ExecutionContextUnregisterNotification(
    NET_EXECUTION_CONTEXT ExecutionContext,
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);
    executionContext->UnregisterNotification(Notification);
}

_Use_decl_annotations_
void
ExecutionContextSetWorkerThreadAffinity(
    NET_EXECUTION_CONTEXT ExecutionContext,
    GROUP_AFFINITY const * Affinity
)
{
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);
    executionContext->SetWorkerThreadAffinity(*Affinity);
}

_Use_decl_annotations_
void
ExecutionContextSetAffinity(
    NET_EXECUTION_CONTEXT ExecutionContext,
    PROCESSOR_NUMBER const * Affinity
)
{
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);

    GROUP_AFFINITY cpuGroupAffinity{};
    cpuGroupAffinity.Group = Affinity->Group;
    cpuGroupAffinity.Mask = static_cast<ULONG_PTR>(1ull << Affinity->Number);

    executionContext->SetWorkerThreadAffinity(cpuGroupAffinity);
    executionContext->SetDpcTargetProcessor(*Affinity);
}

_Use_decl_annotations_
void
ExecutionContextRunSynchronousTask(
    NET_EXECUTION_CONTEXT ExecutionContext,
    void * Context,
    PFN_EXECUTION_CONTEXT_TASK TaskFn
)
{
    ExecutionContextTask task{ Context, TaskFn };

    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);

    executionContext->QueueTask(task);
    task.WaitForCompletion();
}

_Use_decl_annotations_
void
ExecutionContextNotify(
    NET_EXECUTION_CONTEXT ExecutionContext
)
{
    auto executionContext = reinterpret_cast<::ExecutionContext *>(ExecutionContext);
    executionContext->Notify();
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
