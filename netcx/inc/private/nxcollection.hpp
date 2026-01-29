// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KLockHolder.h>

template<typename T>
class NxCollection
{
public:

    NxCollection(
        void
    )
    {
        InitializeListHead(&m_ListHead);
    }

    UINT32
    Count(
        void
    ) const
    {
        return m_Count;
    }

    void
    Add(
        _In_ T * t
    )
    {
        KLockThisExclusive lock(m_ListLock);

        InsertTailList(&m_ListHead, &t->m_linkage);
        m_Count++;
    }

    bool
    Remove(
        _In_ T * t
    )
    /*

    Description:

        Removes an T from this collection.

    Return value:

        true if the item was removed, false if
        the item was not part of this collection

    */
    {
        KLockThisExclusive lock(m_ListLock);

        //
        // Check if this adapter is in the adapter collection
        //
        bool found = false;
        for (LIST_ENTRY *link = m_ListHead.Flink;
            link != &m_ListHead;
            link = link->Flink)
        {
            if (link == &t->m_linkage)
            {
                found = true;
                break;
            }
        }

        //
        // If we found this entry in the adapter colletion, remove it and decrement
        // the count
        //
        if (found)
        {
            RemoveEntryList(&t->m_linkage);
            t->m_linkage.Flink = nullptr;
            t->m_linkage.Blink = nullptr;

            m_Count--;
        }

        return found;
    }

    template <typename lambda>
    void
    ForEach(
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
            auto t = CONTAINING_RECORD(link, T, m_linkage);
            f(*t);
        }
    }

protected:

    _Guarded_by_(m_ListLock)
    LIST_ENTRY
        m_ListHead = {};

    _Guarded_by_(m_ListLock)
    UINT32
        m_Count = 0;

    mutable KPushLock
        m_ListLock;
};
