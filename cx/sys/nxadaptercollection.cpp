// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include "NxAdapterCollection.tmh"
#include "NxAdapterCollection.hpp"

#include "NxAdapter.hpp"

NxAdapterCollection::NxAdapterCollection(
    void
    )
{
    InitializeListHead(&m_ListHead);
}

NTSTATUS
NxAdapterCollection::Initialize(
    void
    )
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfWaitLockCreate(
            WDF_NO_OBJECT_ATTRIBUTES,
            &m_ListLock),
        "Failed to create NxAdapterCollection wait lock");

    return STATUS_SUCCESS;
}

bool
NxAdapterCollection::Empty(
    void
    ) const
{
    return m_Count == 0;
}

ULONG
NxAdapterCollection::Count(
    void
    ) const
{
    return m_Count;
}

void
NxAdapterCollection::Clear(
    void
    )
{
    auto lock = wil::acquire_wdf_wait_lock(m_ListLock);
    while (!IsListEmpty(&m_ListHead))
    {
        RemoveHeadList(&m_ListHead);
    }
}

NxAdapter *
NxAdapterCollection::GetDefaultAdapter(
    void
    ) const
{
    auto lock = wil::acquire_wdf_wait_lock(m_ListLock);

    for (
        LIST_ENTRY *link = m_ListHead.Flink;
        link != &m_ListHead;
        link = link->Flink
        )
    {
        NxAdapter *nxAdapter = CONTAINING_RECORD(link, NxAdapter, m_Linkage);

        if (nxAdapter->IsDefault())
            return nxAdapter;
    }

    return nullptr;
}

_Use_decl_annotations_
void
NxAdapterCollection::AddAdapter(
    NxAdapter * Adapter
    )
{
    auto lock = wil::acquire_wdf_wait_lock(m_ListLock);

    InsertTailList(&m_ListHead, &Adapter->m_Linkage);
    m_Count++;
}

_Use_decl_annotations_
bool
NxAdapterCollection::RemoveAdapter(
    NxAdapter * Adapter
    )
/*

Description:

    Removes an adapter from this collection.

Return value:

    true if the adapter was removed, false if
    the adapter was not part of this collection

*/
{
    auto lock = wil::acquire_wdf_wait_lock(m_ListLock);

    //
    // Check if this adapter is in the adapter collection
    //
    bool foundAdapter = false;
    for (LIST_ENTRY *link = m_ListHead.Flink;
        link != &m_ListHead;
        link = link->Flink)
    {
        if (link == &Adapter->m_Linkage)
        {
            foundAdapter = true;
            break;
        }
    }

    //
    // If we found this entry in the adapter colletion, remove it and decrement
    // the count
    //
    if (foundAdapter)
    {
        RemoveEntryList(&Adapter->m_Linkage);
        Adapter->m_Linkage.Flink = nullptr;
        Adapter->m_Linkage.Blink = nullptr;

        m_Count--;
    }

    return foundAdapter;
}

void
NxAdapterCollection::GetTriageInfo(
    void
)
{
    g_NetAdapterCxTriageBlock.NxAdapterCollectionCountOffset = FIELD_OFFSET(NxAdapterCollection, m_Count);
}
