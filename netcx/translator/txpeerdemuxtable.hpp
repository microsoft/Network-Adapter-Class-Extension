// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KArray.h>
#include <KSpinLock.h>
#include <NetClientTypes.h>

class TxPeerDemuxTable final
{

public:
    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    Initialize(
        size_t MaxPeers
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Insert(
        NET_CLIENT_EUI48_ADDRESS const* Address
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    size_t
    Remove(
        NET_CLIENT_EUI48_ADDRESS const* Address
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    size_t
    Lookup(
        NET_CLIENT_EUI48_ADDRESS const* Address
    ) const;

private:
    _IRQL_requires_max_(DISPATCH_LEVEL)
    size_t
    LookupUnlocked(
        NET_CLIENT_EUI48_ADDRESS const* Address
    ) const;

    struct PeerEntry
    {

        NET_CLIENT_EUI48_ADDRESS
            Address;

        bool 
            Empty = true;
    };

    mutable KSpinLock
        m_lock;

    Rtl::KArray<PeerEntry, NonPagedPoolNx>
        m_table;
};
