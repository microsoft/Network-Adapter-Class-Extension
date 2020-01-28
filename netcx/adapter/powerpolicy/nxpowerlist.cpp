// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NxPowerList.tmh"
#include "NxPowerList.hpp"

#include "NxPowerPolicy.hpp"

_Use_decl_annotations_
NxPowerOffloadList const *
GetNxPowerListFromOffloadList(
    NET_POWER_OFFLOAD_LIST const * List
)
{
    auto powerList = reinterpret_cast<NxPowerOffloadList const *>(&List->Reserved[0]);
    NT_FRE_ASSERT(powerList->Signature == NX_POWER_OFFLOAD_LIST_SIGNATURE);
    return powerList;
}

_Use_decl_annotations_
NxWakeSourceList const *
GetNxPowerListFromWakeList(
    NET_WAKE_SOURCE_LIST const * List
)
{
    auto powerList = reinterpret_cast<NxWakeSourceList const *>(&List->Reserved[0]);
    NT_FRE_ASSERT(powerList->Signature == NX_WAKE_SOURCE_LIST_SIGNATURE);
    return powerList;
}

_Use_decl_annotations_
NxPowerList::NxPowerList(
    ULONG TypeSignature
)
    : Signature(TypeSignature)
{
}

size_t
NxPowerList::GetCount(
    void
) const
{
    return m_count;
}

_Use_decl_annotations_
void
NxPowerList::PushEntry(
    NxPowerEntry * Entry
)
{
    PushEntryList(&m_listHead, &Entry->m_clientListLinkage);
    m_count++;
}

NxPowerOffloadList::NxPowerOffloadList(
    void
) noexcept
    : NxPowerList(NX_POWER_OFFLOAD_LIST_SIGNATURE)
{
}

_Use_decl_annotations_
void *
NxPowerOffloadList::operator new(
    size_t,
    NET_POWER_OFFLOAD_LIST * NetPowerOffloadList
)
{
    return &NetPowerOffloadList->Reserved[0];
}

NxWakeSourceList::NxWakeSourceList(
    void
) noexcept
    : NxPowerList(NX_WAKE_SOURCE_LIST_SIGNATURE)
{
}

_Use_decl_annotations_
void *
NxWakeSourceList::operator new(
    size_t,
    NET_WAKE_SOURCE_LIST * NetWakeSourceList
)
{
    return &NetWakeSourceList->Reserved[0];
}

_Use_decl_annotations_
NxWakeSource *
NxWakeSourceList::GetWakeSourceByIndex(
    size_t Index
) const
{
    size_t i = 0;

    for (
        auto link = m_listHead.Next;
        link != nullptr;
        link = link->Next
        )
    {
        if (i == Index)
        {
            return CONTAINING_RECORD(link, NxWakeSource, m_clientListLinkage);
        }

        i++;
    }

    return nullptr;
}

NxPowerOffload *
NxPowerOffloadList::GetPowerOffloadByIndex(
    _In_ size_t Index
) const
{
    size_t i = 0;

    for (auto link = m_listHead.Next; link != nullptr; link = link->Next)
    {
        if (i == Index)
        {
            return CONTAINING_RECORD(link, NxPowerOffload, m_clientListLinkage);
        }

        i++;
    }

    return nullptr;
}
