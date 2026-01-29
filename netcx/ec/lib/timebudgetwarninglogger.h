// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <ntdef.h>
#include <KMacros.h>
#include "TimeBudget.h"

class TimeBudgetWarningLogger
{
public:
    NONPAGED
    TimeBudgetWarningLogger(
        const GUID& ClientIdentifier,
        const ULONG DispatchTimeWarningInterval,
        ULONG DispatchIrqlWarningWatermarkMs,
        UNICODE_STRING const* FriendlyName
    );

    NONPAGED
    void LogWarningIfOvertime(
        _In_ const TimeBudget& timeBudget
    );

private:

    NONPAGED
    void ReportWarning(ULONG TimeSinceLastReportMs);

    UNICODE_STRING const * m_friendlyEcName {};
    const GUID m_clientIdentifier {};

    UINT32 m_iterationsOverTimeCount {0};
    ULONG m_watermarkMs {0};
    ULONG const m_dispatchTimeWarningIntervalMs {0};

    LARGE_INTEGER m_nextReportingDeadlineTicks {0};
    ULONG const m_reportIntervalTicks {0};
    ULONG m_watermarkTicks {0};
};
