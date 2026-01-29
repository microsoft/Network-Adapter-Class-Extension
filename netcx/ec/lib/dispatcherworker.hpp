// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <kspinlock.h>
#include <kwaitevent.h>

class Dispatcher;
struct DispatcherWorker;

enum class WorkerExitStatus
{
    // This will tell the dispatcher to keep this EC in the active list
    Yield = 1,
    // This will tell the dispatcher to remove the EC from the active list
    Remove,
};

typedef
void
DispatcherWorkerStart(
    DispatcherWorker * Worker
);

typedef
void
DispatcherWorkerStop(
    DispatcherWorker * Worker
);

typedef
WorkerExitStatus
DispatcherWorkerEntry(
    DispatcherWorker * Worker,
    KIRQL Irql
);

enum DispatchPriority : UINT32
{
    DispatchHighPriority = 0,
    DispatchNormalPriority = 1,
    DispatchPriorityMax
};

struct DispatcherWorker
{
    static
    DispatcherWorker *
    FromEntry(
        _In_ LIST_ENTRY * ListEntry
    );

    _No_competing_thread_
    DispatcherWorker(
        Dispatcher & Dispatcher,
        DispatchPriority Priority,
        DispatcherWorkerEntry * WorkerEntry,
        DispatcherWorkerStart * WorkerStart = nullptr,
        DispatcherWorkerStop * WorkerStop = nullptr
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SetTargetDispatcher(
        _In_ Dispatcher & TargetDispatcher
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    Insert(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Rundown(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    InvokeStart(
        void
    );

    const DispatchPriority
        Priority;

    // This is invoked by the current dispatcher to indicate this worker was just added
    // to the active list
    DispatcherWorkerStart * const
        WorkerStart;

    // This is invoked by the current dispatcher to indicate this worker was just remoed
    // from the active list
    DispatcherWorkerStop * const
        WorkerStop;

    // Work function, it keeps getting called until the function itself tells it should stop
    // by returning WorkerExitStatus::Remove or Worker::Rundown() is called
    DispatcherWorkerEntry * const
        WorkerEntry;

    // This has no functional effect, it is just here so that it can be written out
    // when we are tracing events related to this worker. This can be used in whichever
    // way the caller wants, it is opaque to us.
    UINT64
        DebugData;

    KSpinLock
        Lock;

    struct
    {
        UINT32 NeedsPassiveLevel : 1;
        UINT32 Remaining : 31;
    } Flags;

    _Guarded_by_(Lock)
    struct
    {
        UINT32 Ready : 1;
        UINT32 Active : 1;
        UINT32 RunningDown : 1;
        UINT32 Remaining : 29;
    } State{};

    _Guarded_by_(Lock)
    KWaitEvent
        RundownComlete;

    _Guarded_by_(Lock)
    LIST_ENTRY
        Entry{};

    // This will contain the pointer for the Dispatcher that has this object in
    // one of its lists (ready or active)
    _Guarded_by_(Lock)
    Dispatcher *
        CurrentDispatcher{ nullptr };

    _Guarded_by_(Lock)
    Dispatcher *
        TargetDispatcher{ nullptr };
};
