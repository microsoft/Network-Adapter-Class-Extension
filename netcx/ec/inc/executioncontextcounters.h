// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

struct EXECUTION_CONTEXT_COUNTERS
{
    UINT32 Iterations;
    UINT64 IdleIterations;
    UINT64 KernelModeNotifications;
    UINT64 UserModeNotifications;
    UINT64 Syscalls;
    UINT64 KernelModeWork;
    UINT64 UserModeWork;
};

#include <ExecutionContextPerformanceCounters.h>
