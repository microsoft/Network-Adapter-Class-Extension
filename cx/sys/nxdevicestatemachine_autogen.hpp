
/*++
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!    DO NOT MODIFY THIS FILE MANUALLY     !!
 !!    This file is created by              !!
 !!    StateMachineConverter.ps1            !!
 !!    in this directory.                   !!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

NxDeviceStateMachine_AutoGen.hpp

Abstract:

     This header file contains State Machines for NxDevicePnPPwr
     This has been generated automatically from a visio file.

--*/


#pragma once

//
// Index of States in all the State Machines
//
typedef enum _NxDeviceSM_STATE_INDEX {
    NxDevicePnPPwrStateDeviceAddedIndex,
    NxDevicePnPPwrStateDeviceInitializedIndex,
    NxDevicePnPPwrStateDeviceReleasedIndex,
    NxDevicePnPPwrStateDeviceReleasingIsSurpriseRemovedIndex,
    NxDevicePnPPwrStateDeviceReleasingReleaseClientIndex,
    NxDevicePnPPwrStateDeviceReleasingReportPreReleaseToNdisIndex,
    NxDevicePnPPwrStateDeviceReleasingReportSurpriseRemoveToNdisIndex,
    NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedReportPreReleaseToNdisIndex,
    NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedWaitForNdisHaltIndex,
    NxDevicePnPPwrStateDeviceReleasingWaitForNdisHaltIndex,
    NxDevicePnPPwrStateDeviceRemovedIndex,
    NxDevicePnPPwrStateDeviceRemovedReportRemoveToNdisIndex,
    NxDevicePnPPwrStateDeviceStartedIndex,
    NxDevicePnPPwrStateDeviceStartFailedWaitForReleaseHardwareIndex,
    NxDevicePnPPwrStateDeviceStartingReportStartToNdisIndex,
    NxDevicePnPPwrStateDeviceStartingWaitForNdisInitializeIndex,
    NxDevicePnPPwrStateDeviceStoppedPrepareForStartIndex,
    NxDevicePnPPwrStateNullIndex
} NxDeviceSM_STATE_INDEX;
//
// Enum of States in all the State Machines
//
#define NxDeviceSM_STATE\
    NxDevicePnPPwrStateDeviceAdded=0x57a73043,\
    NxDevicePnPPwrStateDeviceInitialized=0x57a73048,\
    NxDevicePnPPwrStateDeviceReleased=0x57a7304a,\
    NxDevicePnPPwrStateDeviceReleasingIsSurpriseRemoved=0x57a7304b,\
    NxDevicePnPPwrStateDeviceReleasingReleaseClient=0x57a7304c,\
    NxDevicePnPPwrStateDeviceReleasingReportPreReleaseToNdis=0x57a7304d,\
    NxDevicePnPPwrStateDeviceReleasingReportSurpriseRemoveToNdis=0x57a7304e,\
    NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedReportPreReleaseToNdis=0x57a7304f,\
    NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedWaitForNdisHalt=0x57a73051,\
    NxDevicePnPPwrStateDeviceReleasingWaitForNdisHalt=0x57a73050,\
    NxDevicePnPPwrStateDeviceRemoved=0x57a73045,\
    NxDevicePnPPwrStateDeviceRemovedReportRemoveToNdis=0x57a73046,\
    NxDevicePnPPwrStateDeviceStarted=0x57a73032,\
    NxDevicePnPPwrStateDeviceStartFailedWaitForReleaseHardware=0x57a73037,\
    NxDevicePnPPwrStateDeviceStartingReportStartToNdis=0x57a73033,\
    NxDevicePnPPwrStateDeviceStartingWaitForNdisInitialize=0x57a73042,\
    NxDevicePnPPwrStateDeviceStoppedPrepareForStart=0x57a73049,\
    NxDevicePnPPwrStateNull,\



//
// Event List For the State Machines
//
#define NxDeviceSM_EVENT\
    NxDevicePnPPwrEventAdapterAdded                         = 0xebe33170 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventAdapterHaltComplete                  = 0xebe33180 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventAdapterInitializeComplete            = 0xebe33190 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventAdapterInitializeFailed              = 0xebe331a0 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventCxPostPrepareHardware                = 0xebe331b0 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventCxPostReleaseHardware                = 0xebe331c0 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventCxPrePrepareHardware                 = 0xebe331d0 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventCxPrePrepareHardwareFailedCleanup    = 0xebe331f0 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventCxPreReleaseHardware                 = 0xebe331e0 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventNo                                   = 0xebe33120 | SmEngineEventPrioritySync,\
    NxDevicePnPPwrEventSyncFail                             = 0xebe330c0 | SmEngineEventPrioritySync,\
    NxDevicePnPPwrEventSyncPending                          = 0xebe330d0 | SmEngineEventPrioritySync,\
    NxDevicePnPPwrEventSyncSuccess                          = 0xebe330e0 | SmEngineEventPrioritySync,\
    NxDevicePnPPwrEventWdfDeviceObjectCleanup               = 0xebe33150 | SmEngineEventPriorityNormal,\
    NxDevicePnPPwrEventYes                                  = 0xebe33130 | SmEngineEventPrioritySync,\


typedef struct _SM_ENGINE_STATE_TABLE_ENTRY *PSM_ENGINE_STATE_TABLE_ENTRY;
extern PSM_ENGINE_STATE_TABLE_ENTRY NxDevicePnPPwrStateTable[];
//
// Operations
//
//
// Place this macro in the class declaration
//
#define GENERATED_DECLARATIONS_FOR_NXDEVICE_STATE_ENTRY_FUNCTIONS()  \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxDevice::_StateEntryFn_ReleasingIsSurpriseRemoved( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxDevice::_StateEntryFn_ReleasingReleaseClient( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxDevice::_StateEntryFn_ReleasingReportPostReleaseToNdis( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxDevice::_StateEntryFn_ReleasingReportPreReleaseToNdis( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxDevice::_StateEntryFn_ReleasingReportSurpriseRemoveToNdis( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxDevice::_StateEntryFn_RemovedReportRemoveToNdis( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxDevice::_StateEntryFn_RemovingReportRemoveToNdis( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxDevice::_StateEntryFn_StartingReportStartToNdis( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxDevice::_StateEntryFn_StartProcessingComplete( \
    _In_ PNxDevice  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxDevice::_StateEntryFn_StoppedPrepareForStart( \
    _In_ PNxDevice  Context \
); \


