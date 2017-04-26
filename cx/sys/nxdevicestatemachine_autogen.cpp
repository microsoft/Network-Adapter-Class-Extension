
/*++
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!    DO NOT MODIFY THIS FILE MANUALLY     !!
 !!    This file is created by              !!
 !!    StateMachineConverter.ps1            !!
 !!    in this directory.                   !!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

NxDeviceStateMachine_AutoGen.cpp

Abstract:

     This header file contains State Machines for NxDevicePnPPwr
     This has been generated automatically from a visio file.

--*/


#include "Nx.hpp"
#include "NxDeviceStateMachine_AutoGen.tmh"
//
// Auto Event Handlers
//
SM_ENGINE_AUTO_EVENT_HANDLER    NxDevicePnPPwrEventHandler_Ignore ;
SM_ENGINE_AUTO_EVENT_HANDLER    NxDevicePnPPwrEventHandler_PrePrepareHardware ;





//
// Definitions for State Entry Functions 
//
SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceAdded;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceInitialized;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceReleased;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceReleasingIsSurpriseRemoved;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceReleasingReleaseClient;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceReleasingReportPreReleaseToNdis;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceReleasingReportSurpriseRemoveToNdis;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceReleasingSurpriseRemovedWaitForNdisHalt;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceReleasingWaitForNdisHalt;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceRemoved;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceRemovedReportRemoveToNdis;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceStarted;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceStartFailedWaitForReleaseHardware;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceStartingReportStartToNdis;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceStartingWaitForNdisInitialize;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxDevicePnPPwrStateEntryFunc_DeviceStoppedPrepareForStart;




