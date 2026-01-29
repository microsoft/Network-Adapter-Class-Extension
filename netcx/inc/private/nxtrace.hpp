// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the private tracing definitions for the
    Nids class extension

--*/

#pragma once

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
        WPP_DEFINE_BIT(FLAG_OBJECT_CALLBACK)        \
        WPP_DEFINE_BIT(FLAG_GENERAL)                \
        )

#define MACRO_START do {
#define MACRO_END } while(0)

// Exclude verbose and configuration traces from IFR
#define WPP_RECORDER_LEVEL_FLAGS_ARGS(level, flags) \
    WPP_CONTROL(WPP_BIT_ ## flags).AutoLogContext, 0, WPP_BIT_ ## flags
#define WPP_RECORDER_LEVEL_FLAGS_FILTER(level, flags) \
    (level < TRACE_LEVEL_VERBOSE || WPP_CONTROL(WPP_BIT_ ## flags).AutoLogVerboseEnabled) && WPP_BIT_ ## flags != WPP_BIT_ ## FLAG_CONFIGURATION

//
// Enums for pretty-printing in WPP.
//
// begin_wpp config
// CUSTOM_TYPE(NDIS_DEVICE_PNP_EVENT, ItemEnum(_NDIS_DEVICE_PNP_EVENT));
// CUSTOM_TYPE(NDIS_PARAMETER_TYPE, ItemEnum(_NDIS_PARAMETER_TYPE));
// CUSTOM_TYPE(NDIS_REQUEST_TYPE, ItemEnum(_NDIS_REQUEST_TYPE));
// CUSTOM_TYPE(SMFXTRANSITIONTYPE, ItemEnum(SmFx::TransitionType));
// CUSTOM_TYPE(DEVICEPNPPOWERSTATE, ItemEnum(NxDeviceStateMachine<NxDevice>::StateId));
// CUSTOM_TYPE(DEVICEPNPPOWEREVENT, ItemEnum(NxDeviceStateMachine<NxDevice>::EventId));
// CUSTOM_TYPE(ADAPTERPNPPOWERSTATE, ItemEnum(AdapterPnpPowerStateMachine<AdapterPnpPower>::StateId));
// CUSTOM_TYPE(ADAPTERPNPPOWEREVENT, ItemEnum(AdapterPnpPowerStateMachine<AdapterPnpPower>::EventId));
// CUSTOM_TYPE(ADAPTERSTATE, ItemEnum(NxAdapterStateMachine<NxAdapter>::StateId));
// CUSTOM_TYPE(ADAPTEREVENT, ItemEnum(NxAdapterStateMachine<NxAdapter>::EventId));
// CUSTOM_TYPE(ALPCMESSAGE, ItemEnum(_NETCXRD_ALPC_MESSAGE_TYPE));
// CUSTOM_TYPE(QUEUETXDEMUXTYPE, ItemEnum(NET_CLIENT_QUEUE_TX_DEMUX_TYPE));
// CUSTOM_TYPE(NET_IF_MEDIA_CONNECT_STATE, ItemEnum(_NET_IF_MEDIA_CONNECT_STATE));
// CUSTOM_TYPE(NdisIdleCondition, ItemEnum(_NDIS_IDLE_CONDITION));
// CUSTOM_TYPE(RdAdapterIoctl, ItemEnum(_NETCXRD_RDADAPTER_IOCTL));
// CUSTOM_TYPE(RdConfigurationIoctl, ItemEnum(_NETCXRD_RDCONFIGURATION_IOCTL));
// CUSTOM_TYPE(RdDeviceIoctl, ItemEnum(_NETCXRD_RDDEVICE_IOCTL));
// CUSTOM_TYPE(RdResetInterfaceIoctl, ItemEnum(_NETCXRD_RDRESETINTERFACE_IOCTL));
// CUSTOM_TYPE(QUEUETYPE, ItemEnum(NetCx::Core::Queue::Type));
// CUSTOM_TYPE(NETQUEUETYPE, ItemEnum(_NET_PACKET_QUEUE_TYPE));
// end_wpp

// begin_wpp config
// LogVerbose{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS,MSG,...);
// end_wpp

// begin_wpp config
// LogWarning{LEVEL=TRACE_LEVEL_WARNING}(FLAGS,MSG,...);
// end_wpp

// begin_wpp config
// LogError{LEVEL=TRACE_LEVEL_ERROR}(FLAGS,MSG,...);
// end_wpp

// begin_wpp config
// LogInfo{LEVEL=TRACE_LEVEL_INFORMATION}(FLAGS,MSG,...);
// end_wpp

//
// CX_RETURN_MSG
//
// begin_wpp config
// FUNC CX_RETURN_MSG{RETURN_COMP=FLAG_GENERAL,LEVEL=TRACE_LEVEL_INFORMATION}(MSG,...);
// end_wpp
#define WPP_RETURN_COMP_LEVEL_POST(comp,level) ; return;
#define WPP_RECORDER_RETURN_COMP_LEVEL_FILTER(comp,level) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_RETURN_COMP_LEVEL_ARGS(comp,level) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// CX_RETURN_STATUS_SUCCESS_MSG
//
// begin_wpp config
// FUNC CX_RETURN_STATUS_SUCCESS_MSG{RETURN_STATUS_SUCCESS_COMP=FLAG_GENERAL,LEVEL=TRACE_LEVEL_INFORMATION}(MSG,...);
// end_wpp
#define WPP_RETURN_STATUS_SUCCESS_COMP_LEVEL_POST(comp,level) ; return STATUS_SUCCESS;
#define WPP_RECORDER_RETURN_STATUS_SUCCESS_COMP_LEVEL_FILTER(comp,level) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_RETURN_STATUS_SUCCESS_COMP_LEVEL_ARGS(comp,level) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_RETURN_IF_NOT_NT_SUCCESS_MSG
//
// begin_wpp config
// FUNC CX_RETURN_IF_NOT_NT_SUCCESS_MSG{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTEXPR,MSG,...);
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

// WPP Macros: CX_RETURN_IF_NOT_NT_SUCCESS
//
// begin_wpp config
// FUNC CX_RETURN_IF_NOT_NT_SUCCESS{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTEXPR3);
// USESUFFIX (CX_RETURN_IF_NOT_NT_SUCCESS, " [status=%!STATUS!]", nt__wpp);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTEXPR3_PRE(comp, level, ntexpr) MACRO_START NTSTATUS nt__wpp = (ntexpr); if (!NT_SUCCESS(nt__wpp)) {
#define WPP_COMPNAME_LEVEL_NTEXPR3_POST(comp, level, ntexpr); return nt__wpp; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR3_FILTER(comp, level, ntexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR3_ARGS(comp, level, ntexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

// WPP Macros: CX_RETURN_NDIS_STATUS_IF_NOT_STATUS_SUCCESS
//
// begin_wpp config
// FUNC CX_RETURN_NDIS_STATUS_IF_NOT_STATUS_SUCCESS{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTEXPR4);
// USESUFFIX (CX_RETURN_NDIS_STATUS_IF_NOT_STATUS_SUCCESS, " [status=%!STATUS!]", nt__wpp);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTEXPR4_PRE(comp, level, ntexpr) MACRO_START NTSTATUS nt__wpp = (ntexpr); if (STATUS_SUCCESS != (nt__wpp)) {
#define WPP_COMPNAME_LEVEL_NTEXPR4_POST(comp, level, ntexpr); return NdisConvertNtStatusToNdisStatus(nt__wpp); } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR4_FILTER(comp, level, ntexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR4_ARGS(comp, level, ntexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

// WPP Macros: CX_RETURN_NTSTATUS_IF_NOT_NDIS_STATUS_SUCCESS
//
// begin_wpp config
// FUNC CX_RETURN_NTSTATUS_IF_NOT_NDIS_STATUS_SUCCESS{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NDISEXPR);
// USESUFFIX (CX_RETURN_NTSTATUS_IF_NOT_NDIS_STATUS_SUCCESS, " [status=%!STATUS!]", NdisConvertNdisStatusToNtStatus(ndis__wpp));
// end_wpp

#define WPP_COMPNAME_LEVEL_NDISEXPR_PRE(comp, level, ndisexpr) MACRO_START NDIS_STATUS ndis__wpp = (ndisexpr); if (NDIS_STATUS_SUCCESS != (ndis__wpp)) {
#define WPP_COMPNAME_LEVEL_NDISEXPR_POST(comp, level, ndisexpr); return NdisConvertNdisStatusToNtStatus(ndis__wpp); } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NDISEXPR_FILTER(comp, level, ndisexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NDISEXPR_ARGS(comp, level, ndisexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_RETURN_NTSTATUS_IF
//
// begin_wpp config
// FUNC CX_RETURN_NTSTATUS_IF{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTCODE,CNDTN);
// USESUFFIX (CX_RETURN_NTSTATUS_IF, "ERROR: Returning %!s!. (%!s! is true).", #NTCODE, #CNDTN);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTCODE_CNDTN_PRE(comp,level,status,condition) MACRO_START if ((condition)) {
#define WPP_COMPNAME_LEVEL_NTCODE_CNDTN_POST(comp,level,status,condition) ; return status; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTCODE_CNDTN_FILTER(comp,level,status,condition) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTCODE_CNDTN_ARGS(comp,level,status,condition) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_RETURN_NTSTATUS_IF_MSG
//
// begin_wpp config
// FUNC CX_RETURN_NTSTATUS_IF_MSG{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTCODE1,CNDTN1,MSG,...);
// USESUFFIX (CX_RETURN_NTSTATUS_IF_MSG, " [status=%!s!], (%!s! is true).", #NTCODE1, #CNDTN1);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTCODE1_CNDTN1_PRE(comp,level,status,condition) MACRO_START if ((condition)) {
#define WPP_COMPNAME_LEVEL_NTCODE1_CNDTN1_POST(comp,level,status,condition) ; return status; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTCODE1_CNDTN1_FILTER(comp,level,status,condition) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTCODE1_CNDTN1_ARGS(comp,level,status,condition) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_WARNING_RETURN_NTSTATUS_IF_MSG
//
// begin_wpp config
// FUNC CX_WARNING_RETURN_NTSTATUS_IF_MSG{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_WARNING}(NTCODE2,CNDTN2,MSG,...);
// USESUFFIX (CX_WARNING_RETURN_NTSTATUS_IF_MSG, " [status=%!s!], (%!s! is true).", #NTCODE2, #CNDTN2);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTCODE2_CNDTN2_PRE(comp,level,status,condition) MACRO_START if ((condition)) {
#define WPP_COMPNAME_LEVEL_NTCODE2_CNDTN2_POST(comp,level,status,condition) ; return status; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTCODE2_CNDTN2_FILTER(comp,level,status,condition) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTCODE2_CNDTN2_ARGS(comp,level,status,condition) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

// WPP Macros: CX_RETURN_NULL_IF_NOT_NT_SUCCESS
//
// begin_wpp config
// FUNC CX_RETURN_NULL_IF_NOT_NT_SUCCESS{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTEXPR5);
// USESUFFIX (CX_RETURN_NULL_IF_NOT_NT_SUCCESS, " [status=%!STATUS!]", nt__wpp);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTEXPR5_PRE(comp, level, ntexpr) MACRO_START NTSTATUS nt__wpp = (ntexpr); if (!NT_SUCCESS(nt__wpp)) {
#define WPP_COMPNAME_LEVEL_NTEXPR5_POST(comp, level, ntexpr); return nullptr; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR5_FILTER(comp, level, ntexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR5_ARGS(comp, level, ntexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_RETURN_NULL_IF_NOT_NT_SUCCESS_MSG
//
// begin_wpp config
// FUNC CX_RETURN_NULL_IF_NOT_NT_SUCCESS_MSG{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_ERROR}(NTEXPR6,MSG,...);
// USESUFFIX (CX_RETURN_NULL_IF_NOT_NT_SUCCESS_MSG, " [status=%!STATUS!]", nt__wpp);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTEXPR6_PRE(comp,level,ntexpr) MACRO_START NTSTATUS nt__wpp = (ntexpr); if (!NT_SUCCESS(nt__wpp)) {
#define WPP_COMPNAME_LEVEL_NTEXPR6_POST(comp,level,ntexpr) ; return nullptr; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR6_FILTER(comp,level,ntexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR6_ARGS(comp,level,ntexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_RETURN_VOID_IF_NOT_NT_SUCCESS_MSG
//
// begin_wpp config
// FUNC CX_RETURN_VOID_IF_NOT_NT_SUCCESS_MSG{COMPNAME=FLAG_ERROR,LEVEL=TRACE_LEVEL_WARNING}(NTEXPR7,MSG,...);
// USESUFFIX (CX_RETURN_VOID_IF_NOT_NT_SUCCESS_MSG, " [status=%!STATUS!]", nt__wpp);
// end_wpp

#define WPP_COMPNAME_LEVEL_NTEXPR7_PRE(comp,level,ntexpr) MACRO_START NTSTATUS nt__wpp = (ntexpr); if (!NT_SUCCESS(nt__wpp)) {
#define WPP_COMPNAME_LEVEL_NTEXPR7_POST(comp,level,ntexpr) ; return; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR7_FILTER(comp,level,ntexpr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_NTEXPR7_ARGS(comp,level,ntexpr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// WPP Macros: CX_RETURN_IF_NULL
//
// begin_wpp config
// FUNC CX_RETURN_IF_NULL{COMPNAME=FLAG_GENERAL,LEVEL=TRACE_LEVEL_WARNING}(PTR1,...);
// end_wpp

#define WPP_COMPNAME_LEVEL_PTR1_PRE(comp,level,ptr) MACRO_START if (ptr == nullptr) {
#define WPP_COMPNAME_LEVEL_PTR1_POST(comp,level,ptr) ; return STATUS_INVALID_PARAMETER; } MACRO_END
#define WPP_RECORDER_COMPNAME_LEVEL_PTR1_FILTER(comp,level,ptr) WPP_RECORDER_LEVEL_FLAGS_FILTER(level, comp)
#define WPP_RECORDER_COMPNAME_LEVEL_PTR1_ARGS(comp,level,ptr) WPP_RECORDER_LEVEL_FLAGS_ARGS(level, comp)

//
// This can be made reg. configurable.
//
#define DEFAULT_WPP_TOTAL_BUFFER_SIZE (4 * PAGE_SIZE)
#define DEFAULT_WPP_ERROR_PARTITION_SIZE (DEFAULT_WPP_TOTAL_BUFFER_SIZE/2)
