
/*++
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!    DO NOT MODIFY THIS FILE MANUALLY     !!
 !!    This file is created by              !!
 !!    StateMachineConverter.ps1            !!
 !!    in this directory.                   !!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

NxAdapterStateMachine_AutoGen.cpp

Abstract:

     This header file contains State Machines for NxAdapter
     This has been generated automatically from a visio file.

--*/


#include "Nx.hpp"
#include "NxAdapterStateMachine_AutoGen.tmh"
//
// Auto Event Handlers
//
SM_ENGINE_AUTO_EVENT_HANDLER    NxAdapterEventHandler_Ignore ;





//
// Definitions for State Entry Functions 
//
SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterHalted;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterHalting;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterInitializedRestartDatapath;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterInitializedWaitForSelfManagedIoInit;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterInitializing;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterPaused;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterPausing;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterRestarting;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_AdapterRunning;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_ClientDataPathPauseComplete;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_ClientDataPathStartFailed;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_PauseClientDataPathForNdisPause;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_PauseClientDataPathForNdisPause2;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_PauseClientDataPathForPacketClientPauseWhilePaused;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_PauseClientDataPathForPacketClientPauseWhileRunning;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend2;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_ShouldClientDataPathStartForNdisRestart;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_ShouldClientDataPathStartForPacketClientStart;

SM_ENGINE_STATE_ENTRY_FUNCTION          NxAdapterStateEntryFunc_ShouldClientDataPathStartForSelfManagedIoRestart;




