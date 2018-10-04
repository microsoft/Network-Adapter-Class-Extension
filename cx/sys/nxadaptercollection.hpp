// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "netadaptercx_triage.h"

class NxAdapter;

class NxAdapterCollection
{
public:

    NxAdapterCollection();

    NTSTATUS
    Initialize(
        void
        );

    bool
    Empty(
        void
        ) const;

    ULONG
    Count(
        void
        ) const;

    void
    Clear(
        void
        );

    NxAdapter *
    GetDefaultAdapter(
        void
        ) const;

    void
    AddAdapter(
        _In_ NxAdapter * Adapter
        );

    bool
    RemoveAdapter(
        _In_ NxAdapter * Adapter
        );

    template <typename lambda>
    void
    ForEachAdapterUnlocked(
        _In_ lambda f
        )
    {
        for (
            LIST_ENTRY *link = m_ListHead.Flink;
            link != &m_ListHead;
            link = link->Flink
            )
        {
            auto adapter = CONTAINING_RECORD(link, NxAdapter, m_Linkage);
            f(*adapter);
        }
    }

    template <typename lambda>
    void
    ForEachAdapterLocked(
        _In_ lambda f
        )
    {
        auto lock = wil::acquire_wdf_wait_lock(m_ListLock);
        ForEachAdapterUnlocked(f);
    }

    static
    void
    GetTriageInfo(
        void
        );

private:

    _Guarded_by_(m_ListLock)
    LIST_ENTRY m_ListHead;

    _Guarded_by_(m_ListLock)
    ULONG m_Count = 0;

    WDFWAITLOCK m_ListLock = nullptr;
};
