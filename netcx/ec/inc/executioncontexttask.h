// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
void
EVT_EXECUTION_CONTEXT_TASK(
    _In_ void * Context
);

typedef EVT_EXECUTION_CONTEXT_TASK *PFN_EXECUTION_CONTEXT_TASK;

typedef struct _EXECUTION_CONTEXT_TASK
{
    void *
        Context;

    PFN_EXECUTION_CONTEXT_TASK
        TaskFn;

    LIST_ENTRY
        Linkage;

    _Reserved_ void * 
        Reserved[4];
} EXECUTION_CONTEXT_TASK;

inline
void
INITIALIZE_EXECUTION_CONTEXT_TASK(
    _Out_ EXECUTION_CONTEXT_TASK * Task,
    _In_ void * TaskContext,
    _In_ PFN_EXECUTION_CONTEXT_TASK TaskFn
)
{
    RtlZeroMemory(Task, sizeof(*Task));
    Task->Context = TaskContext;
    Task->TaskFn = TaskFn;
    InitializeListHead(&Task->Linkage);
}
