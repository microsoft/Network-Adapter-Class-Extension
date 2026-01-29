// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "Dispatcher.hpp"
#include "DispatcherWorker.hpp"

_Use_decl_annotations_
DispatcherWorker *
DispatcherWorker::FromEntry(
    LIST_ENTRY * ListEntry
)
{
    return CONTAINING_RECORD(ListEntry, DispatcherWorker, Entry);
}

_Use_decl_annotations_
DispatcherWorker::DispatcherWorker(
    Dispatcher & Dispatcher,
    DispatchPriority Priority,
    DispatcherWorkerEntry * WorkerEntry,
    DispatcherWorkerStart * WorkerStart,
    DispatcherWorkerStop * WorkerStop
)
    : Priority{ Priority }
    , WorkerStart{ WorkerStart }
    , WorkerStop{ WorkerStop }
    , WorkerEntry{ WorkerEntry }
    , TargetDispatcher{ &Dispatcher }
{
}

_Use_decl_annotations_
void
DispatcherWorker::SetTargetDispatcher(
    Dispatcher & TargetDispatcher
)
/*
    This method will set the dispatcher where this worker will be executed. The behavior is as follow:

    - If this worker is stopped, the next time Insert() is called the new dispatcher will be used
    - If this worker is ready on the current dispatcher, it will be moved to the new dispatcher
      when the current one selects it for execution
    - If this worker is active on the current dispatcher, it will be moved to the new dispatcher the
      next time it is selected for execution
*/
{
    KAcquireSpinLock workerLock{ Lock };
    this->TargetDispatcher = &TargetDispatcher;
}

_Use_decl_annotations_
bool
DispatcherWorker::Insert(
    void
)
/*
    This method will insert this worker in the target dispatcher's ready list. See comments for
    SetTargetDispatcher() to understand how synchronization works between these two methods.
*/
{
    Dispatcher *targetDispatcher;
    {
        KAcquireSpinLock workerLock{ Lock };

        NT_FRE_ASSERT(this->TargetDispatcher != nullptr);
        targetDispatcher = this->TargetDispatcher;
    }

    return targetDispatcher->InsertWorker(this);
}

_Use_decl_annotations_
void
DispatcherWorker::Rundown(
    void
)
/*
    This method is used to ensure a worker is no longer being used by the target dispatcher.
    Upon return, the follow guarantees are given to the caller:

    - No call to Insert() will succeed (it will return false)
    - It is no longer being executed by any dispatcher and it is not in any list (ready or active)

    In addition to the above guarantees, if this is currently being executed the dispatcher will try
    to speed up the rundown by processing this request as soon as possible.
*/
{
    KAcquireSpinLock workerLock{ Lock };

    // Can only rundown once
    NT_FRE_ASSERT(State.RunningDown == 0);
    State.RunningDown = 1;

    if (State.Ready == 0 && State.Active == 0)
    {
        // Nothing else to do
        RundownComlete.Set();
        return;
    }

    // The worker is currently owned by a dispatcher (it is either ready, active or both). We need to
    // delegate the rundown to the dispatcher.
    CurrentDispatcher->RundownActiveWorker(this);

    // Wait until the dispatcher is done with this worker
    workerLock.Release();
    RundownComlete.Wait();
    workerLock.Acquire();

    NT_FRE_ASSERT(
        State.Ready == 0 &&
        State.Active == 0 &&
        State.RunningDown == 1);
}

_Use_decl_annotations_
void
DispatcherWorker::InvokeStart(
    void
)
{
    if (this->WorkerStart != nullptr)
    {
        this->WorkerStart(this);
    }
}
