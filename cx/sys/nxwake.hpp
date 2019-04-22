// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    NxWake and NxWake objects.

--*/

#pragma once

#include <FxObjectBase.hpp>

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
} NX_NET_POWER_ENTRY, *PNX_NET_POWER_ENTRY;

class NxAdapter;
class NxWake;

FORCEINLINE
NxWake *
GetNxWakeFromHandle(
    _In_ NETPOWERSETTINGS WakeListWdfHandle
);

class NxWake
    : public CFxObject<
        NETPOWERSETTINGS,
        NxWake,
        GetNxWakeFromHandle,
        false>
{
private:
    //
    // Pointer to the corresponding NxAdapter object
    //
    NxAdapter *
        m_NxAdapter = nullptr;

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

    //
    // Driver supplied callback to filter an incoming Wake Pattern
    //
    PFN_NET_ADAPTER_PREVIEW_WAKE_PATTERN
        m_EvtPreviewWakePattern = nullptr;

    //
    // Driver supplied callback to filter an incoming protocol offload
    //
    PFN_NET_ADAPTER_PREVIEW_PROTOCOL_OFFLOAD
        m_EvtPreviewProtocolOffload = nullptr;

    //
    // Driver supplied cleanup and destory callbacks.
    //
    PFN_WDF_OBJECT_CONTEXT_CLEANUP
        m_EvtCleanupCallback = nullptr;

    PFN_WDF_OBJECT_CONTEXT_DESTROY
        m_EvtDestroyCallback = nullptr;

    //
    // Object cleanup/destory callbacks remain disabled until NxAdapter
    // initialization is completed to the point of no more failures.
    // That way driver doesnt have to deal with a callback on its
    // NETPOWERSETTINGS if NetAdapterCreate itself failed.
    //
    bool
        m_DriverObjectCallbacksEnabled = false;

    //
    // Controls when a driver is allowed to access the wake settings
    // Protects the settings from being accessed while they're in a window
    // where they can be modified
    //
    bool
        m_DriverCanAccessWakeSettings = false;

    NxWake(
        _In_ NETPOWERSETTINGS NetPowerSettings,
        _In_ NxAdapter * NxAdapter
    );

    NX_NET_POWER_ENTRY *
    CreateWakePatternEntry(
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

    static
    void
    _EvtDestroyCallbackWrapper(
        _In_ WDFOBJECT Object
    );

    static
    void
    _EvtCleanupCallbackWrapper(
        _In_ WDFOBJECT Object
    );

    ULONG
    GetPowerEntryID(
        _In_ NX_NET_POWER_ENTRY * Entry,
        _In_ NX_POWER_ENTRY_TYPE EntryType
    );

public:

    ~NxWake(
        void
    );

    static
    NTSTATUS
    _Create(
        _In_ NxAdapter * NxAdapter,
        _In_ WDF_OBJECT_ATTRIBUTES * NetPowerSettingsObjectAttributes,
        _Out_ NxWake ** NxWakeObject
    );

    NX_NET_POWER_ENTRY *
    CreateProtocolOffloadEntry(
        _In_ NDIS_PM_PROTOCOL_OFFLOAD * ProtocolOffload,
        _In_ UINT InformationBufferLength
    );

    NDIS_STATUS
    SetParameters(
        _In_ NDIS_PM_PARAMETERS * PmParams
    );

    void
    UpdateProtocolOffloadEntryEnabledField(
        NX_NET_POWER_ENTRY * Entry
    );

    NDIS_STATUS
    AddWakePattern(
        _In_ NETADAPTER AdapterWdfHandle,
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    RemoveWakePattern(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    AddProtocolOffload(
        _In_ NETADAPTER AdapterWdfHandle,
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

    void
    AdapterInitComplete(
        void
    );

    ULONG
    GetEnabledWakeUpFlags(
        void
    );

    ULONG
    GetEnabledMediaSpecificWakeUpEvents(
        void
    );

    ULONG
    GetEnabledProtocolOffloads(
        void
    );

    ULONG
    GetEnabledWakePacketPatterns(
        void
    );

    ULONG
    GetProtocolOffloadCount(
        void
    );

    ULONG
    GetWakePatternCount(
        void
    );

    NX_NET_POWER_ENTRY *
    GetEntryAtIndex(
        _In_ ULONG Index,
        _In_ NX_POWER_ENTRY_TYPE Type
    );

    void
    SetPowerPreviewEvtCallbacks(
        _In_ PFN_NET_ADAPTER_PREVIEW_WAKE_PATTERN WakePatternCallback,
        _In_ PFN_NET_ADAPTER_PREVIEW_PROTOCOL_OFFLOAD ProtocolOffloadCallback
    );

    bool
    ArePowerSettingsAccessible(
        void
    );
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxWake, _GetNxWakeFromHandle);

FORCEINLINE
NxWake *
GetNxWakeFromHandle(
    _In_ NETPOWERSETTINGS NetPowerSettingsWdfHandle
    )
/*++
Routine Description:

    This routine is just a wrapper around the _GetNxRequestFromHandle function.
    To be able to define a the NxRequest class above, we need a forward declaration of the
    accessor function. Since _GetNxRequestFromHandle is defined by Wdf, we dont want to
    assume a prototype of that function for the foward declaration.

--*/
{
    return _GetNxWakeFromHandle(NetPowerSettingsWdfHandle);
}

