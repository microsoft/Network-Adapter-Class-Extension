// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"

#include "NxXlatCommon.hpp"

#include "TxPeerDemuxTable.tmh"
#include "TxPeerDemuxTable.hpp"

_Use_decl_annotations_
PAGEDX
NTSTATUS
TxPeerDemuxTable::Initialize(
    size_t MaxPeers
)
{
    if (!m_table.resize(MaxPeers))
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
TxPeerDemuxTable::Insert(
    NET_CLIENT_EUI48_ADDRESS const* Address
)
{
    NT_FRE_ASSERT(m_table.count() != 0U);

    KAcquireSpinLock lock(m_lock);

    for (size_t i = 0U; i < m_table.count(); i++)
    {
        auto& entry = m_table[i];

        if (entry.Empty)
        {
            entry.Address = *Address;
            entry.Empty = false;
            return;
        }
    }

    NT_FRE_ASSERTMSG("Peer Table Overflow", false);
}

_Use_decl_annotations_
size_t
TxPeerDemuxTable::Remove(
    NET_CLIENT_EUI48_ADDRESS const* Address
)
{
    NT_FRE_ASSERT(m_table.count() != 0U);

    KAcquireSpinLock lock(m_lock);

    auto const index = LookupUnlocked(Address);

    NT_FRE_ASSERTMSG("Peer Table Entry Missing", index != SIZE_T_MAX);

    m_table[index].Empty = true;

    return index;
}

_Use_decl_annotations_
size_t
TxPeerDemuxTable::Lookup(
    NET_CLIENT_EUI48_ADDRESS const* Address
) const
{
    KAcquireSpinLock lock(m_lock);

    return LookupUnlocked(Address);
}

_Use_decl_annotations_
size_t TxPeerDemuxTable::LookupUnlocked(
    NET_CLIENT_EUI48_ADDRESS const* Address
) const
{
    for (size_t i = 0U; i < m_table.count(); i++)
    {
        auto const& entry = m_table[i];

        if (entry.Empty)
        {
            continue;
        }

        auto const size = sizeof(entry.Address.Value);
        if (RtlEqualMemory(&entry.Address, Address, size))
        {
            return i;
        }
    }

    return SIZE_T_MAX;
}
