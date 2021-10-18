// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

//
// Tracing Definitions:
//
// Control GUID:
// {94C9795A-DF87-4B51-8DC4-21E82539F158}
//

#define WPP_CONTROL_GUIDS                           \
    WPP_DEFINE_CONTROL_GUID(                        \
        ECTraceGuid,                                \
        (94c9795a,df87,4b51,8dc4,21e82539f158),     \
        WPP_DEFINE_BIT(FLAG_GENERAL)                \
        )

#define MACRO_START do {
#define MACRO_END } while(0)

#define WPP_RECORDER_LEVEL_FLAGS_ARGS(level, flags) \
    WPP_CONTROL(WPP_BIT_ ## flags).AutoLogContext, 0, WPP_BIT_ ## flags
#define WPP_RECORDER_LEVEL_FLAGS_FILTER(level, flags) \
    (level < TRACE_LEVEL_VERBOSE || WPP_CONTROL(WPP_BIT_ ## flags).AutoLogVerboseEnabled)

// WPP Macros: RETURN_IF_NOT_SUCCESS
//
// begin_wpp config
// FUNC RETURN_IF_NOT_SUCCESS{COMPNAME=FLAG_GENERAL,LEVEL=TRACE_LEVEL_ERROR}(NTEXPR);
// USESUFFIX (RETURN_IF_NOT_SUCCESS, " [status=%!STATUS!]", nt__wpp);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTEXPR_PRE(comp, level, ntexpr) MACRO_START NTSTATUS nt__wpp = (ntexpr); if (STATUS_SUCCESS != nt__wpp) {
#define WPP_COMPNAME_LEVEL_NTEXPR_POST(comp, level, ntexpr); NT_FRE_ASSERTMSG("Success code other than STATUS_SUCCESS will be ignored", NT_ERROR(nt__wpp)); return nt__wpp; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR_FILTER(comp, level, ntexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR_ARGS(comp, level, ntexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// This can be made reg. configurable.
//
#define DEFAULT_WPP_TOTAL_BUFFER_SIZE (PAGE_SIZE)
#define DEFAULT_WPP_ERROR_PARTITION_SIZE (DEFAULT_WPP_TOTAL_BUFFER_SIZE/2)
