// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This implements the NETPOWERSETTINGS object.

--*/

#include "Nx.hpp"

#include "NxWake.tmh"
#include "NxWake.hpp"

#include "NxAdapter.hpp"
#include "NxDevice.hpp"
#include "NxMacros.hpp"
#include "version.hpp"

NxWake::NxWake(
    _In_ NETPOWERSETTINGS NetPowerSettingsWdfHandle,
    _In_ NxAdapter * NxAdapter
)
    : CFxObject(NetPowerSettingsWdfHandle)
    , m_NxAdapter(NxAdapter)
{
}

NxWake::~NxWake(
    void
)
{
    while (m_WakeListHead.Next != nullptr)
    {
        auto listEntry = PopEntryList(&m_WakeListHead);
        auto powerEntry = CONTAINING_RECORD(
            listEntry,
            NX_NET_POWER_ENTRY,
            ListEntry);

        ExFreePoolWithTag(powerEntry, NETADAPTERCX_TAG);
    }

    while (m_ProtocolOffloadListHead.Next != nullptr)
    {
        auto listEntry = PopEntryList(&m_ProtocolOffloadListHead);
        auto powerEntry = CONTAINING_RECORD(
            listEntry,
            NX_NET_POWER_ENTRY,
            ListEntry);

        ExFreePoolWithTag(powerEntry, NETADAPTERCX_TAG);
    }
}

void
NxWake::_EvtCleanupCallbackWrapper(
    _In_ WDFOBJECT Object
)
/*++
Routine Description:
    Wraps the driver supplied callback so we can disable them until
    adapter initialization is complete.
--*/
{
    auto nxWake = GetNxWakeFromHandle((NETPOWERSETTINGS)Object);

    if (nxWake->m_DriverObjectCallbacksEnabled)
    {
        if (nxWake->m_EvtCleanupCallback != nullptr)
        {
            return nxWake->m_EvtCleanupCallback(Object);
        }
    }
}

ULONG
NxWake::GetPowerEntryID(
    _In_ NX_NET_POWER_ENTRY * Entry,
    _In_ NX_POWER_ENTRY_TYPE EntryType
    )
{
    switch (EntryType)
    {
    case NxPowerEntryTypeWakePattern:
        return Entry->NdisWoLPattern.PatternId;
    case NxPowerEntryTypeProtocolOffload:
        return Entry->NdisProtocolOffload.ProtocolOffloadId;
    default:
        NT_ASSERTMSG("Invalid NxEntryType", 0);
        return ULONG_MAX;
    }
}

void
NxWake::_EvtDestroyCallbackWrapper(
    _In_ WDFOBJECT Object
)
/*++
Routine Description:
    See _EvtCleanupCallbackWrapper
--*/
{
    auto nxWake = GetNxWakeFromHandle((NETPOWERSETTINGS)Object);

    if (nxWake->m_DriverObjectCallbacksEnabled)
    {
        if (nxWake->m_EvtDestroyCallback != nullptr)
        {
            return nxWake->m_EvtDestroyCallback(Object);
        }
    }
}

