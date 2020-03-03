// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "netadaptercx_triage.h"

#include <KLockHolder.h>

class NxAdapter;

class NxAdapterCollection
{
public:

    NxAdapterCollection(
        void
    );

    ULONG
    Count(
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
    ForEachAdapter(
        _In_ lambda f
    )
    {
        KLockThisShared lock(m_ListLock);

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

    static
    void
    GetTriageInfo(
        void
    );

    NxAdapter *
    FindAndReferenceAdapterByInstanceName(
        _In_ UNICODE_STRING const * InstanceName
    ) const;

    NxAdapter *
    FindAndReferenceAdapterByBaseName(
        _In_ UNICODE_STRING const * BaseName
    ) const;

private:

    _Guarded_by_(m_ListLock)
    LIST_ENTRY
        m_ListHead = {};

    _Guarded_by_(m_ListLock)
    ULONG
        m_Count = 0;

    mutable KPushLock
        m_ListLock;
};
