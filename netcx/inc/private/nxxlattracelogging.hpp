// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <TraceLoggingProvider.h>
#include <telemetry\MicrosoftTelemetry.h>

//
// ETW/TraceLogging related information
//
TRACELOGGING_DECLARE_PROVIDER(g_hNetAdapterCxXlatProvider);

// Provider GUID: {32723328-9a9f-5889-a637-bcc3f77c1324}
DEFINE_GUID(GUID_NETADAPTERCXXLAT_ETW_PROVIDER,
    0x32723328,0x9a9f,0x5889,0xa6,0x37,0xbc,0xc3,0xf7,0x7c,0x13,0x24);
