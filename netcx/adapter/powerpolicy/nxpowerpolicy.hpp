// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#define NETCX_POWER_TAG 'wPxN'

void
FillBitmapParameters(
    _In_ NDIS_PM_WOL_PATTERN const & NdisWolPattern,
    _Inout_ NET_WAKE_SOURCE_BITMAP_PARAMETERS & NxBitmapParameters
);

typedef enum _NX_POWER_ENTRY_TYPE
{
    NxPowerEntryTypeWakePattern,
    NxPowerEntryTypeProtocolOffload,
} NX_POWER_ENTRY_TYPE;

typedef struct _NX_NET_POWER_ENTRY
{
    //
    // Size of the entry, including any patterns
    //
    ULONG 
        Size;

    SINGLE_LIST_ENTRY 
        ListEntry;

    SINGLE_LIST_ENTRY
        PowerListEntry;

    NETADAPTER
        Adapter;

    BOOLEAN
        Enabled;

    union
    {
        NDIS_PM_WOL_PATTERN
            NdisWoLPattern;

        NDIS_PM_PROTOCOL_OFFLOAD
            NdisProtocolOffload;
    };
    //
    // Dont add new members below the union
    //
} NX_NET_POWER_ENTRY;

class NxPowerList;

class NxPowerPolicy
{
private:

    RECORDER_LOG
        m_recorderLog = nullptr;

    //
    // Head of the Wake patterns list
    //
    SINGLE_LIST_ENTRY
        m_WakeListHead = {};

    ULONG
        m_WakeListCount = 0;

    //
    // Head of the protocol offload list
    //
    SINGLE_LIST_ENTRY
        m_ProtocolOffloadListHead = {};

    ULONG
        m_ProtocolOffloadListCount = 0;

    //
    // A local copy of the PM Parameters reported by NDIS
    //
    NDIS_PM_PARAMETERS
        m_PMParameters = {};

    NX_NET_POWER_ENTRY *
    CreateWakePatternEntry(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_PM_WOL_PATTERN * Pattern,
        _In_ UINT InformationBufferLength
    );

    void
    AddWakePatternEntryToList(
        _In_ NX_NET_POWER_ENTRY * Entry
    );

    NX_NET_POWER_ENTRY *
    RemovePowerEntryByID(
        _In_ ULONG PatternID,
        _In_ NX_POWER_ENTRY_TYPE PowerEntryType
    );

    void
    UpdatePatternEntryEnabledField(
        _In_ NX_NET_POWER_ENTRY * Entry
    );

    ULONG
    GetPowerEntryID(
        _In_ NX_NET_POWER_ENTRY * Entry,
        _In_ NX_POWER_ENTRY_TYPE EntryType
    );

public:

    NxPowerPolicy(
        _In_ RECORDER_LOG RecorderLog
    );

    ~NxPowerPolicy(
        void
    );

    void
    UpdatePowerList(
        _In_ bool IsInPowerTransition,
        _Inout_ NxPowerList * List
    );

    NX_NET_POWER_ENTRY *
    CreateProtocolOffloadEntry(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_PM_PROTOCOL_OFFLOAD * ProtocolOffload,
        _In_ UINT InformationBufferLength
    );

    NDIS_STATUS
    SetParameters(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    void
    UpdateProtocolOffloadEntryEnabledField(
        NX_NET_POWER_ENTRY * Entry
    );

    NDIS_STATUS
    AddWakePattern(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    RemoveWakePattern(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    AddProtocolOffload(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    RemoveProtocolOffload(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    RECORDER_LOG
    GetRecorderLog(
        void
    );
};
