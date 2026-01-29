// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>
#include <wil/resource.h>
#include <KArray.h>
#include "ExecutionContext.h"
#include "IKnobs.hpp"

class PassiveLevelKnobs : public IKnobs
{
public:
    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize();

    void
    CheckKnobsUpdated(
        void
    ) override;

    UINT32
    GetKnobInt32(
        KNOB KnobOffset
    ) override;

    bool
    GetKnobsUpdated(
        void
    ) override;

private:
    EXECUTION_CONTEXT_RUNTIME_KNOBS
        m_knobs =
            {
                sizeof(EXECUTION_CONTEXT_RUNTIME_KNOBS), // Size
                ExecutionContextFlagNone, // Flags
                0, // MaxTimeAtDispatch
                0, // DispatchTimeWarning
                15 * 60 * 1000, // DispatchTimeWarningInterval
                0, // DpcWatchdogTimerThreshold
                8, // WorkerThreadPriority
                {
                    64, // MaxPacketsSend.AtPassive
                    64, // MaxPacketsSend.AtDispatch
                },
                {
                    64, // MaxPacketsSendComplete.AtPassive
                    64, // MaxPacketsSendComplete.AtDispatch
                },
                {
                    64, // MaxPacketsReceive.AtPassive
                    64, // MaxPacketsReceive.AtDispatch
                },
                {
                    64, // MaxPacketsReceiveComplete.AtPassive
                    64, // MaxPacketsReceiveComplete.AtDispatch
                },
            };
};