NTSTATUS
NxWake::_Create(
    _In_  NxAdapter * NetAdapter,
    _In_  WDF_OBJECT_ATTRIBUTES * NetPowerSettingsObjectAttributes,
    _Out_ NxWake ** NxWakeObj
)
/*++
Routine Description:
    Static method that creates the NETWAKE object.
--*/
{
    //
    // Create a WDFOBJECT for NETPOWERSETTINGS
    //
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxWake);
    attributes.ParentObject = NetAdapter->GetFxObject();

    //
    // Ensures that the destructor is called when this object is destroyed.
    //
    NxWake::_SetObjectAttributes(&attributes);

    wil::unique_wdf_any<NETPOWERSETTINGS> netPowerSettingsWdfObj;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(
            &attributes,
            (WDFOBJECT *)netPowerSettingsWdfObj.addressof()),
        "WdfObjectCreate for NETPOWERSETTINGS failed");

    //
    // Get the NxWake allocation, which is the ctx associated with the
    // WDF object.
    //
    void * netPowerSettingsWdfObjCtx = GetNxWakeFromHandle(netPowerSettingsWdfObj.get());

    //
    // Use the inplacement new and invoke the NxWake constructor
    //
    auto nxWake = new (netPowerSettingsWdfObjCtx) NxWake(
        netPowerSettingsWdfObj.get(),
        NetAdapter);

    __analysis_assume(nxWake != nullptr);

    if (NetPowerSettingsObjectAttributes->Size != 0)
    {
        //
        // NxWake is created as part of NxAdapter creation. In case something
        // fails after this returns, we dont want the driver's object cleanup
        // or destroy callbacks to be invoked. So we swap the callbacks with
        // our wrapper and only enable these driver callbacks after adapter
        // initialization has completed successfully
        //
        nxWake->m_EvtCleanupCallback = NetPowerSettingsObjectAttributes->EvtCleanupCallback;
        nxWake->m_EvtDestroyCallback = NetPowerSettingsObjectAttributes->EvtDestroyCallback;

        NetPowerSettingsObjectAttributes->EvtCleanupCallback = NxWake::_EvtCleanupCallbackWrapper;
        NetPowerSettingsObjectAttributes->EvtDestroyCallback = NxWake::_EvtDestroyCallbackWrapper;

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfObjectAllocateContext(
                netPowerSettingsWdfObj.get(),
                NetPowerSettingsObjectAttributes,
                nullptr),
            "WdfObjectAllocateContext with NetPowerSettingsObjectAttributes failed");
    }

    //
    // The client's object attributes have been associated with the
    // wake object already. Dont introduce failures after this point.
    //
    netPowerSettingsWdfObj.release();
    *NxWakeObj = nxWake;

    return STATUS_SUCCESS;
}

