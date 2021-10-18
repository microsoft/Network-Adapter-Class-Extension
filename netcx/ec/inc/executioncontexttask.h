// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
void
EVT_EXECUTION_CONTEXT_TASK(
    _In_ void * Context
);

typedef EVT_EXECUTION_CONTEXT_TASK *PFN_EXECUTION_CONTEXT_TASK;
