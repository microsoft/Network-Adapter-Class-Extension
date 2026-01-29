// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "DispatcherWorker.hpp"

class Dispatcher;
struct DispatcherCounters;

class ActiveList
{
public:

    _IRQL_requires_(PASSIVE_LEVEL)
    ActiveList(
        Dispatcher const * Dispatcher,
        DispatcherCounters & Counters
    );

    template <typename Lambda>
    void
    Enumerate(
        Lambda Callback
    )
    {
        bool keepIterating = true;

        for (auto entry = m_listHead.Flink; keepIterating && entry != &m_listHead; entry = entry->Flink)
        {
            auto worker = DispatcherWorker::FromEntry(entry);
            keepIterating = Callback(*worker);
        }
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Insert(
        _In_ DispatcherWorker * Worker
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    DispatcherWorker *
    MoveNext(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    RemoveEntry(
        DispatcherWorker * Worker
    );

private:

    LIST_ENTRY
        m_listHead;

    // This points to the next worker the dispatcher will invoke. We need this state here to
    // ensure we continue from the right place in case we need to switch contexts (e.g. from
    // DPC to worker thread). If the active list is empty this points to the list head.
    LIST_ENTRY *
        m_next{ &m_listHead };

    // Pointer to the Dispatcher instance that owns this list. This is here strictly to facilitate
    // tracing code.
    Dispatcher const *
        m_dispatcher{ nullptr };

    DispatcherCounters &
        m_counters;
};
