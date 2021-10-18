// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <wil/resource.h>
#include <knew.h>
#include <KMacros.h>

enum class ExecutionContextEvent;
enum class ExecutionContextState;
enum class ExecutionContextStateChangeReason;

class ActivityTracker
{
private:

    static const
        size_t NumberOfEntries = 64;

    static_assert(RTL_IS_POWER_OF_TWO(NumberOfEntries));

    struct Entry
    {
        enum class Type
        {
            Event = 1,
            StateChange,
        };

        Type Type;

        union
        {
            // Used if Type == StateChange
            struct
            {
                ExecutionContextStateChangeReason Reason;
                ExecutionContextState PreviousState;
                ExecutionContextState NewState;
            } StateChange;

            // Used if Type == Event
            struct
            {
                ExecutionContextEvent EventType;
            } Event;
        } u;
    };

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    ActivityTracker(
        _In_ GUID const & Identifier
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Initialize(
        _In_ UNICODE_STRING const * Name
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    LogEvent(
        _In_ ExecutionContextEvent Event
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    LogStateChange(
        _In_ ExecutionContextStateChangeReason Reason,
        _In_ ExecutionContextState PreviousState,
        _In_ ExecutionContextState NewState
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    LogIteration(
        _In_ UINT64 Work
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    LogTask(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    ResetCounters(
        void
    );

private:

    GUID const
        m_identifier;

     UNICODE_STRING const *
        m_friendlyName = nullptr;

    Entry
        m_entries[NumberOfEntries];

    size_t
        m_entryIndex = 0;

    UINT64
        m_iterationCounter = 0;

    UINT64
        m_workCounter = 0;

    UINT32
        m_taskCounter = 0;
};
