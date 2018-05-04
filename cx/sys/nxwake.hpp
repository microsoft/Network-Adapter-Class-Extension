// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    NxWake and NxWake objects.

--*/

#pragma once

typedef enum _NX_POWER_ENTRY_TYPE {
    NxPowerEntryTypeWakePattern,
    NxPowerEntryTypeProtocolOffload,
} NX_POWER_ENTRY_TYPE;

typedef struct _NX_NET_POWER_ENTRY {
    //
    // Size of the entry, including any patterns
    //
    ULONG               Size;
    SINGLE_LIST_ENTRY   ListEntry;
    BOOLEAN             Enabled;

    union
    {
        NDIS_PM_WOL_PATTERN         NdisWoLPattern;
        NDIS_PM_PROTOCOL_OFFLOAD    NdisProtocolOffload;
    };
    //
    // Dont add new members below the union
    //
} NX_NET_POWER_ENTRY, *PNX_NET_POWER_ENTRY;

PNxWake
FORCEINLINE
GetNxWakeFromHandle(
    _In_ NETPOWERSETTINGS WakeListWdfHandle
    );

typedef class NxWake *PNxWake;
class NxWake : public CFxObject<NETPOWERSETTINGS,
                                    NxWake,
                                    GetNxWakeFromHandle,
                                    false>
{
private:
    //
    // Pointer to the corresponding NxAdapter object
    //
    PNxAdapter                   m_NxAdapter;

    //
    // Head of the Wake patterns list
    //
    SINGLE_LIST_ENTRY            m_WakeListHead;
    ULONG                        m_WakeListCount;

    //
    // Head of the protocol offload list
    //
    SINGLE_LIST_ENTRY            m_ProtocolOffloadListHead;
    ULONG                        m_ProtocolOffloadListCount;

    //
    // A local copy of the PM Parameters reported by NDIS
    //
    NDIS_PM_PARAMETERS           m_PMParameters;

    //
    // Driver supplied callback to filter an incoming Wake Pattern
    //
    PFN_NET_ADAPTER_PREVIEW_WAKE_PATTERN m_EvtPreviewWakePattern;

    //
    // Driver supplied callback to filter an incoming protocol offload
    //
    PFN_NET_ADAPTER_PREVIEW_PROTOCOL_OFFLOAD m_EvtPreviewProtocolOffload;

    //
    // Driver supplied cleanup and destory callbacks.
    //
    PFN_WDF_OBJECT_CONTEXT_CLEANUP m_EvtCleanupCallback;
    PFN_WDF_OBJECT_CONTEXT_DESTROY m_EvtDestroyCallback;

    //
    // Object cleanup/destory callbacks remain disabled until NxAdapter
    // initialization is completed to the point of no more failures.
    // That way driver doesnt have to deal with a callback on its
    // NETPOWERSETTINGS if NetAdapterCreate itself failed.
    //
    BOOLEAN                        m_DriverObjectCallbacksEnabled;

    //
    // Controls when a driver is allowed to access the wake settings
    // Protects the settings from being accessed while they're in a window
    // where they can be modified
    //
    BOOLEAN                        m_DriverCanAccessWakeSettings;

    NxWake(
        _In_ NETPOWERSETTINGS         NetPowerSettings,
        _In_ PNxAdapter               NxAdapter
    );

    PNX_NET_POWER_ENTRY
    CreateWakePatternEntry(
        _In_ PNDIS_PM_WOL_PATTERN Pattern,
        _In_ UINT                 InformationBufferLength
    );

    VOID
    AddWakePatternEntryToList(
        _In_ PNX_NET_POWER_ENTRY Entry
    );

    PNX_NET_POWER_ENTRY
    RemovePowerEntryByID(
        _In_ ULONG PatternID,
        _In_ NX_POWER_ENTRY_TYPE NxPowerEntryType
    );

    VOID
    UpdatePatternEntryEnabledField(
        _In_ PNX_NET_POWER_ENTRY Entry
    );

    static
    VOID
    _EvtDestroyCallbackWrapper(
        _In_ WDFOBJECT Object
    );

    static
    VOID
    _EvtCleanupCallbackWrapper(
        _In_ WDFOBJECT Object
    );

    ULONG
    GetPowerEntryID(
        _In_ PNX_NET_POWER_ENTRY NxEntry,
        _In_ NX_POWER_ENTRY_TYPE NxEntryType
        )
    {
        if (NxEntryType == NxPowerEntryTypeWakePattern) {
            return NxEntry->NdisWoLPattern.PatternId;
        }
        else if (NxEntryType == NxPowerEntryTypeProtocolOffload) {
            return NxEntry->NdisProtocolOffload.ProtocolOffloadId;
        }
        else {
            NT_ASSERTMSG("Invalid NxEntryType", 0);
            return ULONG_MAX;
        }
    }

public:

    ~NxWake();

    static
    NTSTATUS
    _Create(
        _In_   PNxAdapter               NxAdapter,
        _In_   PWDF_OBJECT_ATTRIBUTES   NetPowerSettingsObjectAttributes,
        _Out_  PNxWake*                 NxWakeObject
        );

    PNX_NET_POWER_ENTRY
    CreateProtocolOffloadEntry(
        _In_ PNDIS_PM_PROTOCOL_OFFLOAD ProtocolOffload,
        _In_ UINT InformationBufferLength);

    NDIS_STATUS
    ProcessOidPmParameters(
        _In_ PNDIS_PM_PARAMETERS PmParams
        );

    VOID UpdateProtocolOffloadEntryEnabledField(
        PNX_NET_POWER_ENTRY Entry
        );

    NDIS_STATUS
    ProcessOidWakePattern(
        _In_ NETADAPTER AdapterWdfHandle,
        _In_ _NDIS_OID_REQUEST::_REQUEST_DATA::_SET *Set
        );

    NDIS_STATUS
    ProcessOidProtocolOffload(
        _In_ NETADAPTER AdapterWdfHandle,
        _In_ _NDIS_OID_REQUEST::_REQUEST_DATA::_SET *Set
        );

    RECORDER_LOG
    GetRecorderLog() {
        return m_NxAdapter->GetRecorderLog();
    }

    VOID
    AdapterInitComplete(
        VOID
    );

    ULONG
    GetEnabledWakeUpFlags(
        VOID
    )
    {
        return m_PMParameters.WakeUpFlags;
    }

    ULONG
    GetEnabledMediaSpecificWakeUpEvents(
        VOID
    )
    {
        return m_PMParameters.MediaSpecificWakeUpEvents;
    }

    ULONG
    GetEnabledProtocolOffloads(
        VOID
    )
    {
        return m_PMParameters.EnabledProtocolOffloads;
    }

    ULONG
    GetEnabledWakePacketPatterns(
        VOID
    )
    {
        return m_PMParameters.EnabledWoLPacketPatterns;
    }

    ULONG
    GetProtocolOffloadCount(
        VOID
    )
    {
        return m_ProtocolOffloadListCount;
    }

    ULONG
    GetWakePatternCount(
        VOID
    )
    {
        return m_WakeListCount;
    }

    PNX_NET_POWER_ENTRY
    GetEntryAtIndex(
        _In_ ULONG Index,
        _In_ NX_POWER_ENTRY_TYPE Type
    );

    VOID
    SetPowerPreviewEvtCallbacks(
        _In_ PFN_NET_ADAPTER_PREVIEW_WAKE_PATTERN WakePatternCallback,
        _In_ PFN_NET_ADAPTER_PREVIEW_PROTOCOL_OFFLOAD ProtocolOffloadCallback
    )
    {
        m_EvtPreviewWakePattern = WakePatternCallback;
        m_EvtPreviewProtocolOffload = ProtocolOffloadCallback;
    }

    BOOLEAN
    ArePowerSettingsAccessible(
        VOID
    );

    ULONG
    GetWakePatternCountForType(
        _In_ NDIS_PM_WOL_PACKET WakePatternType
    );

    ULONG
    GetProtocolOffloadCountForType(
        _In_ NDIS_PM_PROTOCOL_OFFLOAD_TYPE OffloadType
    );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxWake, _GetNxWakeFromHandle);

PNxWake
FORCEINLINE
GetNxWakeFromHandle(
    _In_ NETPOWERSETTINGS     NetPowerSettingsWdfHandle
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
