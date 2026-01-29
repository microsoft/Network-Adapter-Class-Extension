// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

struct DispatcherCounters
{
    union
    {
        UINT64 AsArray[static_cast<UINT32>(DispatcherStateChangeReason::Max)];
        struct
        {
            UINT64 InsertWorker;
            UINT64 DpcScheduled;
            UINT64 ThreadScheduled;
            UINT64 ContextSteal;
            UINT64 WorkerNeedsPassiveLevel;
            UINT64 DpcBudgetExceeded;
            UINT64 ArbitraryDpcBudgetExceeded;
            UINT64 Idle;
            UINT64 KnobsReload;
        } AsCounters;

    } StateChangeReason;

    static_assert(
        sizeof(StateChangeReason.AsArray) ==
        sizeof(StateChangeReason.AsCounters));

    union
    {
        UINT64 AsArray[DispatchPriorityMax];
        struct
        {
            UINT64 HighPriority;
            UINT64 NormalPriority;
        } AsCounters;
    } ActiveList;

    static_assert(
        sizeof(ActiveList.AsArray) ==
        sizeof(ActiveList.AsCounters));

    union
    {
        UINT64 AsArray[DispatchPriorityMax];
        struct
        {
            UINT64 HighPriority;
            UINT64 NormalPriority;
        } AsCounters;
    } ReadyList;

    static_assert(
        sizeof(ReadyList.AsArray) ==
        sizeof(ReadyList.AsCounters));
};
