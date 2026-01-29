// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "PassiveLevelKnobs.hpp"

#include <kmemoryaccess.h>

_Use_decl_annotations_
NTSTATUS
PassiveLevelKnobs::Initialize(void)
{
    return STATUS_SUCCESS;
}

void
PassiveLevelKnobs::CheckKnobsUpdated(void)
{
}

bool
PassiveLevelKnobs::GetKnobsUpdated(void)
{
    return 0;
};

UINT32
PassiveLevelKnobs::GetKnobInt32(KNOB KnobOffset)
{
    return mem::ReadNoFence((UINT32 *)((PUCHAR)&m_knobs + (int)KnobOffset));
}
