// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxWakeSource.hpp"
#include "NxPowerOffload.hpp"

#define NX_POWER_OFFLOAD_LIST_SIGNATURE 'LPxN'
#define NX_WAKE_SOURCE_LIST_SIGNATURE 'SWxN'

class NxPowerList
{
public:

    ULONG const
        Signature;

    size_t
    GetCount(
        void
    ) const;

    void
    PushEntry(
        _In_ NxPowerEntry * Entry
    );

protected:

    NxPowerList(
        _In_ ULONG Signature
    );

    SINGLE_LIST_ENTRY
        m_listHead = {};

    size_t
        m_count = 0;
};

static_assert(sizeof(NxPowerList) <= sizeof(NET_POWER_OFFLOAD_LIST::Reserved));

class NxPowerOffloadList : public NxPowerList
{
public:

    NxPowerOffloadList(
        void
    ) noexcept;

    void *
    operator new(
        _In_ size_t,
        _In_ NET_POWER_OFFLOAD_LIST * NetPowerOffloadList
    );

    NxPowerOffload *
    GetPowerOffloadByIndex(
        _In_ size_t Index
    ) const;
};

static_assert(sizeof(NxPowerOffloadList) <= sizeof(NET_POWER_OFFLOAD_LIST::Reserved));

class NxWakeSourceList : public NxPowerList
{
public:

    NxWakeSourceList(
        void
    ) noexcept;

    void *
    operator new(
        _In_ size_t,
        _In_ NET_WAKE_SOURCE_LIST * NetWakeSourceList
    );

    NxWakeSource *
    GetWakeSourceByIndex(
        _In_ size_t Index
    ) const;
};

static_assert(sizeof(NxWakeSourceList) <= sizeof(NET_POWER_OFFLOAD_LIST::Reserved));

NxPowerOffloadList const *
GetNxPowerListFromOffloadList(
    _In_ NET_POWER_OFFLOAD_LIST const * List
);

NxWakeSourceList const *
GetNxPowerListFromWakeList(
    _In_ NET_WAKE_SOURCE_LIST const * List
);