//
// State Entries for the states in the State Machine
//
SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterHalted[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterHalted[] = {
    { NxAdapterEventNdisMiniportInitializeEx ,NxAdapterStateAdapterInitializingIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterHalted = {
    // State Enum
    NxAdapterStateAdapterHalted,
    // State Entry Function
    NULL,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNoStateEntryFunction),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterHalted,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterHalted
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterHalting[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterHalting[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterHaltedIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterHalting = {
    // State Enum
    NxAdapterStateAdapterHalting,
    // State Entry Function
    NxAdapterStateEntryFunc_AdapterHalting,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterHalting,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterHalting
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterInitializedRestartDatapath[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterInitializedRestartDatapath[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausedIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterInitializedRestartDatapath = {
    // State Enum
    NxAdapterStateAdapterInitializedRestartDatapath,
    // State Entry Function
    NxAdapterStateEntryFunc_AdapterInitializedRestartDatapath,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterInitializedRestartDatapath,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterInitializedRestartDatapath
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterInitializedWaitForSelfManagedIoInit[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterInitializedWaitForSelfManagedIoInit[] = {
    { NxAdapterEventInitializeSelfManagedIO ,NxAdapterStateAdapterPausedIndex },
    { NxAdapterEventNdisMiniportHalt ,NxAdapterStateAdapterHaltingIndex },
    { NxAdapterEventRestartSelfManagedIO ,NxAdapterStateAdapterInitializedRestartDatapathIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterInitializedWaitForSelfManagedIoInit = {
    // State Enum
    NxAdapterStateAdapterInitializedWaitForSelfManagedIoInit,
    // State Entry Function
    NULL,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNoStateEntryFunction),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterInitializedWaitForSelfManagedIoInit,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterInitializedWaitForSelfManagedIoInit
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterInitializing[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterInitializing[] = {
    { NxAdapterEventSyncFail ,      NxAdapterStateAdapterHaltedIndex },
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterInitializedWaitForSelfManagedIoInitIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterInitializing = {
    // State Enum
    NxAdapterStateAdapterInitializing,
    // State Entry Function
    NxAdapterStateEntryFunc_AdapterInitializing,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterInitializing,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterInitializing
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterPaused[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterPaused[] = {
    { NxAdapterEventNdisMiniportHalt ,NxAdapterStateAdapterHaltingIndex },
    { NxAdapterEventNdisMiniportPause ,NxAdapterStatePauseClientDataPathForNdisPause2Index },
    { NxAdapterEventNdisMiniportRestart ,NxAdapterStateShouldClientDataPathStartForNdisRestartIndex },
    { NxAdapterEventPacketClientPause ,NxAdapterStatePauseClientDataPathForPacketClientPauseWhilePausedIndex },
    { NxAdapterEventPacketClientStart ,NxAdapterStateShouldClientDataPathStartForPacketClientStartIndex },
    { NxAdapterEventRestartSelfManagedIO ,NxAdapterStateShouldClientDataPathStartForSelfManagedIoRestartIndex },
    { NxAdapterEventSuspendSelfManagedIO ,NxAdapterStatePauseClientDataPathForSelfManagedIoSuspend2Index },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterPaused = {
    // State Enum
    NxAdapterStateAdapterPaused,
    // State Entry Function
    NxAdapterStateEntryFunc_AdapterPaused,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesDefinedEventsOnly),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterPaused,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterPaused
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterPausing[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterPausing[] = {
    { NxAdapterEventClientPauseComplete ,NxAdapterStateClientDataPathPauseCompleteIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterPausing = {
    // State Enum
    NxAdapterStateAdapterPausing,
    // State Entry Function
    NxAdapterStateEntryFunc_AdapterPausing,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterPausing,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterPausing
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterRestarting[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterRestarting[] = {
    { NxAdapterEventClientStartFailed ,NxAdapterStateClientDataPathStartFailedIndex },
    { NxAdapterEventClientStartSucceeded ,NxAdapterStateAdapterRunningIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterRestarting = {
    // State Enum
    NxAdapterStateAdapterRestarting,
    // State Entry Function
    NxAdapterStateEntryFunc_AdapterRestarting,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterRestarting,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterRestarting
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_AdapterRunning[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_AdapterRunning[] = {
    { NxAdapterEventNdisMiniportPause ,NxAdapterStatePauseClientDataPathForNdisPauseIndex },
    { NxAdapterEventPacketClientPause ,NxAdapterStatePauseClientDataPathForPacketClientPauseWhileRunningIndex },
    { NxAdapterEventSuspendSelfManagedIO ,NxAdapterStatePauseClientDataPathForSelfManagedIoSuspendIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryAdapterRunning = {
    // State Enum
    NxAdapterStateAdapterRunning,
    // State Entry Function
    NxAdapterStateEntryFunc_AdapterRunning,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagHandlesDefinedEventsOnly),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_AdapterRunning,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_AdapterRunning
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_ClientDataPathPauseComplete[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_ClientDataPathPauseComplete[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausedIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryClientDataPathPauseComplete = {
    // State Enum
    NxAdapterStateClientDataPathPauseComplete,
    // State Entry Function
    NxAdapterStateEntryFunc_ClientDataPathPauseComplete,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_ClientDataPathPauseComplete,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_ClientDataPathPauseComplete
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_ClientDataPathStartFailed[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_ClientDataPathStartFailed[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausedIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryClientDataPathStartFailed = {
    // State Enum
    NxAdapterStateClientDataPathStartFailed,
    // State Entry Function
    NxAdapterStateEntryFunc_ClientDataPathStartFailed,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_ClientDataPathStartFailed,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_ClientDataPathStartFailed
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_PauseClientDataPathForNdisPause[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_PauseClientDataPathForNdisPause[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausingIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryPauseClientDataPathForNdisPause = {
    // State Enum
    NxAdapterStatePauseClientDataPathForNdisPause,
    // State Entry Function
    NxAdapterStateEntryFunc_PauseClientDataPathForNdisPause,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_PauseClientDataPathForNdisPause,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_PauseClientDataPathForNdisPause
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_PauseClientDataPathForNdisPause2[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_PauseClientDataPathForNdisPause2[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausedIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryPauseClientDataPathForNdisPause2 = {
    // State Enum
    NxAdapterStatePauseClientDataPathForNdisPause2,
    // State Entry Function
    NxAdapterStateEntryFunc_PauseClientDataPathForNdisPause2,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_PauseClientDataPathForNdisPause2,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_PauseClientDataPathForNdisPause2
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_PauseClientDataPathForPacketClientPauseWhilePaused[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_PauseClientDataPathForPacketClientPauseWhilePaused[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausedIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryPauseClientDataPathForPacketClientPauseWhilePaused = {
    // State Enum
    NxAdapterStatePauseClientDataPathForPacketClientPauseWhilePaused,
    // State Entry Function
    NxAdapterStateEntryFunc_PauseClientDataPathForPacketClientPauseWhilePaused,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_PauseClientDataPathForPacketClientPauseWhilePaused,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_PauseClientDataPathForPacketClientPauseWhilePaused
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_PauseClientDataPathForPacketClientPauseWhileRunning[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_PauseClientDataPathForPacketClientPauseWhileRunning[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausingIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryPauseClientDataPathForPacketClientPauseWhileRunning = {
    // State Enum
    NxAdapterStatePauseClientDataPathForPacketClientPauseWhileRunning,
    // State Entry Function
    NxAdapterStateEntryFunc_PauseClientDataPathForPacketClientPauseWhileRunning,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_PauseClientDataPathForPacketClientPauseWhileRunning,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_PauseClientDataPathForPacketClientPauseWhileRunning
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_PauseClientDataPathForSelfManagedIoSuspend[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_PauseClientDataPathForSelfManagedIoSuspend[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausingIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryPauseClientDataPathForSelfManagedIoSuspend = {
    // State Enum
    NxAdapterStatePauseClientDataPathForSelfManagedIoSuspend,
    // State Entry Function
    NxAdapterStateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_PauseClientDataPathForSelfManagedIoSuspend,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_PauseClientDataPathForSelfManagedIoSuspend
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_PauseClientDataPathForSelfManagedIoSuspend2[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_PauseClientDataPathForSelfManagedIoSuspend2[] = {
    { NxAdapterEventSyncSuccess ,   NxAdapterStateAdapterPausedIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryPauseClientDataPathForSelfManagedIoSuspend2 = {
    // State Enum
    NxAdapterStatePauseClientDataPathForSelfManagedIoSuspend2,
    // State Entry Function
    NxAdapterStateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend2,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_PauseClientDataPathForSelfManagedIoSuspend2,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_PauseClientDataPathForSelfManagedIoSuspend2
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_ShouldClientDataPathStartForNdisRestart[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_ShouldClientDataPathStartForNdisRestart[] = {
    { NxAdapterEventNo ,            NxAdapterStateAdapterPausedIndex },
    { NxAdapterEventYes ,           NxAdapterStateAdapterRestartingIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryShouldClientDataPathStartForNdisRestart = {
    // State Enum
    NxAdapterStateShouldClientDataPathStartForNdisRestart,
    // State Entry Function
    NxAdapterStateEntryFunc_ShouldClientDataPathStartForNdisRestart,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_ShouldClientDataPathStartForNdisRestart,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_ShouldClientDataPathStartForNdisRestart
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_ShouldClientDataPathStartForPacketClientStart[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_ShouldClientDataPathStartForPacketClientStart[] = {
    { NxAdapterEventNo ,            NxAdapterStateAdapterPausedIndex },
    { NxAdapterEventYes ,           NxAdapterStateAdapterRestartingIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryShouldClientDataPathStartForPacketClientStart = {
    // State Enum
    NxAdapterStateShouldClientDataPathStartForPacketClientStart,
    // State Entry Function
    NxAdapterStateEntryFunc_ShouldClientDataPathStartForPacketClientStart,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_ShouldClientDataPathStartForPacketClientStart,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_ShouldClientDataPathStartForPacketClientStart
};



SM_ENGINE_AUTO_EVENT_TRANSITION   NxAdapterAutoEventTransitions_ShouldClientDataPathStartForSelfManagedIoRestart[] = {
    { SmEngineEventNull ,           NULL },
};


SM_ENGINE_STATE_TRANSITION   NxAdapterStateTransitions_ShouldClientDataPathStartForSelfManagedIoRestart[] = {
    { NxAdapterEventNo ,            NxAdapterStateAdapterPausedIndex },
    { NxAdapterEventYes ,           NxAdapterStateAdapterRestartingIndex },
    { SmEngineEventNull ,           NxAdapterStateNullIndex },
};


SM_ENGINE_STATE_TABLE_ENTRY   NxAdapterStateTableEntryShouldClientDataPathStartForSelfManagedIoRestart = {
    // State Enum
    NxAdapterStateShouldClientDataPathStartForSelfManagedIoRestart,
    // State Entry Function
    NxAdapterStateEntryFunc_ShouldClientDataPathStartForSelfManagedIoRestart,
    // State Flags
    (SM_ENGINE_STATE_FLAGS)(SmEngineStateFlagNone),
    // Auto Event Handlers
    NxAdapterAutoEventTransitions_ShouldClientDataPathStartForSelfManagedIoRestart,
    // Event State Pairs for Transitions
    NxAdapterStateTransitions_ShouldClientDataPathStartForSelfManagedIoRestart
};










//
// Global List of States in all the State Machines
//
PSM_ENGINE_STATE_TABLE_ENTRY    NxAdapterStateTable[] =
{
    &NxAdapterStateTableEntryAdapterHalted,
    &NxAdapterStateTableEntryAdapterHalting,
    &NxAdapterStateTableEntryAdapterInitializedRestartDatapath,
    &NxAdapterStateTableEntryAdapterInitializedWaitForSelfManagedIoInit,
    &NxAdapterStateTableEntryAdapterInitializing,
    &NxAdapterStateTableEntryAdapterPaused,
    &NxAdapterStateTableEntryAdapterPausing,
    &NxAdapterStateTableEntryAdapterRestarting,
    &NxAdapterStateTableEntryAdapterRunning,
    &NxAdapterStateTableEntryClientDataPathPauseComplete,
    &NxAdapterStateTableEntryClientDataPathStartFailed,
    &NxAdapterStateTableEntryPauseClientDataPathForNdisPause,
    &NxAdapterStateTableEntryPauseClientDataPathForNdisPause2,
    &NxAdapterStateTableEntryPauseClientDataPathForPacketClientPauseWhilePaused,
    &NxAdapterStateTableEntryPauseClientDataPathForPacketClientPauseWhileRunning,
    &NxAdapterStateTableEntryPauseClientDataPathForSelfManagedIoSuspend,
    &NxAdapterStateTableEntryPauseClientDataPathForSelfManagedIoSuspend2,
    &NxAdapterStateTableEntryShouldClientDataPathStartForNdisRestart,
    &NxAdapterStateTableEntryShouldClientDataPathStartForPacketClientStart,
    &NxAdapterStateTableEntryShouldClientDataPathStartForSelfManagedIoRestart,
};




SM_ENGINE_EVENT
NxAdapterStateEntryFunc_AdapterHalting(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_AdapterHalting is called when the
    state machine enters the AdapterHalting State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_NdisHalt(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_AdapterHalting


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_AdapterInitializedRestartDatapath(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_AdapterInitializedRestartDatapath is called when the
    state machine enters the AdapterInitializedRestartDatapath State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_RestartDatapathAfterStop(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_AdapterInitializedRestartDatapath


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_AdapterInitializing(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_AdapterInitializing is called when the
    state machine enters the AdapterInitializing State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxAdapter::_StateEntryFn_NdisInitialize(context);

    return syncEvent;

} // NxAdapterStateEntryFunc_AdapterInitializing


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_AdapterPaused(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_AdapterPaused is called when the
    state machine enters the AdapterPaused State

--*/
{

    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_ClientDataPathPaused(context);

    return SmEngineEventNull;

} // NxAdapterStateEntryFunc_AdapterPaused


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_AdapterPausing(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_AdapterPausing is called when the
    state machine enters the AdapterPausing State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxAdapter::_StateEntryFn_ClientDataPathPausing(context);

    return syncEvent;

} // NxAdapterStateEntryFunc_AdapterPausing


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_AdapterRestarting(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_AdapterRestarting is called when the
    state machine enters the AdapterRestarting State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxAdapter::_StateEntryFn_ClientDataPathStarting(context);

    return syncEvent;

} // NxAdapterStateEntryFunc_AdapterRestarting


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_AdapterRunning(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_AdapterRunning is called when the
    state machine enters the AdapterRunning State

--*/
{

    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_ClientDataPathStarted(context);

    return SmEngineEventNull;

} // NxAdapterStateEntryFunc_AdapterRunning


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_ClientDataPathPauseComplete(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_ClientDataPathPauseComplete is called when the
    state machine enters the ClientDataPathPauseComplete State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_ClientDataPathPauseComplete(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_ClientDataPathPauseComplete


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_ClientDataPathStartFailed(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_ClientDataPathStartFailed is called when the
    state machine enters the ClientDataPathStartFailed State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_ClientDataPathStartFailed(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_ClientDataPathStartFailed


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_PauseClientDataPathForNdisPause(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_PauseClientDataPathForNdisPause is called when the
    state machine enters the PauseClientDataPathForNdisPause State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_PauseClientDataPathForNdisPause(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_PauseClientDataPathForNdisPause


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_PauseClientDataPathForNdisPause2(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_PauseClientDataPathForNdisPause2 is called when the
    state machine enters the PauseClientDataPathForNdisPause2 State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_PauseClientDataPathForNdisPause2(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_PauseClientDataPathForNdisPause2


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_PauseClientDataPathForPacketClientPauseWhilePaused(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_PauseClientDataPathForPacketClientPauseWhilePaused is called when the
    state machine enters the PauseClientDataPathForPacketClientPauseWhilePaused State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_PauseClientDataPathForPacketClientPauseWhilePaused(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_PauseClientDataPathForPacketClientPauseWhilePaused


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_PauseClientDataPathForPacketClientPauseWhileRunning(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_PauseClientDataPathForPacketClientPauseWhileRunning is called when the
    state machine enters the PauseClientDataPathForPacketClientPauseWhileRunning State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_PauseClientDataPathForPacketClientPauseWhileRunning(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_PauseClientDataPathForPacketClientPauseWhileRunning


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend is called when the
    state machine enters the PauseClientDataPathForSelfManagedIoSuspend State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_PauseClientDataPathForSelfManagedIoSuspend(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend2(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend2 is called when the
    state machine enters the PauseClientDataPathForSelfManagedIoSuspend2 State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    NxAdapter::_StateEntryFn_PauseClientDataPathForSelfManagedIoSuspend(context);

    syncEvent = NxAdapterEventSyncSuccess;

    return syncEvent;

} // NxAdapterStateEntryFunc_PauseClientDataPathForSelfManagedIoSuspend2


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_ShouldClientDataPathStartForNdisRestart(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_ShouldClientDataPathStartForNdisRestart is called when the
    state machine enters the ShouldClientDataPathStartForNdisRestart State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxAdapter::_StateEntryFn_ShouldClientDataPathStartForNdisRestart(context);

    return syncEvent;

} // NxAdapterStateEntryFunc_ShouldClientDataPathStartForNdisRestart


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_ShouldClientDataPathStartForPacketClientStart(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_ShouldClientDataPathStartForPacketClientStart is called when the
    state machine enters the ShouldClientDataPathStartForPacketClientStart State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxAdapter::_StateEntryFn_ShouldClientDataPathStartForPacketClientStart(context);

    return syncEvent;

} // NxAdapterStateEntryFunc_ShouldClientDataPathStartForPacketClientStart


SM_ENGINE_EVENT
NxAdapterStateEntryFunc_ShouldClientDataPathStartForSelfManagedIoRestart(
    __in
      PVOID  ContextAsPVOID
    )
/*++
Routine Description:

    NxAdapter StateEntryFunc_ShouldClientDataPathStartForSelfManagedIoRestart is called when the
    state machine enters the ShouldClientDataPathStartForSelfManagedIoRestart State

--*/
{

    SM_ENGINE_EVENT                 syncEvent;
    PNxAdapter  context;
    
    context = (PNxAdapter) ContextAsPVOID;
    syncEvent = (SM_ENGINE_EVENT)NxAdapter::_StateEntryFn_ShouldClientDataPathStartForSelfManagedIoRestart(context);

    return syncEvent;

} // NxAdapterStateEntryFunc_ShouldClientDataPathStartForSelfManagedIoRestart





