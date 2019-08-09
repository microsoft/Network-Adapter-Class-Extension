// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NxPowerPolicy.tmh"
#include "NxPowerPolicy.hpp"

#include "NxPowerList.hpp"

void
FillBitmapParameters(
    _In_ NDIS_PM_WOL_PATTERN const & NdisWolPattern,
    _Inout_ NET_WAKE_SOURCE_BITMAP_PARAMETERS & NxBitmapParameters
)
{
    auto const basePointer = reinterpret_cast<unsigned char const *>(&NdisWolPattern);

    NxBitmapParameters.Id = NdisWolPattern.PatternId;
    NxBitmapParameters.MaskSize = NdisWolPattern.WoLPattern.WoLBitMapPattern.MaskSize;
    NxBitmapParameters.Mask = basePointer + NdisWolPattern.WoLPattern.WoLBitMapPattern.MaskOffset;
    NxBitmapParameters.PatternSize = NdisWolPattern.WoLPattern.WoLBitMapPattern.PatternSize;
    NxBitmapParameters.Pattern = basePointer + NdisWolPattern.WoLPattern.WoLBitMapPattern.PatternOffset;
}

NxPowerPolicy::NxPowerPolicy(
    _In_ RECORDER_LOG RecorderLog
)
    : m_recorderLog(RecorderLog)
{
}

NxPowerPolicy::~NxPowerPolicy(
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

        ExFreePoolWithTag(powerEntry, NETCX_POWER_TAG);
    }

    while (m_ProtocolOffloadListHead.Next != nullptr)
    {
        auto listEntry = PopEntryList(&m_ProtocolOffloadListHead);
        auto powerEntry = CONTAINING_RECORD(
            listEntry,
            NX_NET_POWER_ENTRY,
            ListEntry);

        ExFreePoolWithTag(powerEntry, NETCX_POWER_TAG);
    }
}

ULONG
NxPowerPolicy::GetPowerEntryID(
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

_Use_decl_annotations_
void
NxPowerPolicy::UpdatePowerList(
    bool IsInPowerTransition,
    NxPowerList * List
)
{
    auto listHead =
        List->EntryType == NxPowerEntryTypeProtocolOffload
            ? &m_ProtocolOffloadListHead
            : &m_WakeListHead;

    for (auto link = listHead->Next; link != nullptr; link = link->Next)
    {
        auto entry = CONTAINING_RECORD(
            link,
            NX_NET_POWER_ENTRY,
            ListEntry);

        auto const addToList = !IsInPowerTransition || entry->Enabled;

        if (addToList)
        {
            List->PushEntry(entry);
        }
    }
}

NDIS_STATUS
NxPowerPolicy::AddProtocolOffload(
    _In_ NETADAPTER Adapter,
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * SetInformation
)
{
    if (SetInformation->InformationBufferLength < sizeof(NDIS_PM_PROTOCOL_OFFLOAD))
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Invalid InformationBufferLength (%u) for OID_PM_ADD_PROTOCOL_OFFLOAD", SetInformation->InformationBufferLength);

        return NDIS_STATUS_INVALID_PARAMETER;
    }

    auto ndisProtocolOffload = static_cast<NDIS_PM_PROTOCOL_OFFLOAD *>(SetInformation->InformationBuffer);

    //
    // Allocate and make a copy before invoking the callback to avoid
    // failure after driver accepts a pattern
    //
    auto powerEntry = CreateProtocolOffloadEntry(
        Adapter,
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
    // Add it to the list
    //
    m_ProtocolOffloadListCount++;

    PushEntryList(
        &m_ProtocolOffloadListHead,
        &powerEntry->ListEntry);

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
NxPowerPolicy::RemoveProtocolOffload(
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * SetInformation
)
/*++
Routine Description:
    Processes removal of protocol offloads.
--*/
{
    if (SetInformation->InformationBufferLength < sizeof(ULONG))
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Invalid InformationBufferLength (%u) for OID_PM_REMOVE_PROTOCOL_OFFLOAD", SetInformation->InformationBufferLength);

        return NDIS_STATUS_INVALID_PARAMETER;
    }

    auto powerEntry = RemovePowerEntryByID(
        *((PULONG)SetInformation->InformationBuffer),
        NxPowerEntryTypeProtocolOffload);

    RtlFillMemory(
        powerEntry,
        powerEntry->Size,
        0xC0);

    ExFreePoolWithTag(powerEntry, NETCX_POWER_TAG);

    return NDIS_STATUS_SUCCESS;
}

RECORDER_LOG
NxPowerPolicy::GetRecorderLog(
    void
)
{
    return m_recorderLog;
}

NX_NET_POWER_ENTRY *
NxPowerPolicy::CreateProtocolOffloadEntry(
    _In_ NETADAPTER Adapter,
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
        NETCX_POWER_TAG));

    if (powerEntry == nullptr)
    {
        LogError(GetRecorderLog(), FLAG_POWER, "Allocation for Nx power entry failed");
        return nullptr;
    }

    RtlZeroMemory(powerEntry, totalAllocationSize);
    powerEntry->Size = totalAllocationSize;
    powerEntry->Adapter = Adapter;

    RtlCopyMemory(
        &powerEntry->NdisProtocolOffload,
        NdisProtocolOffload,
        ndisProtocolOffloadSize);

    return powerEntry;
}

