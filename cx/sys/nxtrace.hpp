/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    nxtrace.h

Abstract:

    This module contains the private tracing definitions for the 
    Nids class extension

Environment:

    kernel mode only

Revision History:


--*/

#pragma once

extern "C" {
#include <WppRecorder.h>
}

//
// Tracing Definitions:
//
// Control GUID: 
// {16d3161e-4883-493f-acd9-8e2065b82270}
// 

#define WPP_CONTROL_GUIDS                           \
    WPP_DEFINE_CONTROL_GUID(                        \
        NxTraceGuid,                                \
        (16d3161e,4883,493f,acd9,8e2065b82270),     \
        WPP_DEFINE_BIT(FLAG_DRIVER)                 \
        WPP_DEFINE_BIT(FLAG_ADAPTER)                \
        WPP_DEFINE_BIT(FLAG_CONFIGURATION)          \
        WPP_DEFINE_BIT(FLAG_UTILITY)                \
        WPP_DEFINE_BIT(FLAG_REQUEST)                \
        WPP_DEFINE_BIT(FLAG_REQUEST_QUEUE)          \
        WPP_DEFINE_BIT(FLAG_NET_OBJECT)             \
        WPP_DEFINE_BIT(FLAG_DEVICE)                 \
        WPP_DEFINE_BIT(FLAG_DATA)                   \
        WPP_DEFINE_BIT(FLAG_VERIFIER)               \
        WPP_DEFINE_BIT(FLAG_TRANSLATOR)             \
        WPP_DEFINE_BIT(FLAG_POWER)                  \
        WPP_DEFINE_BIT(FLAG_ERROR)                  \
        WPP_DEFINE_BIT(FLAG_NET_QUEUE)              \
        )

#define MACRO_START do {
#define MACRO_END } while(0)

