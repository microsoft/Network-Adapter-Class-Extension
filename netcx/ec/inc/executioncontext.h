// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

DECLARE_HANDLE(NET_EXECUTION_CONTEXT);

typedef enum _EXECUTION_CONTEXT_FLAGS
{
    ExecutionContextFlagNone = 0,
    ExecutionContextFlagRunDpcForFirstLoop = 1 << 0,
    ExecutionContextFlagRunWorkerThreadAtDispatch = 1 << 1,
    ExecutionContextFlagTryExtendMaxTimeAtDispatch = 1 << 2,
} EXECUTION_CONTEXT_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS(EXECUTION_CONTEXT_FLAGS)

typedef struct _EXECUTION_CONTEXT_WORK_UNIT_KNOBS
{
    UINT32 AtPassive;
    UINT32 AtDispatch;
} EXECUTION_CONTEXT_WORK_UNIT_KNOBS;

typedef struct _EXECUTION_CONTEXT_RUNTIME_KNOBS
{
    ULONG Size;

    EXECUTION_CONTEXT_FLAGS Flags;
    UINT32 MaxTimeAtDispatch;
    UINT32 DispatchTimeWarning;
    UINT32 DispatchTimeWarningInterval;
    UINT32 DpcWatchdogTimerThreshold;
    UINT32 WorkerThreadPriority;

    EXECUTION_CONTEXT_WORK_UNIT_KNOBS MaxPacketsSend;
    EXECUTION_CONTEXT_WORK_UNIT_KNOBS MaxPacketsSendComplete;
    EXECUTION_CONTEXT_WORK_UNIT_KNOBS MaxPacketsReceive;
    EXECUTION_CONTEXT_WORK_UNIT_KNOBS MaxPacketsReceiveComplete;

} EXECUTION_CONTEXT_RUNTIME_KNOBS;

typedef struct _EXECUTION_CONTEXT_CONFIGURATION
{
    ULONG Size;
    EXECUTION_CONTEXT_RUNTIME_KNOBS const * RuntimeKnobs;
    GUID const * ClientIdentifier;
    UNICODE_STRING ClientFriendlyName;
} EXECUTION_CONTEXT_CONFIGURATION;

inline
void
INITIALIZE_EXECUTION_CONTEXT_CONFIGURATION(
    _Out_ EXECUTION_CONTEXT_CONFIGURATION * Configuration
)
{
    RtlZeroMemory(Configuration, sizeof(*Configuration));
    Configuration->Size = sizeof(*Configuration);
}

inline
void
INITIALIZE_EXECUTION_CONTEXT_RUNTIME_KNOBS(
    _Out_ EXECUTION_CONTEXT_RUNTIME_KNOBS * RuntimeKnobs
)
{
    RtlZeroMemory(RuntimeKnobs, sizeof(*RuntimeKnobs));
    RuntimeKnobs->Size = sizeof(*RuntimeKnobs);
    RuntimeKnobs->Flags = ExecutionContextFlagNone;
}
