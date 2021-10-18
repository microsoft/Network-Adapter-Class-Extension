// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

// Keep the enum values sorted by polling priority
typedef enum _EXECUTION_CONTEXT_POLL_TYPE
{
    ExecutionContextPollTypePreAdvance = 2000,
    ExecutionContextPollTypeNdisMiniportDriverPoll = 4000,
    ExecutionContextPollTypeAdvance = 4000,
    ExecutionContextPollTypeNdisMiniportPostDriverPoll = 6000,
    ExecutionContextPollTypePostAdvance = 6000,
} EXECUTION_CONTEXT_POLL_TYPE;

typedef struct _EXECUTION_CONTEXT_POLL_PARAMETERS
{
    _In_ KIRQL Irql;
    _Reserved_ UCHAR Reserved1[3];

    _Out_ ULONG WorkCounter;

    void *Reserved3[3];
} EXECUTION_CONTEXT_POLL_PARAMETERS;

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
void
EVT_EXECUTION_CONTEXT_POLL(
    _In_ void * Context,
    _Inout_ EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
);

typedef EVT_EXECUTION_CONTEXT_POLL *PFN_EXECUTION_CONTEXT_POLL;

typedef struct _EXECUTION_CONTEXT_POLL
{
    EXECUTION_CONTEXT_POLL_TYPE
        Type;

    ULONG
        Tag;

    void *
        Context;

    PFN_EXECUTION_CONTEXT_POLL
        PollFn;

    LIST_ENTRY
        Link;

    void *
        Reserved[4];
} EXECUTION_CONTEXT_POLL;

inline
void
INITIALIZE_EXECUTION_CONTEXT_POLL(
    _Out_ EXECUTION_CONTEXT_POLL * Poll,
    _In_ _EXECUTION_CONTEXT_POLL_TYPE Type,
    _In_opt_ void * Context,
    _In_ PFN_EXECUTION_CONTEXT_POLL PollFn
)
{
    RtlZeroMemory(Poll, sizeof(*Poll));
    Poll->Type = Type;
    Poll->Context = Context;
    Poll->PollFn = PollFn;
    InitializeListHead(&Poll->Link);
}

inline
void
INITIALIZE_EXECUTION_CONTEXT_POLL_WITH_TAG(
    _Out_ EXECUTION_CONTEXT_POLL * Poll,
    _In_ _EXECUTION_CONTEXT_POLL_TYPE Type,
    _In_opt_ void * Context,
    _In_ PFN_EXECUTION_CONTEXT_POLL PollFn,
    _In_ ULONG Tag
)
{
    INITIALIZE_EXECUTION_CONTEXT_POLL(
        Poll,
        Type,
        Context,
        PollFn);

    Poll->Tag = Tag;
}
