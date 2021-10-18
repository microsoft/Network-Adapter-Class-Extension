// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include <ExecutionContextPoll.h>
#include "HistogramHub.h"
#include "ExecutionContextStatistics.hpp"

static Statistics::Hub *ecStatistics = nullptr;

static RTL_RUN_ONCE ecRunOnce = RTL_RUN_ONCE_INIT;
static PAGEDX RTL_RUN_ONCE_INIT_FN InitializeEcStatOnce;

PAGED
NTSTATUS
InitializeGlobalEcStatistic()
{
    PAGED_CODE();
    // assuming creation can only happen once, and is never concurrent with getGlobalEcStatistics();
    auto ntStatus = RtlRunOnceExecuteOnce(
        &ecRunOnce,
        InitializeEcStatOnce,
        nullptr,
        nullptr);

    if (STATUS_SUCCESS != ntStatus)
        return ntStatus;

    return STATUS_SUCCESS;
}

PAGEDX
static
ULONG
NTAPI
InitializeEcStatOnce(
    PRTL_RUN_ONCE RunOnce,
    PVOID Parameter,
    PVOID *Context)
{
    UNREFERENCED_PARAMETER((RunOnce, Parameter, Context));
    NT_FRE_ASSERT(ecStatistics == nullptr);
    ecStatistics = new(std::nothrow) Statistics::Hub {};
    if (!ecStatistics)
    {
        return FALSE;
    }
    return TRUE;
}

NONPAGED
Statistics::Hub&
GetGlobalEcStatistics()
{
    NT_FRE_ASSERT(ecStatistics != nullptr);
    return *ecStatistics;
}

PAGED
void
FreeGlobalEcStatistics()
{
    if (ecStatistics == nullptr)
    {
        return;
    }

    ecStatistics->~Hub();
    ExFreePool(ecStatistics);
    ecStatistics = nullptr;
    ecRunOnce = RTL_RUN_ONCE_INIT;
}
