// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"
#include "TimeBudgetWarningLogger.h"
#include "EcEvents.h"

static const ULONG MS_TO_TIME_INCREMENT_MULT = 10ul * 1000ul;

#ifndef _KERNEL_MODE
ULONG KeQueryTimeIncrement()
{
    return 1L;
}

VOID KeQueryTickCount (
    _Out_ PLARGE_INTEGER CurrentCount
)
{
    CurrentCount->QuadPart = 0L;
}
#endif

PAGED
TimeBudgetWarningLogger::TimeBudgetWarningLogger(
    const GUID& ClientIdentifier,
    const ULONG DispatchTimeWarningInterval
)
    : m_reportIntervalTicks(DispatchTimeWarningInterval * MS_TO_TIME_INCREMENT_MULT / KeQueryTimeIncrement())
    , m_clientIdentifier(ClientIdentifier)
    , m_dispatchTimeWarningIntervalMs(DispatchTimeWarningInterval)
{
}

PAGED
void TimeBudgetWarningLogger::SetWatermark(
    _In_ ULONG DispatchIrqlWarningWatermarkMs
)
{
    m_watermarkTicks = DispatchIrqlWarningWatermarkMs * MS_TO_TIME_INCREMENT_MULT / KeQueryTimeIncrement();
    m_watermarkMs = DispatchIrqlWarningWatermarkMs;
}

NONPAGED
void TimeBudgetWarningLogger::LogWarningIfOvertime(
    _In_ const TimeBudget& timeBudget
)
{
    const auto totalTimeInDispatch = timeBudget.TotalTimeInDispatch();
    if (m_watermarkTicks == 0 || totalTimeInDispatch == 0)
    {
        return;
    }

    if (totalTimeInDispatch >= m_watermarkTicks)
    {
        m_iterationsOverTimeCount++;
    }

    LARGE_INTEGER currentSystemTimeTicks;
    KeQueryTickCount(&currentSystemTimeTicks);

    if (currentSystemTimeTicks.QuadPart > m_nextReportingDeadlineTicks.QuadPart)
    {
        if (m_iterationsOverTimeCount != 0)
        {
            ULONG timeSinceLastReportMs = 0;
            if (m_nextReportingDeadlineTicks.QuadPart != 0)
            {
                auto overtimeTicks = static_cast<ULONG>(currentSystemTimeTicks.QuadPart - m_nextReportingDeadlineTicks.QuadPart);
                timeSinceLastReportMs = overtimeTicks * KeQueryTimeIncrement() / MS_TO_TIME_INCREMENT_MULT;
                timeSinceLastReportMs += m_dispatchTimeWarningIntervalMs;
            }

            ReportWarning(timeSinceLastReportMs);

            m_nextReportingDeadlineTicks.QuadPart = currentSystemTimeTicks.QuadPart + m_reportIntervalTicks;
            m_iterationsOverTimeCount = 0;
        }
    }
}

NONPAGED
void TimeBudgetWarningLogger::ReportWarning(ULONG TimeSinceLastReportMs)
{
    EventWriteExecutionTimeExceededWarning(
        &m_clientIdentifier,
        m_friendlyEcName->Buffer,
        m_watermarkMs,
        m_iterationsOverTimeCount,
        TimeSinceLastReportMs
    );
}

PAGED
void TimeBudgetWarningLogger::SetFriendlyName(
    UNICODE_STRING const * FriendlyName
)
{
    m_friendlyEcName = FriendlyName;
}