//
// State Entries for the states in the State Machine
//
SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceAdded[] = {
    { NxDevicePnPPwrEventCxPrePrepareHardware ,NxDevicePnPPwrEventHandler_PrePrepareHardware },
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceAdded[] = {
    { NxDevicePnPPwrEventCxPostPrepareHardware ,NxDevicePnPPwrStateDeviceStartingReportStartToNdisIndex },
    { NxDevicePnPPwrEventCxPreReleaseHardware ,NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedReportPreReleaseToNdisIndex },
    { NxDevicePnPPwrEventWdfDeviceObjectCleanup ,NxDevicePnPPwrStateDeviceRemovedReportRemoveToNdisIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceAdded = {
    // State Enum
    NxDevicePnPPwrStateDeviceAdded,
    // State Entry Function
    NULL,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesLowPriEvents|SmEngineStateFlagNoStateEntryFunction),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceAdded,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceAdded
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceInitialized[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceInitialized[] = {
    { NxDevicePnPPwrEventAdapterAdded ,NxDevicePnPPwrStateDeviceAddedIndex },
    { NxDevicePnPPwrEventWdfDeviceObjectCleanup ,NxDevicePnPPwrStateDeviceRemovedIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceInitialized = {
    // State Enum
    NxDevicePnPPwrStateDeviceInitialized,
    // State Entry Function
    NULL,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesLowPriEvents|SmEngineStateFlagNoStateEntryFunction),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceInitialized,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceInitialized
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceReleased[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceReleased[] = {
    { NxDevicePnPPwrEventCxPrePrepareHardware ,NxDevicePnPPwrStateDeviceStoppedPrepareForStartIndex },
    { NxDevicePnPPwrEventWdfDeviceObjectCleanup ,NxDevicePnPPwrStateDeviceRemovedIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceReleased = {
    // State Enum
    NxDevicePnPPwrStateDeviceReleased,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceReleased,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesLowPriEvents),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceReleased,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceReleased
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceReleasingIsSurpriseRemoved[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceReleasingIsSurpriseRemoved[] = {
    { NxDevicePnPPwrEventNo ,       NxDevicePnPPwrStateDeviceReleasingReportPreReleaseToNdisIndex },
    { NxDevicePnPPwrEventYes ,      NxDevicePnPPwrStateDeviceReleasingReportSurpriseRemoveToNdisIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceReleasingIsSurpriseRemoved = {
    // State Enum
    NxDevicePnPPwrStateDeviceReleasingIsSurpriseRemoved,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceReleasingIsSurpriseRemoved,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceReleasingIsSurpriseRemoved,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceReleasingIsSurpriseRemoved
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceReleasingReleaseClient[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceReleasingReleaseClient[] = {
    { NxDevicePnPPwrEventCxPostReleaseHardware ,NxDevicePnPPwrStateDeviceReleasedIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceReleasingReleaseClient = {
    // State Enum
    NxDevicePnPPwrStateDeviceReleasingReleaseClient,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceReleasingReleaseClient,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceReleasingReleaseClient,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceReleasingReleaseClient
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceReleasingReportPreReleaseToNdis[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceReleasingReportPreReleaseToNdis[] = {
    { NxDevicePnPPwrEventSyncFail , NxDevicePnPPwrStateDeviceReleasingReleaseClientIndex },
    { NxDevicePnPPwrEventSyncSuccess ,NxDevicePnPPwrStateDeviceReleasingWaitForNdisHaltIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceReleasingReportPreReleaseToNdis = {
    // State Enum
    NxDevicePnPPwrStateDeviceReleasingReportPreReleaseToNdis,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceReleasingReportPreReleaseToNdis,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceReleasingReportPreReleaseToNdis,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceReleasingReportPreReleaseToNdis
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceReleasingReportSurpriseRemoveToNdis[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceReleasingReportSurpriseRemoveToNdis[] = {
    { NxDevicePnPPwrEventSyncFail , NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedReportPreReleaseToNdisIndex },
    { NxDevicePnPPwrEventSyncSuccess ,NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedWaitForNdisHaltIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceReleasingReportSurpriseRemoveToNdis = {
    // State Enum
    NxDevicePnPPwrStateDeviceReleasingReportSurpriseRemoveToNdis,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceReleasingReportSurpriseRemoveToNdis,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceReleasingReportSurpriseRemoveToNdis,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceReleasingReportSurpriseRemoveToNdis
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis[] = {
    { NxDevicePnPPwrEventSyncFail , NxDevicePnPPwrStateDeviceReleasingReleaseClientIndex },
    { NxDevicePnPPwrEventSyncSuccess ,NxDevicePnPPwrStateDeviceReleasingReleaseClientIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceReleasingSurpriseRemovedReportPreReleaseToNdis = {
    // State Enum
    NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedReportPreReleaseToNdis,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagRequiresPassive),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceReleasingSurpriseRemovedWaitForNdisHalt[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceReleasingSurpriseRemovedWaitForNdisHalt[] = {
    { NxDevicePnPPwrEventAdapterHaltComplete ,NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedReportPreReleaseToNdisIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceReleasingSurpriseRemovedWaitForNdisHalt = {
    // State Enum
    NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedWaitForNdisHalt,
    // State Entry Function
    NULL,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesLowPriEvents|SmEngineStateFlagNoStateEntryFunction),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceReleasingSurpriseRemovedWaitForNdisHalt,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceReleasingSurpriseRemovedWaitForNdisHalt
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceReleasingWaitForNdisHalt[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceReleasingWaitForNdisHalt[] = {
    { NxDevicePnPPwrEventAdapterHaltComplete ,NxDevicePnPPwrStateDeviceReleasingReleaseClientIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceReleasingWaitForNdisHalt = {
    // State Enum
    NxDevicePnPPwrStateDeviceReleasingWaitForNdisHalt,
    // State Entry Function
    NULL,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesLowPriEvents|SmEngineStateFlagNoStateEntryFunction),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceReleasingWaitForNdisHalt,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceReleasingWaitForNdisHalt
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceRemoved[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceRemoved[] = {
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceRemoved = {
    // State Enum
    NxDevicePnPPwrStateDeviceRemoved,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceRemoved,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceRemoved,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceRemoved
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceRemovedReportRemoveToNdis[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceRemovedReportRemoveToNdis[] = {
    { NxDevicePnPPwrEventSyncSuccess ,NxDevicePnPPwrStateDeviceRemovedIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceRemovedReportRemoveToNdis = {
    // State Enum
    NxDevicePnPPwrStateDeviceRemovedReportRemoveToNdis,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceRemovedReportRemoveToNdis,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceRemovedReportRemoveToNdis,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceRemovedReportRemoveToNdis
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceStarted[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceStarted[] = {
    { NxDevicePnPPwrEventCxPreReleaseHardware ,NxDevicePnPPwrStateDeviceReleasingIsSurpriseRemovedIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceStarted = {
    // State Enum
    NxDevicePnPPwrStateDeviceStarted,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceStarted,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesLowPriEvents),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceStarted,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceStarted
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceStartFailedWaitForReleaseHardware[] = {
    { NxDevicePnPPwrEventAdapterInitializeFailed ,NxDevicePnPPwrEventHandler_Ignore },
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceStartFailedWaitForReleaseHardware[] = {
    { NxDevicePnPPwrEventCxPreReleaseHardware ,NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedReportPreReleaseToNdisIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceStartFailedWaitForReleaseHardware = {
    // State Enum
    NxDevicePnPPwrStateDeviceStartFailedWaitForReleaseHardware,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceStartFailedWaitForReleaseHardware,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesLowPriEvents),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceStartFailedWaitForReleaseHardware,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceStartFailedWaitForReleaseHardware
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceStartingReportStartToNdis[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceStartingReportStartToNdis[] = {
    { NxDevicePnPPwrEventSyncFail , NxDevicePnPPwrStateDeviceStartFailedWaitForReleaseHardwareIndex },
    { NxDevicePnPPwrEventSyncSuccess ,NxDevicePnPPwrStateDeviceStartingWaitForNdisInitializeIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceStartingReportStartToNdis = {
    // State Enum
    NxDevicePnPPwrStateDeviceStartingReportStartToNdis,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceStartingReportStartToNdis,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceStartingReportStartToNdis,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceStartingReportStartToNdis
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceStartingWaitForNdisInitialize[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceStartingWaitForNdisInitialize[] = {
    { NxDevicePnPPwrEventAdapterInitializeComplete ,NxDevicePnPPwrStateDeviceStartedIndex },
    { NxDevicePnPPwrEventAdapterInitializeFailed ,NxDevicePnPPwrStateDeviceStartFailedWaitForReleaseHardwareIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceStartingWaitForNdisInitialize = {
    // State Enum
    NxDevicePnPPwrStateDeviceStartingWaitForNdisInitialize,
    // State Entry Function
    NULL,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesLowPriEvents|SmEngineStateFlagNoStateEntryFunction),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceStartingWaitForNdisInitialize,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceStartingWaitForNdisInitialize
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxDevicePnPPwrAutoEventTransitions_DeviceStoppedPrepareForStart[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxDevicePnPPwrStateTransitions_DeviceStoppedPrepareForStart[] = {
    { NxDevicePnPPwrEventCxPostPrepareHardware ,NxDevicePnPPwrStateDeviceStartingReportStartToNdisIndex },
    { NxDevicePnPPwrEventCxPrePrepareHardwareFailedCleanup ,NxDevicePnPPwrStateDeviceReleasingSurpriseRemovedReportPreReleaseToNdisIndex },
    { NxDevicePnPPwrEventCxPreReleaseHardware ,NxDevicePnPPwrStateDeviceStartFailedWaitForReleaseHardwareIndex },
    { SmEngineEventNull ,           NxDevicePnPPwrStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxDevicePnPPwrStateTableEntryDeviceStoppedPrepareForStart = {
    // State Enum
    NxDevicePnPPwrStateDeviceStoppedPrepareForStart,
    // State Entry Function
    NxDevicePnPPwrStateEntryFunc_DeviceStoppedPrepareForStart,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxDevicePnPPwrAutoEventTransitions_DeviceStoppedPrepareForStart,
    // Event State Pairs for Transitions
    NxDevicePnPPwrStateTransitions_DeviceStoppedPrepareForStart
};










//
// Global List of States in all the State Machines
//
PSM_ENGINE_STATE_TABLE_ENTRY    NxDevicePnPPwrStateTable[] =
{
    &NxDevicePnPPwrStateTableEntryDeviceAdded,
    &NxDevicePnPPwrStateTableEntryDeviceInitialized,
    &NxDevicePnPPwrStateTableEntryDeviceReleased,
    &NxDevicePnPPwrStateTableEntryDeviceReleasingIsSurpriseRemoved,
    &NxDevicePnPPwrStateTableEntryDeviceReleasingReleaseClient,
    &NxDevicePnPPwrStateTableEntryDeviceReleasingReportPreReleaseToNdis,
    &NxDevicePnPPwrStateTableEntryDeviceReleasingReportSurpriseRemoveToNdis,
    &NxDevicePnPPwrStateTableEntryDeviceReleasingSurpriseRemovedReportPreReleaseToNdis,
    &NxDevicePnPPwrStateTableEntryDeviceReleasingSurpriseRemovedWaitForNdisHalt,
    &NxDevicePnPPwrStateTableEntryDeviceReleasingWaitForNdisHalt,
    &NxDevicePnPPwrStateTableEntryDeviceRemoved,
    &NxDevicePnPPwrStateTableEntryDeviceRemovedReportRemoveToNdis,
    &NxDevicePnPPwrStateTableEntryDeviceStarted,
    &NxDevicePnPPwrStateTableEntryDeviceStartFailedWaitForReleaseHardware,
    &NxDevicePnPPwrStateTableEntryDeviceStartingReportStartToNdis,
    &NxDevicePnPPwrStateTableEntryDeviceStartingWaitForNdisInitialize,
    &NxDevicePnPPwrStateTableEntryDeviceStoppedPrepareForStart,
};




SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceReleased(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceReleased is called when the
    state machine enters the DeviceReleased State

--*/
{

    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    NxDevice::_StateEntryFn_ReleasingReportPostReleaseToNdis(context);

    return SmEngineEventNull;

} // NxDevicePnPPwrStateEntryFunc_DeviceReleased


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceReleasingIsSurpriseRemoved(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceReleasingIsSurpriseRemoved is called when the
    state machine enters the DeviceReleasingIsSurpriseRemoved State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxDevice::_StateEntryFn_ReleasingIsSurpriseRemoved(context);

    return syncEvent;

} // NxDevicePnPPwrStateEntryFunc_DeviceReleasingIsSurpriseRemoved


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceReleasingReleaseClient(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceReleasingReleaseClient is called when the
    state machine enters the DeviceReleasingReleaseClient State

--*/
{

    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    NxDevice::_StateEntryFn_ReleasingReleaseClient(context);

    return SmEngineEventNull;

} // NxDevicePnPPwrStateEntryFunc_DeviceReleasingReleaseClient


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceReleasingReportPreReleaseToNdis(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceReleasingReportPreReleaseToNdis is called when the
    state machine enters the DeviceReleasingReportPreReleaseToNdis State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxDevice::_StateEntryFn_ReleasingReportPreReleaseToNdis(context);

    return syncEvent;

} // NxDevicePnPPwrStateEntryFunc_DeviceReleasingReportPreReleaseToNdis


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceReleasingReportSurpriseRemoveToNdis(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceReleasingReportSurpriseRemoveToNdis is called when the
    state machine enters the DeviceReleasingReportSurpriseRemoveToNdis State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxDevice::_StateEntryFn_ReleasingReportSurpriseRemoveToNdis(context);

    return syncEvent;

} // NxDevicePnPPwrStateEntryFunc_DeviceReleasingReportSurpriseRemoveToNdis


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis is called when the
    state machine enters the DeviceReleasingSurpriseRemovedReportPreReleaseToNdis State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxDevice::_StateEntryFn_ReleasingReportPreReleaseToNdis(context);

    return syncEvent;

} // NxDevicePnPPwrStateEntryFunc_DeviceReleasingSurpriseRemovedReportPreReleaseToNdis


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceRemoved(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceRemoved is called when the
    state machine enters the DeviceRemoved State

--*/
{

    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    NxDevice::_StateEntryFn_RemovedReportRemoveToNdis(context);

    return SmEngineEventNull;

} // NxDevicePnPPwrStateEntryFunc_DeviceRemoved


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceRemovedReportRemoveToNdis(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceRemovedReportRemoveToNdis is called when the
    state machine enters the DeviceRemovedReportRemoveToNdis State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    NxDevice::_StateEntryFn_RemovingReportRemoveToNdis(context);

    syncEvent = NxDevicePnPPwrEventSyncSuccess;

    return syncEvent;

} // NxDevicePnPPwrStateEntryFunc_DeviceRemovedReportRemoveToNdis


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceStarted(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceStarted is called when the
    state machine enters the DeviceStarted State

--*/
{

    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    NxDevice::_StateEntryFn_StartProcessingComplete(context);

    return SmEngineEventNull;

} // NxDevicePnPPwrStateEntryFunc_DeviceStarted


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceStartFailedWaitForReleaseHardware(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceStartFailedWaitForReleaseHardware is called when the
    state machine enters the DeviceStartFailedWaitForReleaseHardware State

--*/
{

    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    NxDevice::_StateEntryFn_StartProcessingComplete(context);

    return SmEngineEventNull;

} // NxDevicePnPPwrStateEntryFunc_DeviceStartFailedWaitForReleaseHardware


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceStartingReportStartToNdis(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceStartingReportStartToNdis is called when the
    state machine enters the DeviceStartingReportStartToNdis State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxDevice::_StateEntryFn_StartingReportStartToNdis(context);

    return syncEvent;

} // NxDevicePnPPwrStateEntryFunc_DeviceStartingReportStartToNdis


SM_ENGINE_EVENT
NxDevicePnPPwrStateEntryFunc_DeviceStoppedPrepareForStart(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxDevicePnPPwr StateEntryFunc_DeviceStoppedPrepareForStart is called when the
    state machine enters the DeviceStoppedPrepareForStart State

--*/
{

    PNxDevice  context;
    
    context = (PNxDevice) ContextAsPVOID;
    NxDevice::_StateEntryFn_StoppedPrepareForStart(context);

    return SmEngineEventNull;

} // NxDevicePnPPwrStateEntryFunc_DeviceStoppedPrepareForStart





