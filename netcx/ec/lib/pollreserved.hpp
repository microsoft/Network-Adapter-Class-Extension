// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "ExecutionContextStatistics.hpp"
#include <ExecutionContextPoll.h>
#include <KPtr.h>

typedef struct _EXECUTION_CONTEXT_POLL EXECUTION_CONTEXT_POLL;

// This structure is stored in EXECUTION_CONTEXT_POLL's Reserved field.
struct PollReserved
{
    KPoolPtr<Statistics::Histogram>
        histogram;
};

static_assert(sizeof(PollReserved) <= FIELD_SIZE(EXECUTION_CONTEXT_POLL, Reserved));

NONPAGED
PollReserved &
ContextFromPoll(
    _In_ EXECUTION_CONTEXT_POLL * Poll
);