#define WPP_RECORDER_LEVEL_FLAGS_ARGS(level, flags) \
    WPP_CONTROL(WPP_BIT_ ## flags).AutoLogContext, 0, WPP_BIT_ ## flags
#define WPP_RECORDER_LEVEL_FLAGS_FILTER(level, flags) \
    (level < TRACE_LEVEL_INFORMATION || WPP_CONTROL(WPP_BIT_ ## flags).AutoLogVerboseEnabled)





//
// Enums for pretty-printing in WPP.
//

// begin_wpp config
// CUSTOM_TYPE(NDIS_DEVICE_PNP_EVENT, ItemEnum(_NDIS_DEVICE_PNP_EVENT));
// CUSTOM_TYPE(NDIS_PARAMETER_TYPE, ItemEnum(_NDIS_PARAMETER_TYPE));
// CUSTOM_TYPE(NDIS_REQUEST_TYPE, ItemEnum(_NDIS_REQUEST_TYPE));
// CUSTOM_TYPE(NDIS_OID, ItemListLong(0,1));
// end_wpp

// begin_wpp config
// USEPREFIX(LogError,     "%!STDPREFIX! !! NetAdapterCx - %!FUNC!: ");
// USEPREFIX(LogWarning,   "%!STDPREFIX!  ! NetAdapterCx - %!FUNC!: ");
// USEPREFIX(LogInfo,      "%!STDPREFIX!    NetAdapterCx - %!FUNC!: ");
// USEPREFIX(LogVerbose,   "%!STDPREFIX!  . NetAdapterCx - %!FUNC!: ");
// USEPREFIX(FuncEntry,    "%!STDPREFIX! >> NetAdapterCx - %!FUNC!: entry");
// USEPREFIX(FuncExit,     "%!STDPREFIX! << NetAdapterCx - %!FUNC!: exit");
// end_wpp

// begin_wpp config
// FUNC FuncEntry{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC FuncExit{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// end_wpp

// begin_wpp config
// LogVerbose{LEVEL=TRACE_LEVEL_VERBOSE}(IFRLOG,FLAGS,MSG,...);
// end_wpp

// begin_wpp config
// LogError{LEVEL=TRACE_LEVEL_ERROR}(IFRLOG,FLAGS,MSG,...);
// end_wpp

// begin_wpp config
// LogWarning{LEVEL=TRACE_LEVEL_WARNING}(IFRLOG,FLAGS,MSG,...);
// end_wpp

// begin_wpp config
// LogInfo{LEVEL=TRACE_LEVEL_INFORMATION}(IFRLOG,FLAGS,MSG,...);
// end_wpp

//
// WPP Macros: LogInitMsg
//
// begin_wpp config
// FUNC LogInitMsg{COMPNAME=FLAG_DRIVER}(LEVEL,MSG,...);
// USEPREFIX (LogInitMsg, "%!STDPREFIX! NetAdapterCx Init - %!FUNC!: ");
// end_wpp

#define WPP_COMPNAME_LEVEL_PRE(comp,level) MACRO_START
#define WPP_COMPNAME_LEVEL_POST(comp,level) ; MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_FILTER(comp,level) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)  
#define WPP_RECORDER_COMPNAME_LEVEL_ARGS(comp,level) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_RETURN_IF_NOT_NT_SUCCESS_MSG
//
// begin_wpp config
// FUNC CX_RETURN_IF_NOT_NT_SUCCESS_MSG{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTEXPR,MSG,...);
// USEPREFIX (CX_RETURN_IF_NOT_NT_SUCCESS_MSG, "%!STDPREFIX! !! NetAdapterCx - %!FUNC!: ");
// USESUFFIX (CX_RETURN_IF_NOT_NT_SUCCESS_MSG, " [status=%!STATUS!]", nt__wpp);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTEXPR_PRE(comp,level,ntexpr) MACRO_START NTSTATUS nt__wpp = (ntexpr); if (!NT_SUCCESS(nt__wpp)) {
#define WPP_COMPNAME_LEVEL_NTEXPR_POST(comp,level,ntexpr) ; return nt__wpp; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR_FILTER(comp,level,ntexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)  
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR_ARGS(comp,level,ntexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_LOG_IF_NOT_NT_SUCCESS_MSG
//
// begin_wpp config
// FUNC CX_LOG_IF_NOT_NT_SUCCESS_MSG{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_WARNING}(NTEXPR2,MSG,...);
// USEPREFIX (CX_LOG_IF_NOT_NT_SUCCESS_MSG, "%!STDPREFIX!  ! NetAdapterCx - %ws: ", nt__function);
// USESUFFIX (CX_LOG_IF_NOT_NT_SUCCESS_MSG, " [status=%!STATUS!]", nt__wpp);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTEXPR2_PRE(comp,level,ntexpr) [&](PCWSTR nt__function) { NTSTATUS nt__wpp = (ntexpr); if (!NT_SUCCESS(nt__wpp)) {
#define WPP_COMPNAME_LEVEL_NTEXPR2_POST(comp,level,ntexpr) ; } return nt__wpp; }(__FUNCTIONW__)
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR2_FILTER(comp,level,ntexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)  
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR2_ARGS(comp,level,ntexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_RETURN_NTSTATUS_IF
//
// begin_wpp config
// FUNC CX_RETURN_NTSTATUS_IF{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTCODE,CNDTN);
// USEPREFIX (CX_RETURN_NTSTATUS_IF, "%!STDPREFIX! !! NetAdapterCx - %!FUNC!: ");
// USESUFFIX (CX_RETURN_NTSTATUS_IF, "ERROR: Returning %!s!. (%!s! is true).", #NTCODE, #CNDTN);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTCODE_CNDTN_PRE(comp,level,status,condition) MACRO_START if ((condition)) {
#define WPP_COMPNAME_LEVEL_NTCODE_CNDTN_POST(comp,level,status,condition) ; return status; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTCODE_CNDTN_FILTER(comp,level,status,condition) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)  
#define WPP_RECORDER_COMPNAME_LEVEL_NTCODE_CNDTN_ARGS(comp,level,status,condition) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// This can be made reg. configurable.
//
#define DEFAULT_WPP_TOTAL_BUFFER_SIZE (PAGE_SIZE)
#define DEFAULT_WPP_ERROR_PARTITION_SIZE (DEFAULT_WPP_TOTAL_BUFFER_SIZE/2)
