// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>

namespace Statistics
{
    class Hub;
    class Histogram;
}

NONPAGED
Statistics::Hub&
GetGlobalEcStatistics();

PAGED
void
FreeGlobalEcStatistics();

PAGED
NTSTATUS
InitializeGlobalEcStatistic();
