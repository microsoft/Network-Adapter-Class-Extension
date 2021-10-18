// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include "NxAdapterCollection.hpp"
#include "NxAdapter.hpp"

#include "NxAdapterCollection.tmh"

void
NxAdapterCollection::GetTriageInfo(
    void
)
{
    g_NetAdapterCxTriageBlock.NxAdapterCollectionCountOffset = FIELD_OFFSET(NxAdapterCollection, m_Count);
}

_Use_decl_annotations_
unique_miniport_reference
NxAdapterCollection::FindAndReferenceMiniportByInstanceName(
    UNICODE_STRING const * InstanceName
) const
{
    KLockThisShared lock(m_ListLock);

    for (LIST_ENTRY *link = m_ListHead.Flink;
        link != &m_ListHead;
        link = link->Flink)
    {
        auto nxAdapter = CONTAINING_RECORD(link, NxAdapter, m_linkage);

        if (RtlEqualUnicodeString(&nxAdapter->m_instanceName, InstanceName, TRUE))
        {
            return nxAdapter->GetMiniportReference();
        }
    }

    return nullptr;
}

_Use_decl_annotations_
NxAdapter *
NxAdapterCollection::FindAndReferenceAdapterByBaseName(
    UNICODE_STRING const * BaseName
) const
{
    KLockThisShared lock(m_ListLock);

    for (LIST_ENTRY *link = m_ListHead.Flink;
        link != &m_ListHead;
        link = link->Flink)
    {
        auto nxAdapter = CONTAINING_RECORD(link, NxAdapter, m_linkage);

        if (RtlEqualUnicodeString(&nxAdapter->m_baseName, BaseName, TRUE))
        {
            return NdisWdfMiniportTryReference(nxAdapter->GetNdisHandle()) ? nxAdapter : nullptr;
        }
    }

    return nullptr;
}
