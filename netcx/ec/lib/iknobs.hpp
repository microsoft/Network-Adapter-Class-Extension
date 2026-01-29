// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "ExecutionContext.h"

enum class KNOB
{
    Flags = FIELD_OFFSET(EXECUTION_CONTEXT_RUNTIME_KNOBS, Flags),
    MaxTimeAtDispatch = FIELD_OFFSET(EXECUTION_CONTEXT_RUNTIME_KNOBS, MaxTimeAtDispatch),
    DispatchTimeWarning = FIELD_OFFSET(EXECUTION_CONTEXT_RUNTIME_KNOBS, DispatchTimeWarning),
    DispatchTimeWarningInterval = FIELD_OFFSET(EXECUTION_CONTEXT_RUNTIME_KNOBS, DispatchTimeWarningInterval),
    DpcWatchdogTimerThreshold = FIELD_OFFSET(EXECUTION_CONTEXT_RUNTIME_KNOBS, DpcWatchdogTimerThreshold),
    WorkerThreadPriority = FIELD_OFFSET(EXECUTION_CONTEXT_RUNTIME_KNOBS, WorkerThreadPriority),
};

class IKnobs {
public:
    virtual
    UINT32
    GetKnobInt32(
        KNOB KnobOffset
    ) = 0;

    virtual
    bool
    GetKnobsUpdated(
        void
    ) = 0;
    
    virtual
    void
    CheckKnobsUpdated(
        void
    ) = 0;
};