NDIS_STATUS
NxWake::AddProtocolOffload(
    _In_ NETADAPTER AdapterWdfHandle,
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * SetInformation
)
/*++
Routine Description:
    Processes addition of protocol offloads. The added offload is provided
    to the driver for filtering if it has registered a preview callback to
    process it.

Returns:
    NDIS_STATUS
    Note if driver returns status other than  NDIS_STATUS_PM_PROTOCOL_OFFLOAD_LIST_FULL
    from m_EvtPreviewProtocolOffload it is treated as NDIS_STATUS_SUCCESS.

--*/
{
    ASSERT(! m_DriverCanAccessWakeSettings);

    auto ndisProtocolOffload = static_cast<NDIS_PM_PROTOCOL_OFFLOAD *>(SetInformation->InformationBuffer);

    //
    // Allocate and make a copy before invoking the callback to avoid
    // failure after driver accepts a pattern
    //
    auto powerEntry = CreateProtocolOffloadEntry(
        ndisProtocolOffload,
        SetInformation->InformationBufferLength);

    if (powerEntry == nullptr)
    {
        return NdisConvertNtStatusToNdisStatus(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Change powerEntry to see if it is enabled.
    //
    UpdateProtocolOffloadEntryEnabledField(powerEntry);

    //
    // Invoke optional callback
    //
    if (m_EvtPreviewProtocolOffload != nullptr)
    {
        m_DriverCanAccessWakeSettings = true;

        auto ntStatus = m_EvtPreviewProtocolOffload(
            AdapterWdfHandle,
            GetFxObject(),
            powerEntry->NdisProtocolOffload.ProtocolOffloadType,
            &(powerEntry->NdisProtocolOffload));

        m_DriverCanAccessWakeSettings = false;

        if (ntStatus == STATUS_NDIS_PM_PROTOCOL_OFFLOAD_LIST_FULL)
        {
            //
            // In case driver incorrectly saved a pointer to the pattern
            // help catch it sooner.
            //
            RtlFillMemory(
                powerEntry,
                powerEntry->Size,
                0xC0);

            ExFreePoolWithTag(powerEntry, NETADAPTERCX_TAG);
            return NDIS_STATUS_PM_PROTOCOL_OFFLOAD_LIST_FULL;
        }
    }

    //
    // Add it to the list
    //
    m_ProtocolOffloadListCount++;

    PushEntryList(
        &m_ProtocolOffloadListHead,
        &powerEntry->ListEntry);

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
NxWake::RemoveProtocolOffload(
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * SetInformation
)
/*++
Routine Description:
    Processes removal of protocol offloads.
--*/
{
    ASSERT(! m_DriverCanAccessWakeSettings);

    auto powerEntry = RemovePowerEntryByID(
        *((PULONG)SetInformation->InformationBuffer),
        NxPowerEntryTypeProtocolOffload);

    RtlFillMemory(
        powerEntry,
        powerEntry->Size,
        0xC0);

    ExFreePoolWithTag(powerEntry, NETADAPTERCX_TAG);

    return NDIS_STATUS_SUCCESS;
}

RECORDER_LOG
NxWake::GetRecorderLog(
    void
)
{
    return m_NxAdapter->GetRecorderLog();
}

NX_NET_POWER_ENTRY *
NxWake::CreateProtocolOffloadEntry(
    _In_ NDIS_PM_PROTOCOL_OFFLOAD * NdisProtocolOffload,
    _In_ UINT InformationBufferLength
)
/*++
Routine Description:
    Create a Nx power entry for protocol offload. Caller must free the
    allocated entry when appropriate.

Arguments:
    Pattern - Incoming PNDIS_PM_PROTOCOL_OFFLOAD
    InformationBufferLength - Length of the buffer containing the offload.
--*/
{
    ULONG ndisProtocolOffloadSize = sizeof(NDIS_PM_PROTOCOL_OFFLOAD);

    if (InformationBufferLength < ndisProtocolOffloadSize)
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Invalid InformationBufferLength %u for protocol offload entry", InformationBufferLength);
        return nullptr;
    }

    ULONG totalAllocationSize;

    NTSTATUS addStatus = RtlULongAdd(
       sizeof(NX_NET_POWER_ENTRY),
        ndisProtocolOffloadSize,
        &totalAllocationSize);

    if (!NT_SUCCESS(addStatus))
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Unable to compute size requirement for protocol offload entry %!STATUS!", addStatus);
        return nullptr;
    }

    auto powerEntry = static_cast<NX_NET_POWER_ENTRY *>(ExAllocatePoolWithTag(
        NonPagedPoolNx,
        totalAllocationSize,
        NETADAPTERCX_TAG));

    if (powerEntry == nullptr)
    {
        LogError(GetRecorderLog(), FLAG_POWER, "Allocation for Nx power entry failed");
        return nullptr;
    }

    RtlZeroMemory(powerEntry, totalAllocationSize);
    powerEntry->Size = totalAllocationSize;

    RtlCopyMemory(
        &powerEntry->NdisProtocolOffload,
        NdisProtocolOffload,
        ndisProtocolOffloadSize);

    return powerEntry;
}

NX_NET_POWER_ENTRY *
NxWake::CreateWakePatternEntry(
    _In_ NDIS_PM_WOL_PATTERN * Pattern,
    _In_ UINT InformationBufferLength
)
/*++
Routine Description:
    Create a Nx power pattern entry after taking into account pattern size
    requirements. Caller must free the allocated entry when appropriate.

Arguments:
    Pattern - Incoming PNDIS_PM_WOL_PATTERN
    InformationBufferLength - Length of the buffer containing the Pattern.
--*/
{
    ULONG patternPayloadSize = 0;

    if (Pattern->WoLPacketType == NdisPMWoLPacketBitmapPattern)
    {
        patternPayloadSize =
            max(Pattern->WoLPattern.WoLBitMapPattern.MaskOffset + Pattern->WoLPattern.WoLBitMapPattern.MaskSize,
                Pattern->WoLPattern.WoLBitMapPattern.PatternOffset + Pattern->WoLPattern.WoLBitMapPattern.PatternSize);

        patternPayloadSize = max(patternPayloadSize, sizeof(NDIS_PM_WOL_PATTERN));
        patternPayloadSize = patternPayloadSize - sizeof(NDIS_PM_WOL_PATTERN);
    }

    ULONG totalAllocationSize;

    NTSTATUS addStatus = RtlULongAdd(
        sizeof(NX_NET_POWER_ENTRY),
        patternPayloadSize,
        &totalAllocationSize);

    if (!NT_SUCCESS(addStatus))
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Unable to compute size requirement for WoL entry %!STATUS!", addStatus);
        return nullptr;
    }

    if (InformationBufferLength < (patternPayloadSize + sizeof(NDIS_PM_WOL_PATTERN)))
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Invalid InformationBufferLength %u for WoL entry", InformationBufferLength);
        return nullptr;
    }

    auto powerEntry = static_cast<NX_NET_POWER_ENTRY *>(ExAllocatePoolWithTag(
        NonPagedPoolNx,
        totalAllocationSize,
        NETADAPTERCX_TAG));

    if (powerEntry == nullptr)
    {
        LogError(GetRecorderLog(), FLAG_POWER, "Allocation for Nx power entry failed");
        return nullptr;
    }

    RtlZeroMemory(powerEntry, totalAllocationSize);
    powerEntry->Size = totalAllocationSize;

    RtlCopyMemory(
        &powerEntry->NdisWoLPattern,
        Pattern,
        sizeof(NDIS_PM_WOL_PATTERN)+patternPayloadSize);

    return powerEntry;
}

