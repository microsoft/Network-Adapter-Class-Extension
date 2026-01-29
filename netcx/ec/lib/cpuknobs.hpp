// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>
#include <wil/resource.h>
#include <KArray.h>
#include "KnobDescriptor.h"
#include "KnobNamespace.h"
#include "ExecutionContext.h"
#include "IKnobs.hpp"

class CpuKnobs : public NONPAGED_OBJECT<'bkCE'>, public IKnobs
{
public:
    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        _In_ ULONG cpu
    );

    void
    CheckKnobsUpdated(
        void
    ) override;

    void
    MarkKnobsUpdated(
        void
    );

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
        m_knobs;
    
    KnobNamespace
        m_knobNamespace;
    
    Rtl::KArray<KnobDescriptor>
        m_knobDescriptors;

    volatile LONG
        m_knobsUpdated = 0;
};
