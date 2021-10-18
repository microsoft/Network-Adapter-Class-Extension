// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include "TimeBudget.h"

static const ULONG ONE_HOUR_MS = 60ul * 60ul * 1000ul;
static const ULONG MS_TO_TIME_INCREMENT_MULT = 10ul * 1000ul;

#ifdef _KERNEL_MODE
_Use_decl_annotations_
TimeBudget::TimeBudget(
    ULONG MaxTimeMs,
    bool TryExtendMaxTime,
    UINT32 WatchdogDpcStarvationLimit,
    KIRQL CurrentIrql
)
    : m_tryExtendMaxTime(TryExtendMaxTime)
    // 1 unit of "time increment" == 100 nanoseconds, typical value is 10ms or 10 * 10 * 1000 "TimeIncrement" units
    , m_timeIncrement(KeQueryTimeIncrement())
    , m_allowedTimeDurationInTicks(min(MaxTimeMs, ONE_HOUR_MS) * MS_TO_TIME_INCREMENT_MULT / m_timeIncrement)
    , m_watchdogDpcStarvationLimit(WatchdogDpcStarvationLimit)
    , m_irql(CurrentIrql)
{
}
#else
_Use_decl_annotations_
TimeBudget::TimeBudget(
    ULONG,
    bool TryExtendMaxTime,
    UINT32 WatchdogDpcStarvationLimit,
    KIRQL
)
    : m_tryExtendMaxTime(TryExtendMaxTime)
    , m_timeIncrement(0)
    , m_allowedTimeDurationInTicks(0)
    , m_watchdogDpcStarvationLimit(WatchdogDpcStarvationLimit)
    , m_irql(0)
{
}
#endif

void
TimeBudget::Reset(
    void
)
{
#ifdef _KERNEL_MODE
    KeQueryTickCount(&m_startTimeTicks);
    m_desiredEndTimeTicks.QuadPart = m_startTimeTicks.QuadPart + m_allowedTimeDurationInTicks;
#endif
}

_Use_decl_annotations_
bool
TimeBudget::IsOutOfBudget(
    void
)
{
#ifdef _KERNEL_MODE
    if (m_irql == PASSIVE_LEVEL)
    {
        // In passive mode we always have "budget" to run
        return false;
    }

    if (m_allowedTimeDurationInTicks > 0)
    {
        LARGE_INTEGER currentTime;
        KeQueryTickCount(&currentTime);

        if (currentTime.QuadPart > m_startTimeTicks.QuadPart && m_startTimeTicks.QuadPart != 0)
        {
            m_totalTimeInDispatch = static_cast<ULONG>(currentTime.QuadPart - m_startTimeTicks.QuadPart);
        }

        if (currentTime.QuadPart > m_desiredEndTimeTicks.QuadPart)
        {
            if (m_tryExtendMaxTime)
            {
                // We run out of time, but we only stop the DPC if there is something
                // else to run on the CPU. Otherwise nothing stops us from running a bit longer
                bool shouldYield = KeShouldYieldProcessor();
                if (shouldYield)
                {
                    m_outOfBudgetReason = OutOfBudgetReason::KeShouldYield;
                }
                return shouldYield;
            }
            else
            {
                m_outOfBudgetReason = OutOfBudgetReason::DpcTimeEplapsed;
                return true;
            }
        }

        return IsDpcWatchdogStarving();
    }
    else
    {
        bool shouldYield = KeShouldYieldProcessor();
        if (shouldYield)
        {
            m_outOfBudgetReason = OutOfBudgetReason::KeShouldYield;
        }
        return shouldYield;
    }

#else
    return false;
#endif
}

_Use_decl_annotations_
ULONG
TimeBudget::TotalTimeInDispatch(
    void
) const
{
    return m_totalTimeInDispatch;
}

_Use_decl_annotations_
bool
TimeBudget::IsDpcWatchdogStarving(
    void
)
{
#ifdef _KERNEL_MODE
    KDPC_WATCHDOG_INFORMATION WatchDogInfo;
    NTSTATUS NtStatus = KeQueryDpcWatchdogInformation(&WatchDogInfo);

    if (!NT_SUCCESS(NtStatus))
    {
        return false;
    }

    // Single DPC limit
    if (WatchDogInfo.DpcTimeLimit)
    {
        // DpcTimeLimit
        // Time limit for a single, current deferred procedure call. If DPC time-out has been disabled, this value is set to 0.

        // DpcTimeCount
        // Time remaining for the current deferred procedure call, if DPC time-out has been enabled.
        ULONG const dpcWatchdogPercent = WatchDogInfo.DpcTimeCount * 100;
        bool limitExceeded = dpcWatchdogPercent < WatchDogInfo.DpcTimeLimit * (100 - m_watchdogDpcStarvationLimit);

        if (limitExceeded)
        {
            m_outOfBudgetReason = OutOfBudgetReason::SingleDpcWatchdog;
            return limitExceeded;
        }
    }

    // Cumulative limit, either within a series of DPCs or if IRQL was raised manually
    if (WatchDogInfo.DpcWatchdogLimit)
    {
        // DpcWatchdogLimit
        // Total time limit permitted for a sequence of deferred procedure calls. If DPC watchdog has been disabled, this value is set to zero.

        // DpcWatchdogCount
        // Time value remaining for the current sequence of deferred procedure calls, if enabled.
        ULONG const dpcWatchdogPercent = WatchDogInfo.DpcWatchdogCount * 100;
        bool limitExceeded = dpcWatchdogPercent < WatchDogInfo.DpcWatchdogLimit * (100 - m_watchdogDpcStarvationLimit);

        if (limitExceeded)
        {
            m_outOfBudgetReason = OutOfBudgetReason::CumulativeDpcWatchdog;
            return limitExceeded;
        }
    }

    return false;
#else
    return false;
#endif
}