NDIS_STATUS
NxWake::AddWakePattern(
    _In_ NETADAPTER AdapterWdfHandle,
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * SetInformation
)
/*++
Routine Description:
    Processes addition of Wake patterns. The pattern is presented
    to the driver if it has registered a callback to process it.

Returns:
    NDIS_STATUS
    Note if driver returns status other than  STATUS_NDIS_PM_WOL_PATTERN_LIST_FULL
    from m_EvtPreviewWakePattern it is treated as NDIS_STATUS_SUCCESS.

--*/
{
    ASSERT (! m_DriverCanAccessWakeSettings);

    auto & device = *GetNxDeviceFromHandle(m_NxAdapter->GetDevice());

    if (!device.IncreaseWakePatternReference())
    {
        LogInfo(
            GetRecorderLog(),
            FLAG_DEVICE,
            "Rejecting wake pattern because the maximum number of patterns was reached. NETADAPTER=%p",
            m_NxAdapter->GetFxObject());

        return NDIS_STATUS_PM_WOL_PATTERN_LIST_FULL;
    }

    // Make sure we remove the wake pattern reference from the device if something goes wrong
    auto wakePatternReference = wil::scope_exit([&device]() { device.DecreaseWakePatternReference(); });

    auto ndisWolPattern = static_cast<NDIS_PM_WOL_PATTERN *>(SetInformation->InformationBuffer);

    //
    // Allocate and make a copy before invoking the callback to avoid
    // failure after driver accepts a pattern
    //
    auto powerEntry = CreateWakePatternEntry(
        ndisWolPattern,
        SetInformation->InformationBufferLength);

    if (powerEntry == nullptr)
    {
        return NdisConvertNtStatusToNdisStatus(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Change powerEntry to see if it is enabled..
    //
    UpdatePatternEntryEnabledField(powerEntry);

    //
    // Invoke optional callback
    //
    if (m_EvtPreviewWakePattern != nullptr)
    {
        m_DriverCanAccessWakeSettings = true;

        NTSTATUS ntStatus = m_EvtPreviewWakePattern(
            AdapterWdfHandle,
            GetFxObject(),
            powerEntry->NdisWoLPattern.WoLPacketType,
            &(powerEntry->NdisWoLPattern));

        m_DriverCanAccessWakeSettings = false;

        if (ntStatus == STATUS_NDIS_PM_WOL_PATTERN_LIST_FULL)
        {
            //
            // In case driver incorrectly saved a pointer to the pattern
            // help catch it sooner.
            //
            RtlFillMemory(
                powerEntry,
                powerEntry->Size,
                0xC0);

            ExFreePoolWithTag(powerEntry, NETADAPTERCX_TAG);

            return NDIS_STATUS_PM_WOL_PATTERN_LIST_FULL;
        }
    }

    AddWakePatternEntryToList(powerEntry);

    wakePatternReference.release();

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
NxWake::RemoveWakePattern(
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * SetInformation
)
/*++
Routine Description:
    Processes removal of Wake patterns.
--*/
{
    ASSERT(! m_DriverCanAccessWakeSettings);

    auto powerEntry = RemovePowerEntryByID(
        *((PULONG)SetInformation->InformationBuffer),
        NxPowerEntryTypeWakePattern);

    RtlFillMemory(
        powerEntry,
        powerEntry->Size,
        0xC0);

    ExFreePoolWithTag(powerEntry, NETADAPTERCX_TAG);

    auto & device = *GetNxDeviceFromHandle(m_NxAdapter->GetDevice());
    device.DecreaseWakePatternReference();

    return NDIS_STATUS_SUCCESS;
}

void
NxWake::AddWakePatternEntryToList(
    _In_ NX_NET_POWER_ENTRY * Entry
)
/*++
Routine Description:
    Adds an entry to the list and increments count.
--*/
{
    m_WakeListCount++;
    PushEntryList(&m_WakeListHead, &Entry->ListEntry);
}

NX_NET_POWER_ENTRY *
NxWake::RemovePowerEntryByID(
    _In_ ULONG PatternID,
    _In_ NX_POWER_ENTRY_TYPE PowerEntryType
)
/*++
Routine Description:
    Iterates through the list of nx power entries and pops the entry that matches
    the PatternID. NDIS guarantees that the PatternID for a given power entry type
    are unique for each miniport.

Arguments:
    PatternID - Unique identifier of the pattern.
    PowerEntryType - Type to be removed. Wake pattern or protocol offload

Returns:
    Wake entry if found. NULL if not found.

--*/
{
    SINGLE_LIST_ENTRY * prevEntry = nullptr;
    ULONG * countToDecrement = nullptr;

    if (PowerEntryType == NxPowerEntryTypeWakePattern)
    {
        prevEntry = &m_WakeListHead;
        countToDecrement = &m_WakeListCount;
        ASSERT(m_WakeListCount != 0);
    }
    else if (PowerEntryType == NxPowerEntryTypeProtocolOffload)
    {
        prevEntry = &m_ProtocolOffloadListHead;
        countToDecrement = &m_ProtocolOffloadListCount;
        ASSERT(m_ProtocolOffloadListCount != 0);
    }
    else 
    {
        NT_ASSERTMSG("Invalid NxEntryType", 0);
    }

    auto listEntry = prevEntry->Next;

    while (listEntry != nullptr)
    {
        auto powerEntry = CONTAINING_RECORD(
            listEntry,
            NX_NET_POWER_ENTRY,
            ListEntry);

        if (GetPowerEntryID(powerEntry, PowerEntryType) == PatternID)
        {
            (*countToDecrement)--;
            PopEntryList(prevEntry);
            return powerEntry;
        }
        prevEntry = listEntry;
        listEntry = listEntry->Next;
    }

    return nullptr;
}

NDIS_STATUS
NxWake::SetParameters(
    _In_ NDIS_PM_PARAMETERS * PmParams
)
/*++
Routine Description:
    Stores the incoming the NDIS_PM_PARAMETERS and if the Wake pattern
    have changed then it updates the Wake patterns to reflect the change
--*/
{
    bool updateWakePatterns = true;
    bool updateProtocolOffload = true;

    ASSERT (m_DriverCanAccessWakeSettings == false);

    if (m_PMParameters.EnabledWoLPacketPatterns == PmParams->EnabledWoLPacketPatterns)
    {
        updateWakePatterns = false;
    }

    if (m_PMParameters.EnabledProtocolOffloads == PmParams->EnabledProtocolOffloads)
    {
        updateProtocolOffload = false;
    }

    RtlCopyMemory(
        &m_PMParameters,
        PmParams,
        sizeof(m_PMParameters));

    if (updateWakePatterns)
    {
        auto listEntry = m_WakeListHead.Next;
        while (listEntry != nullptr)
        {
            auto powerEntry = CONTAINING_RECORD(
                listEntry,
                NX_NET_POWER_ENTRY,
                ListEntry);

            UpdatePatternEntryEnabledField(powerEntry);
            listEntry = listEntry->Next;
        }
    }

    if (updateProtocolOffload)
    {
        auto listEntry = m_ProtocolOffloadListHead.Next;
        while (listEntry != nullptr)
        {
            auto powerEntry = CONTAINING_RECORD(
                listEntry,
                NX_NET_POWER_ENTRY,
                ListEntry);

            UpdateProtocolOffloadEntryEnabledField(powerEntry);
            listEntry = listEntry->Next;
        }
    }

    return NDIS_STATUS_SUCCESS;
}

void
NxWake::UpdateProtocolOffloadEntryEnabledField(
    _In_ NX_NET_POWER_ENTRY * Entry
)
/*++
Routine Description:
    Updates the protocol offload entry's Enabled field based on the
    NDIS_PM_PARAMETERS.
--*/
{
    Entry->Enabled = FALSE;

    switch (Entry->NdisProtocolOffload.ProtocolOffloadType)
    {
    case NdisPMProtocolOffloadIdIPv4ARP:
        Entry->Enabled = !!(m_PMParameters.EnabledProtocolOffloads & NDIS_PM_PROTOCOL_OFFLOAD_ARP_ENABLED);
        break;
    case NdisPMProtocolOffloadIdIPv6NS:
        Entry->Enabled = !!(m_PMParameters.EnabledProtocolOffloads & NDIS_PM_PROTOCOL_OFFLOAD_NS_ENABLED);
        break;
    case NdisPMProtocolOffload80211RSNRekey:
        Entry->Enabled = !!(m_PMParameters.EnabledProtocolOffloads & NDIS_PM_PROTOCOL_OFFLOAD_80211_RSN_REKEY_ENABLED);
        break;
    default:
        NT_ASSERTMSG("Unexpected protocol offload type", 0);
        break;
    }
}

void
NxWake::UpdatePatternEntryEnabledField(
    _In_ NX_NET_POWER_ENTRY * Entry
)
/*++
Routine Description:
    Updates the Wake entry's Enabled field based on the NDIS_PM_PARAMETERS.
--*/
{
    Entry->Enabled = FALSE;

    switch (Entry->NdisWoLPattern.WoLPacketType)
    {
    case NdisPMWoLPacketBitmapPattern:
        if (m_PMParameters.EnabledWoLPacketPatterns & NDIS_PM_WOL_BITMAP_PATTERN_ENABLED)
        {
            Entry->Enabled = TRUE;
        }
        break;
    case NdisPMWoLPacketMagicPacket:
        if (m_PMParameters.EnabledWoLPacketPatterns & NDIS_PM_WOL_MAGIC_PACKET_ENABLED)
        {
            Entry->Enabled = TRUE;
        }
        break;
    case NdisPMWoLPacketIPv4TcpSyn:
        if (m_PMParameters.EnabledWoLPacketPatterns & NDIS_PM_WOL_IPV4_TCP_SYN_ENABLED)
        {
            Entry->Enabled = TRUE;
        }
        break;
    case NdisPMWoLPacketIPv6TcpSyn:
        if (m_PMParameters.EnabledWoLPacketPatterns & NDIS_PM_WOL_IPV6_TCP_SYN_ENABLED)
        {
            Entry->Enabled = TRUE;
        }
        break;
    case NdisPMWoLPacketEapolRequestIdMessage:
        if (m_PMParameters.EnabledWoLPacketPatterns & NDIS_PM_WOL_EAPOL_REQUEST_ID_MESSAGE_ENABLED)
        {
            Entry->Enabled = TRUE;
        }
        break;
    default:
        ASSERT(FALSE);
        break;
    }
}

NX_NET_POWER_ENTRY *
NxWake::GetEntryAtIndex(
    _In_ ULONG Index,
    _In_ NX_POWER_ENTRY_TYPE PowerEntryType
)
/*++
Routine Description:
    Returns the NX_NET_POWER_ENTRY at index Index.

Arguments:
    Index - 0 based index into the list.

Returns:
    PNX_NET_POWER_ENTRY if found. NULL otherwise.

--*/
{
    PSINGLE_LIST_ENTRY listHead;
    ULONG maxCount;

    ASSERT(
        PowerEntryType == NxPowerEntryTypeProtocolOffload ||
        PowerEntryType == NxPowerEntryTypeWakePattern);

    if (PowerEntryType == NxPowerEntryTypeWakePattern)
    {
        maxCount = m_WakeListCount;
        listHead = &m_WakeListHead;
    }
    else
    {
        maxCount = m_ProtocolOffloadListCount;
        listHead = &m_ProtocolOffloadListHead;
    }

    if (Index >= maxCount)
    {
        return nullptr;
    }

    ULONG i;
    PSINGLE_LIST_ENTRY ple;

    for (i = 0, ple = listHead->Next;
        ple != nullptr;
        ple = ple->Next, i++)
    {
        if (i != Index)
        {
            continue;
        }

        return CONTAINING_RECORD(
            ple,
            NX_NET_POWER_ENTRY,
            ListEntry);
    }

    return nullptr;
}

void
NxWake::AdapterInitComplete(
    void
)
/*++
Routine Description:
    Notification from NxAdapter that initialization is complete to the point of
    no more failures and it is time to enable any driver provided NETPOWERSETTINGS
    cleanup/destroy callbacks.
--*/
{
    ASSERT(m_DriverObjectCallbacksEnabled == false);
    m_DriverObjectCallbacksEnabled = true;
}

ULONG
NxWake::GetEnabledWakeUpFlags(
    void
)
{
    return m_PMParameters.WakeUpFlags;
}

ULONG
NxWake::GetEnabledMediaSpecificWakeUpEvents(
    void
)
{
    return m_PMParameters.MediaSpecificWakeUpEvents;
}

ULONG
NxWake::GetEnabledProtocolOffloads(
    void
)
{
    return m_PMParameters.EnabledProtocolOffloads;
}

ULONG
NxWake::GetEnabledWakePacketPatterns(
    void
)
{
    return m_PMParameters.EnabledWoLPacketPatterns;
}

ULONG
NxWake::GetProtocolOffloadCount(
    void
)
{
    return m_ProtocolOffloadListCount;
}

ULONG
NxWake::GetWakePatternCount(
    void
)
{
    return m_WakeListCount;
}

bool
NxWake::ArePowerSettingsAccessible(
    void
)
/*++
Routine Description:
    Checks whether the Power Settings are Accessible
--*/
{
    auto wdfDevice = m_NxAdapter->GetDevice();
    auto nxDevice = GetNxDeviceFromHandle(wdfDevice);
    return (m_DriverCanAccessWakeSettings || nxDevice->IsDeviceInPowerTransition());
}

void
NxWake::SetPowerPreviewEvtCallbacks(
    _In_ PFN_NET_ADAPTER_PREVIEW_WAKE_PATTERN WakePatternCallback,
    _In_ PFN_NET_ADAPTER_PREVIEW_PROTOCOL_OFFLOAD ProtocolOffloadCallback
)
{
    m_EvtPreviewWakePattern = WakePatternCallback;
    m_EvtPreviewProtocolOffload = ProtocolOffloadCallback;
}
