// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <ExecutionContext.h>
#include <ExecutionContextPoll.h>
#include <ExecutionContextNotification.h>
#include <ExecutionContextTask.h>

#ifdef __cplusplus
extern "C" {
#endif

// {9C0B898D-6275-48EC-81B4-E5EDBE44B535}
DEFINE_GUID(EXECUTION_CONTEXT_MODULE_ID, 
    0x9c0b898d, 0x6275, 0x48ec, 0x81, 0xb4, 0xe5, 0xed, 0xbe, 0x44, 0xb5, 0x35);

// {53FE9525-4B95-47AF-8B1F-F278B7D588FD}
DEFINE_GUID(EXECUTION_CONTEXT_DISPATCH_TABLE_ID, 
    0x53fe9525, 0x4b95, 0x47af, 0x8b, 0x1f, 0xf2, 0x78, 0xb7, 0xd5, 0x88, 0xfd);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NET_EXECUTION_CONTEXT_CREATE(
    _In_ EXECUTION_CONTEXT_CONFIGURATION const * Configuration,
    _Out_ NET_EXECUTION_CONTEXT * ExecutionContext
);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_DESTROY(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext
);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_REGISTER_POLL(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext,
    _Inout_ EXECUTION_CONTEXT_POLL * Poll
);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_UNREGISTER_POLL(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext,
    _Inout_ EXECUTION_CONTEXT_POLL * Poll
);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_CHANGE_POLL_FUNCTION(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext,
    _Inout_ EXECUTION_CONTEXT_POLL * Poll,
    _In_ PFN_EXECUTION_CONTEXT_POLL PollFn
);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_REGISTER_NOTIFICATION(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext,
    _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_UNREGISTER_NOTIFICATION(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext,
    _Inout_ EXECUTION_CONTEXT_NOTIFICATION * Notification
);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_SET_WORKER_THREAD_AFFINITY(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext,
    _In_ GROUP_AFFINITY const * Affinity
);

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
void
NET_EXECUTION_CONTEXT_SET_AFFINITY(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext,
    _In_ PROCESSOR_NUMBER const * Affinity
);

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_RUN_SYNCHRONOUS_TASK(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext,
    _In_ void * Context,
    _In_ PFN_EXECUTION_CONTEXT_TASK TaskFn
);

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
void
NET_EXECUTION_CONTEXT_NOTIFY(
    _In_ NET_EXECUTION_CONTEXT ExecutionContext
);

typedef
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NET_EXECUTION_CONTEXT_COLLECT_HISTOGRAMS_IOCTL(
    UCHAR *out,
    ULONG cbOut,
    ULONG *bytesNeeded
);

typedef
_IRQL_requires_(PASSIVE_LEVEL)
void
NET_EXECUTION_CONTEXT_RESET_HISTOGRAMS(
    void
);

typedef struct _NET_EXECUTION_CONTEXT_DISPATCH
{
    SIZE_T
        Size;

    NET_EXECUTION_CONTEXT_CREATE *
        Create;

    NET_EXECUTION_CONTEXT_DESTROY *
        Destroy;

    NET_EXECUTION_CONTEXT_REGISTER_POLL *
        RegisterPoll;

    NET_EXECUTION_CONTEXT_UNREGISTER_POLL *
        UnregisterPoll;

    NET_EXECUTION_CONTEXT_CHANGE_POLL_FUNCTION *
        ChangePollFunction;

    NET_EXECUTION_CONTEXT_REGISTER_NOTIFICATION *
        RegisterNotification;

    NET_EXECUTION_CONTEXT_UNREGISTER_NOTIFICATION *
        UnregisterNotification;

    // Deprecated, will be removed when NetCx is refactored to use SetAffinity
    NET_EXECUTION_CONTEXT_SET_WORKER_THREAD_AFFINITY *
        SetWorkerThreadAffinity;

    NET_EXECUTION_CONTEXT_SET_AFFINITY *
        SetAffinity;

    NET_EXECUTION_CONTEXT_RUN_SYNCHRONOUS_TASK *
        RunSynchronousTask;

    NET_EXECUTION_CONTEXT_NOTIFY *
        Notify;

    NET_EXECUTION_CONTEXT_COLLECT_HISTOGRAMS_IOCTL *
        CollectHistogramsIoctl;

    NET_EXECUTION_CONTEXT_RESET_HISTOGRAMS *
        ResetHistograms;

} NET_EXECUTION_CONTEXT_DISPATCH;

#ifdef __cplusplus
}
#endif
