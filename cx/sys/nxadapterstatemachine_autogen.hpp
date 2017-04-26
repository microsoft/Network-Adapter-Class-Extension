
/*++
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 !!    DO NOT MODIFY THIS FILE MANUALLY     !!
 !!    This file is created by              !!
 !!    StateMachineConverter.ps1            !!
 !!    in this directory.                   !!
 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

NxAdapterStateMachine_AutoGen.hpp

Abstract:

     This header file contains State Machines for NxAdapter
     This has been generated automatically from a visio file.

--*/


#pragma once

//
// Index of States in all the State Machines
//
typedef enum _NxAdapterSM_STATE_INDEX {
    NxAdapterStateAdapterHaltedIndex,
    NxAdapterStateAdapterHaltingIndex,
    NxAdapterStateAdapterInitializedRestartDatapathIndex,
    NxAdapterStateAdapterInitializedWaitForSelfManagedIoInitIndex,
    NxAdapterStateAdapterInitializingIndex,
    NxAdapterStateAdapterPausedIndex,
    NxAdapterStateAdapterPausingIndex,
    NxAdapterStateAdapterRestartingIndex,
    NxAdapterStateAdapterRunningIndex,
    NxAdapterStateClientDataPathPauseCompleteIndex,
    NxAdapterStateClientDataPathStartFailedIndex,
    NxAdapterStatePauseClientDataPathForNdisPauseIndex,
    NxAdapterStatePauseClientDataPathForNdisPause2Index,
    NxAdapterStatePauseClientDataPathForPacketClientPauseWhilePausedIndex,
    NxAdapterStatePauseClientDataPathForPacketClientPauseWhileRunningIndex,
    NxAdapterStatePauseClientDataPathForSelfManagedIoSuspendIndex,
    NxAdapterStatePauseClientDataPathForSelfManagedIoSuspend2Index,
    NxAdapterStateShouldClientDataPathStartForNdisRestartIndex,
    NxAdapterStateShouldClientDataPathStartForPacketClientStartIndex,
    NxAdapterStateShouldClientDataPathStartForSelfManagedIoRestartIndex,
    NxAdapterStateNullIndex
} NxAdapterSM_STATE_INDEX;
//
// Enum of States in all the State Machines
//
#define NxAdapterSM_STATE\
    NxAdapterStateAdapterHalted=0x57a73050,\
    NxAdapterStateAdapterHalting=0x57a73058,\
    NxAdapterStateAdapterInitializedRestartDatapath=0x57a73060,\
    NxAdapterStateAdapterInitializedWaitForSelfManagedIoInit=0x57a7304a,\
    NxAdapterStateAdapterInitializing=0x57a7304b,\
    NxAdapterStateAdapterPaused=0x57a7304c,\
    NxAdapterStateAdapterPausing=0x57a73054,\
    NxAdapterStateAdapterRestarting=0x57a73051,\
    NxAdapterStateAdapterRunning=0x57a73052,\
    NxAdapterStateClientDataPathPauseComplete=0x57a73057,\
    NxAdapterStateClientDataPathStartFailed=0x57a73053,\
    NxAdapterStatePauseClientDataPathForNdisPause=0x57a73055,\
    NxAdapterStatePauseClientDataPathForNdisPause2=0x57a73059,\
    NxAdapterStatePauseClientDataPathForPacketClientPauseWhilePaused=0x57a7305d,\
    NxAdapterStatePauseClientDataPathForPacketClientPauseWhileRunning=0x57a7305e,\
    NxAdapterStatePauseClientDataPathForSelfManagedIoSuspend=0x57a73056,\
    NxAdapterStatePauseClientDataPathForSelfManagedIoSuspend2=0x57a7305a,\
    NxAdapterStateShouldClientDataPathStartForNdisRestart=0x57a7304f,\
    NxAdapterStateShouldClientDataPathStartForPacketClientStart=0x57a7305f,\
    NxAdapterStateShouldClientDataPathStartForSelfManagedIoRestart=0x57a7305b,\
    NxAdapterStateNull,\



//
// Event List For the State Machines
//
#define NxAdapterSM_EVENT\
    NxAdapterEventClientPauseComplete         = 0xebe33260 | SmEngineEventPrioritySync,\
    NxAdapterEventClientStartFailed           = 0xebe331b0 | SmEngineEventPrioritySync,\
    NxAdapterEventClientStartSucceeded        = 0xebe331c0 | SmEngineEventPrioritySync,\
    NxAdapterEventInitializeSelfManagedIO     = 0xebe331d0 | SmEngineEventPriorityNormal,\
    NxAdapterEventNdisMiniportHalt            = 0xebe331e0 | SmEngineEventPriorityNormal,\
    NxAdapterEventNdisMiniportInitializeEx    = 0xebe331f0 | SmEngineEventPriorityNormal,\
    NxAdapterEventNdisMiniportPause           = 0xebe33270 | SmEngineEventPriorityNormal,\
    NxAdapterEventNdisMiniportRestart         = 0xebe33250 | SmEngineEventPriorityNormal,\
    NxAdapterEventNo                          = 0xebe33200 | SmEngineEventPrioritySync,\
    NxAdapterEventPacketClientPause           = 0xebe332a0 | SmEngineEventPriorityNormal,\
    NxAdapterEventPacketClientStart           = 0xebe332b0 | SmEngineEventPriorityNormal,\
    NxAdapterEventRestartSelfManagedIO        = 0xebe33290 | SmEngineEventPriorityNormal,\
    NxAdapterEventSuspendSelfManagedIO        = 0xebe33280 | SmEngineEventPriorityNormal,\
    NxAdapterEventSyncFail                    = 0xebe33210 | SmEngineEventPrioritySync,\
    NxAdapterEventSyncPending                 = 0xebe33220 | SmEngineEventPrioritySync,\
    NxAdapterEventSyncSuccess                 = 0xebe33230 | SmEngineEventPrioritySync,\
    NxAdapterEventYes                         = 0xebe33240 | SmEngineEventPrioritySync,\


typedef struct _SM_ENGINE_STATE_TABLE_ENTRY *PSM_ENGINE_STATE_TABLE_ENTRY;
extern PSM_ENGINE_STATE_TABLE_ENTRY NxAdapterStateTable[];
//
// Operations
//
//
// Place this macro in the class declaration
//
#define GENERATED_DECLARATIONS_FOR_NXADAPTER_STATE_ENTRY_FUNCTIONS()  \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_ClientDataPathPauseComplete( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_ClientDataPathPaused( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxAdapter::_StateEntryFn_ClientDataPathPausing( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_ClientDataPathStarted( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_ClientDataPathStartFailed( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxAdapter::_StateEntryFn_ClientDataPathStarting( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_NdisHalt( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxAdapter::_StateEntryFn_NdisInitialize( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_PauseClientDataPathForNdisPause( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_PauseClientDataPathForNdisPause2( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_PauseClientDataPathForPacketClientPauseWhilePaused( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_PauseClientDataPathForPacketClientPauseWhileRunning( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_PauseClientDataPathForSelfManagedIoSuspend( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static VOID \
NxAdapter::_StateEntryFn_RestartDatapathAfterStop( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxAdapter::_StateEntryFn_ShouldClientDataPathStartForNdisRestart( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxAdapter::_StateEntryFn_ShouldClientDataPathStartForPacketClientStart( \
    _In_ PNxAdapter  Context \
); \
__drv_maxIRQL(DISPATCH_LEVEL) \
static SM_ENGINE_EVENT \
NxAdapter::_StateEntryFn_ShouldClientDataPathStartForSelfManagedIoRestart( \
    _In_ PNxAdapter  Context \
); \


