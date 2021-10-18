// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

enum class OutOfBudgetReason
{
    None,
    SingleDpcWatchdog,
    CumulativeDpcWatchdog,
    KeShouldYield,
    DpcTimeEplapsed
};

class TimeBudget
{
public:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    TimeBudget(
        _In_ ULONG MaxTimeMs,
        _In_ bool TryExtendMaxTime,
        _In_ UINT32 WatchdogDpcStarvationLimit,
        _In_ KIRQL CurrentIrql
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsOutOfBudget(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    TotalTimeInDispatch(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Reset(
        void
    );

private:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsDpcWatchdogStarving(
        void
    );

private:

    bool const
        m_tryExtendMaxTime {};

    ULONG const
        m_timeIncrement {};

    ULONG const
        m_allowedTimeDurationInTicks {};

    UINT32
        m_watchdogDpcStarvationLimit {};

    LARGE_INTEGER
        m_desiredEndTimeTicks {};

    KIRQL
        m_irql {};

    LARGE_INTEGER
        m_startTimeTicks {};

    ULONG
        m_totalTimeInDispatch {};

    OutOfBudgetReason
        m_outOfBudgetReason = OutOfBudgetReason::None;
};