NX_NET_POWER_ENTRY *
NxPowerPolicy::CreateWakePatternEntry(
    _In_ NETADAPTER Adapter,
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
        NETCX_POWER_TAG));

    if (powerEntry == nullptr)
    {
        LogError(GetRecorderLog(), FLAG_POWER, "Allocation for Nx power entry failed");
        return nullptr;
    }

    RtlZeroMemory(powerEntry, totalAllocationSize);
    powerEntry->Size = totalAllocationSize;
    powerEntry->Adapter = Adapter;

    RtlCopyMemory(
        &powerEntry->NdisWoLPattern,
        Pattern,
        sizeof(NDIS_PM_WOL_PATTERN)+patternPayloadSize);

    return powerEntry;
}

NDIS_STATUS
NxPowerPolicy::AddWakePattern(
    _In_ NETADAPTER Adapter,
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * SetInformation
)
{
    if (SetInformation->InformationBufferLength < sizeof(NDIS_PM_WOL_PATTERN))
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Invalid InformationBufferLength (%u) for OID_PM_ADD_WOL_PATTERN", SetInformation->InformationBufferLength);

        return NDIS_STATUS_INVALID_PARAMETER;
    }

    auto ndisWolPattern = static_cast<NDIS_PM_WOL_PATTERN *>(SetInformation->InformationBuffer);

    //
    // Allocate and make a copy before invoking the callback to avoid
    // failure after driver accepts a pattern
    //
    auto powerEntry = CreateWakePatternEntry(
        Adapter,
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

    AddWakePatternEntryToList(powerEntry);

    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
NxPowerPolicy::RemoveWakePattern(
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * SetInformation
)
/*++
Routine Description:
    Processes removal of Wake patterns.
--*/
{
    if (SetInformation->InformationBufferLength < sizeof(ULONG))
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Invalid InformationBufferLength (%u) for OID_PM_REMOVE_WOL_PATTERN", SetInformation->InformationBufferLength);

        return NDIS_STATUS_INVALID_PARAMETER;
    }

    auto powerEntry = RemovePowerEntryByID(
        *((PULONG)SetInformation->InformationBuffer),
        NxPowerEntryTypeWakePattern);

    RtlFillMemory(
        powerEntry,
        powerEntry->Size,
        0xC0);

    ExFreePoolWithTag(powerEntry, NETCX_POWER_TAG);

    return NDIS_STATUS_SUCCESS;
}

void
NxPowerPolicy::AddWakePatternEntryToList(
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
NxPowerPolicy::RemovePowerEntryByID(
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
NxPowerPolicy::SetParameters(
    _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
)
/*++
Routine Description:
    Stores the incoming the NDIS_PM_PARAMETERS and if the Wake pattern
    have changed then it updates the Wake patterns to reflect the change
--*/
{
    if (Set->InformationBufferLength < sizeof(NDIS_PM_PARAMETERS))
    {
        LogError(GetRecorderLog(), FLAG_POWER,
            "Invalid InformationBufferLength (%u) for OID_PM_PARAMETERS", Set->InformationBufferLength);

        return NDIS_STATUS_INVALID_PARAMETER;
    }

    auto const PmParams = static_cast<NDIS_PM_PARAMETERS *>(Set->InformationBuffer);

    bool updateWakePatterns = true;
    bool updateProtocolOffload = true;

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
NxPowerPolicy::UpdateProtocolOffloadEntryEnabledField(
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
    case NdisPMProtocolOffload80211RSNRekeyV2:
        Entry->Enabled = !!(m_PMParameters.EnabledProtocolOffloads & NDIS_PM_PROTOCOL_OFFLOAD_80211_RSN_REKEY_ENABLED);
        break;
    default:
        NT_ASSERTMSG("Unexpected protocol offload type", 0);
        break;
    }
}

void
NxPowerPolicy::UpdatePatternEntryEnabledField(
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
