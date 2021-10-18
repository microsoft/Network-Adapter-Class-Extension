// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include "HistogramHub.h"
#include "HistogramApi.h"

NONPAGEDX
_Use_decl_annotations_
NTSTATUS
CollectHistogramsForIoctl(
    const Statistics::Hub& hub,
    NDIS_COLLECT_HISTOGRAM_OUT *out,
    ULONG cbOut,
    ULONG &bytesNeeded
)
{
    return hub.Serialize(out, cbOut, bytesNeeded);
}

NONPAGED
void
ResetHistogramsValues(
    Statistics::Hub& hub
)
{
    hub.ResetHistogramsValues();
}
