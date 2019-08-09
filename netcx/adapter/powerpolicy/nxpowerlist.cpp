// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NxPowerList.tmh"
#include "NxPowerList.hpp"

_Use_decl_annotations_
NxPowerList const *
GetNxPowerListFromOffloadList(
    NET_POWER_OFFLOAD_LIST const * List
)
{
    auto powerList = reinterpret_cast<NxPowerList const *>(&List->Reserved[0]);
    NT_FRE_ASSERT(powerList->Signature == NX_POWER_OFFLOAD_LIST_SIGNATURE);
    return powerList;
}

_Use_decl_annotations_
NxPowerList const *
GetNxPowerListFromWakeList(
    NET_WAKE_SOURCE_LIST const * List
)
{
    auto powerList = reinterpret_cast<NxPowerList const *>(&List->Reserved[0]);
    NT_FRE_ASSERT(powerList->Signature == NX_WAKE_SOURCE_LIST_SIGNATURE);
    return powerList;
}

_Use_decl_annotations_
NxPowerList::NxPowerList(
    NX_POWER_ENTRY_TYPE Type
)
    : EntryType(Type)
    , Signature(
        Type == NxPowerEntryTypeProtocolOffload
            ? NX_POWER_OFFLOAD_LIST_SIGNATURE
            : NX_WAKE_SOURCE_LIST_SIGNATURE)
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
NX_NET_POWER_ENTRY const *
NxPowerList::GetElement(
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
            return CONTAINING_RECORD(
                link,
                NX_NET_POWER_ENTRY,
                PowerListEntry);
        }

        i++;
    }

    return nullptr;
}

_Use_decl_annotations_
void
NxPowerList::PushEntry(
    NX_NET_POWER_ENTRY * Entry
)
{
    PushEntryList(
        &m_listHead,
        &Entry->PowerListEntry);

    m_count++;
}
