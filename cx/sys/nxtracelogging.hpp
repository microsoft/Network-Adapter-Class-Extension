// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include <TraceLoggingProvider.h>
#include <telemetry\MicrosoftTelemetry.h> 

//
// ETW/TraceLogging related information
//
TRACELOGGING_DECLARE_PROVIDER(g_hNetAdapterCxEtwProvider);

// ETW Keywords, each value is a bit
#define NET_ADAPTER_CX_NX_DEVICE 0x1
#define NET_ADAPTER_CX_NX_ADAPTER 0x2
#define NET_ADAPTER_CX_STATE_TRANSITION 0x4
#define NET_ADAPTER_CX_OID 0x8
#define NET_ADAPTER_CX_NXDEVICE_PNP_STATE_TRANSITION (NET_ADAPTER_CX_NX_DEVICE | NET_ADAPTER_CX_STATE_TRANSITION)
#define NET_ADAPTER_CX_NXADAPTER_STATE_TRANSITION (NET_ADAPTER_CX_NX_ADAPTER | NET_ADAPTER_CX_STATE_TRANSITION)

// Provider GUID: {828a1d06-71b8-5fc5-b237-28ae6aeac7c4}
DEFINE_GUID(GUID_NETADAPTERCX_ETW_PROVIDER,
    0x828a1d06, 0x71b8, 0x5fc5, 0xb2, 0x37, 0x28, 0xae, 0x6a, 0xea, 0xc7, 0xc4);
