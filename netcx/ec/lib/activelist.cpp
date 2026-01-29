// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "ActiveList.hpp"
#include "Dispatcher.hpp"
#include "DispatcherCounters.hpp"
#include <EcEvents.h>

_Use_decl_annotations_
ActiveList::ActiveList(
    Dispatcher const * Dispatcher,
    DispatcherCounters & Counters
)
    : m_dispatcher{ Dispatcher }
    , m_counters{ Counters }
{
    InitializeListHead(&m_listHead);
}

_Use_decl_annotations_
void
ActiveList::Insert(
    DispatcherWorker * Worker
)
{
    LIST_ENTRY *i;

    for (i = m_next; i->Flink != m_next; i = i->Flink)
    {
        if (i == &m_listHead)
        {
            continue;
        }

        auto currentWorker = DispatcherWorker::FromEntry(i);

        if (Worker->Priority < currentWorker->Priority)
        {
            break;
        }
    }

    EventWriteDispatcherInsertActiveList(
        reinterpret_cast<UINT64>(&m_dispatcher),
        reinterpret_cast<UINT64>(Worker),
        Worker->DebugData,
        Worker->Priority);

    if (Worker->Priority < ARRAYSIZE(m_counters.ActiveList.AsArray))
    {
        InterlockedIncrement64(
            reinterpret_cast<LONG64 volatile *>(&m_counters.ActiveList.AsArray[Worker->Priority]));
    }

    // Insert the new entry before i
    InsertTailList(i, &Worker->Entry);

    // If the new entry was inserted before the current next entry we need to update it
    if (i == m_next)
    {
        m_next = &Worker->Entry;
    }
}

_Use_decl_annotations_
DispatcherWorker *
ActiveList::MoveNext(
    void
)
{
    if (m_next == &m_listHead)
    {
        NT_FRE_ASSERT(IsListEmpty(&m_listHead));
        return nullptr;
    }

    auto worker = DispatcherWorker::FromEntry(m_next);

    m_next = m_next->Flink != &m_listHead
        ? m_next->Flink
        : m_next->Flink->Flink;

    return worker;
}

_Use_decl_annotations_
void
ActiveList::RemoveEntry(
    DispatcherWorker * Worker
)
{
    // Tell the worker it will stop running, if it wants that
    if (Worker->WorkerStop != nullptr)
    {
        Worker->WorkerStop(Worker);
    }

    if (&Worker->Entry == m_next)
    {
        m_next = m_next->Flink != &m_listHead
            ? m_next->Flink
            : m_next->Flink->Flink;
    }

    if (Worker->Priority < ARRAYSIZE(m_counters.ActiveList.AsArray))
    {
        InterlockedDecrement64(
            reinterpret_cast<LONG64 volatile *>(&m_counters.ActiveList.AsArray[Worker->Priority]));
    }

    EventWriteDispatcherRemoveActiveList(
        reinterpret_cast<UINT64>(&m_dispatcher),
        reinterpret_cast<UINT64>(Worker),
        Worker->DebugData,
        Worker->Priority);

    RemoveEntryList(&Worker->Entry);

    if (IsListEmpty(&m_listHead))
    {
        m_next = &m_listHead;
    }
}
