// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <KArray.h>

#include "NxUtility.hpp"

#include "NxAdapterCollection.hpp"
#include "NxAdapter.hpp"
#include "ndisminiport/NxNdisPnpPower.hpp"

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
            return nxAdapter->m_ndisPnpPower->TryReferenceNdisMiniport() ? nxAdapter : nullptr;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
NTSTATUS
NxAdapterCollection::GetAllAdapters(
    Rtl::KArray<unique_wdf_reference<NETADAPTER>> & AdaptersArray
) const
{
    if (AdaptersArray.count() != 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    KLockThisShared lock(m_ListLock);

    if (!AdaptersArray.reserve(m_Count))
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (LIST_ENTRY *link = m_ListHead.Flink;
        link != &m_ListHead;
        link = link->Flink)
    {
        auto nxAdapter = CONTAINING_RECORD(link, NxAdapter, m_linkage);
        WdfObjectReference(nxAdapter->GetFxObject());

        unique_wdf_reference<NETADAPTER> reference{nxAdapter->GetFxObject()};
        AdaptersArray.append(wistd::move(reference));
    }

    return STATUS_SUCCESS;
}
