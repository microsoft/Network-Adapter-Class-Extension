// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include "PollReserved.hpp"
#include "ExecutionContextPoll.h"

NONPAGEDX
_Use_decl_annotations_
PollReserved &
ContextFromPoll(
    EXECUTION_CONTEXT_POLL * Poll
)
{
    static_assert(sizeof(PollReserved) <= FIELD_SIZE(EXECUTION_CONTEXT_POLL, Reserved[0]));
    return reinterpret_cast<PollReserved&>(Poll->Reserved[0]);
}
