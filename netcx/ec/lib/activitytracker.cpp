// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "ActivityTracker.hpp"
#include "ExecutionContext.hpp"
#include "EcEvents.h"

_Use_decl_annotations_
ActivityTracker::ActivityTracker(
    _In_ GUID const & Identifier
)
    : m_identifier(Identifier)
{
}

_Use_decl_annotations_
void
ActivityTracker::Initialize(
    UNICODE_STRING const * Name
)
{
    m_friendlyName = Name;
}

_Use_decl_annotations_
void
ActivityTracker::LogEvent(
    ExecutionContextEvent Event
)
{
    m_entries[m_entryIndex].Type = Entry::Type::Event;
    m_entries[m_entryIndex].u.Event.EventType = Event;

    m_entryIndex = (m_entryIndex + 1) % NumberOfEntries;
}

_Use_decl_annotations_
void
ActivityTracker::LogStateChange(
    ExecutionContextStateChangeReason Reason,
    ExecutionContextState PreviousState,
    ExecutionContextState NewState
)
{
    auto & entry = m_entries[m_entryIndex];
    entry.Type = Entry::Type::StateChange;
    entry.u.StateChange.Reason = Reason;
    entry.u.StateChange.PreviousState = PreviousState;
    entry.u.StateChange.NewState = NewState;

    m_entryIndex = (m_entryIndex + 1) % NumberOfEntries;

    EventWriteExecutionContextStateChange(
        &m_identifier,
        static_cast<UINT32>(Reason),
        static_cast<UINT32>(PreviousState),
        static_cast<UINT32>(NewState),
        m_taskCounter,
        m_iterationCounter,
        m_workCounter,
        m_friendlyName->Length / sizeof(UNICODE_NULL),
        m_friendlyName->Buffer);
}

_Use_decl_annotations_
void
ActivityTracker::LogIteration(
    UINT64 Work
)
{
    m_iterationCounter++;
    m_workCounter += Work;
}

_Use_decl_annotations_
void
ActivityTracker::LogTask(
    void
)
{
    m_taskCounter++;
}

_Use_decl_annotations_
void
ActivityTracker::ResetCounters(
    void
)
{
    m_iterationCounter = 0;
    m_workCounter = 0;
    m_taskCounter = 0;
}
