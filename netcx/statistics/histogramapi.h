// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>
#include <NdisStatisticalIoctls.h>

namespace Statistics
{
    class Hub;
}

NONPAGED
NTSTATUS
CollectHistogramsForIoctl(
    const Statistics::Hub& hub,
    _Out_writes_bytes_(cbOut) NDIS_COLLECT_HISTOGRAM_OUT *out,
    _In_ ULONG cbOut,
    _Out_ ULONG &bytesNeeded
);

NONPAGED
void
ResetHistogramsValues(
    Statistics::Hub& hub
);
