// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxPowerPolicy.hpp"

#define NX_POWER_OFFLOAD_LIST_SIGNATURE 'LPxN'
#define NX_WAKE_SOURCE_LIST_SIGNATURE 'SWxN'

class NxPowerList
{
public:

    ULONG const
        Signature;

    NX_POWER_ENTRY_TYPE const
        EntryType;

    NxPowerList(
        _In_ NX_POWER_ENTRY_TYPE Type
    );

    size_t
    GetCount(
        void
    ) const;

    NX_NET_POWER_ENTRY const *
    GetElement(
        _In_ size_t Index
    ) const;

    void
    PushEntry(
        _In_ NX_NET_POWER_ENTRY * Entry
    );

private:

    SINGLE_LIST_ENTRY
        m_listHead = {};

    size_t
        m_count = 0;
};

static_assert(sizeof(NxPowerList) <= sizeof(NET_POWER_OFFLOAD_LIST::Reserved));

NxPowerList const *
GetNxPowerListFromOffloadList(
    _In_ NET_POWER_OFFLOAD_LIST const * List
);

NxPowerList const *
GetNxPowerListFromWakeList(
    _In_ NET_WAKE_SOURCE_LIST const * List
);
