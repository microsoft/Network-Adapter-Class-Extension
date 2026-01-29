// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"

#include <nt/rtl/integermacro.h>
#include <limits.h>
#include "NxWakeReason.hpp"

#include "NxWakeReason.tmh"

#define MAX_ETHERNET_PACKET_SIZE 1514

_Use_decl_annotations_
NxWakeReason::NxWakeReason(
    NETADAPTER Adapter
)
    : m_adapter(Adapter)
{
}

NTSTATUS
NxWakeReason::Initialize(
    void
)
{
    ULONG wakeReasonSize =
        RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8) +
        RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8) +
        RTL_NUM_ALIGN_UP(MAX_ETHERNET_PACKET_SIZE, 8);

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = m_adapter;

    auto const ntStatus = WdfMemoryCreate(
        &objectAttributes,
        NonPagedPoolNx,
        NX_WAKE_REASON_TAG,
        wakeReasonSize,
        &m_wakeReasonMemory,
        nullptr);

    if (ntStatus != STATUS_SUCCESS)
    {
        m_wakeReasonMemory = WDF_NO_HANDLE;
    }

    return ntStatus;
}

_Use_decl_annotations_
NTSTATUS
NxWakeReason::MakeWakeReasonPacket(
    NET_ADAPTER_WAKE_REASON_PACKET const * Reason,
    NDIS_PM_WAKE_REASON ** PWakeReason,
    ULONG * WakeReasonSize
) const
{
    if (Reason->PatternId == NetAdapterWakeFilterPatternId)
    {
        LogInfo(FLAG_POWER, "NETADAPTER %p reported wake. Reason: Packet matched receive filter", m_adapter);
    }
    else if (Reason->PatternId == NetAdapterWakeMagicPatternId)
    {
        LogInfo(FLAG_POWER, "NETADAPTER %p reported wake. Reason: Magic Packet", m_adapter);
    }
    else if (Reason->PatternId == NetAdapterWakeEapolPatternId)
    {
        LogInfo(FLAG_POWER, "NETADAPTER %p reported wake. Reason: EAPOL Packet", m_adapter);
    }
    else
    {
        LogInfo(FLAG_POWER, "NETADAPTER %p reported wake. Reason: Packet matched a bitmap pattern (%u)", m_adapter, Reason->PatternId);
    }

    if (Reason->WakePacket == WDF_NO_HANDLE)
    {
        LogWarning(FLAG_POWER, "NETADAPTER %p reported wake reason packet without payload", m_adapter);

        return STATUS_INVALID_PARAMETER;
    }

    size_t savedPacketSize;
    auto wakePacketBuffer = WdfMemoryGetBuffer(Reason->WakePacket, &savedPacketSize);
    if (savedPacketSize > MAX_ETHERNET_PACKET_SIZE)
    {
        savedPacketSize = MAX_ETHERNET_PACKET_SIZE;
    }

    auto const wakePacketSize =
        RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8) +
        RTL_NUM_ALIGN_UP(savedPacketSize, 8);
    *WakeReasonSize =
        static_cast<ULONG>(
            RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8) +
            wakePacketSize);

    NT_FRE_ASSERT(wakePacketSize <= ULONG_MAX);
    NT_FRE_ASSERT(*WakeReasonSize <= ULONG_MAX);

    size_t reservedWakeReasonSize;
    *PWakeReason = static_cast<NDIS_PM_WAKE_REASON*>(WdfMemoryGetBuffer(m_wakeReasonMemory, &reservedWakeReasonSize));

    NDIS_PM_WAKE_REASON * WakeReason = *PWakeReason;
    RtlZeroMemory(WakeReason, reservedWakeReasonSize);
    WakeReason->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    WakeReason->Header.Revision = NDIS_PM_WAKE_REASON_REVISION_1;
    WakeReason->Header.Size = NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1;
    WakeReason->WakeReason = NdisWakeReasonPacket;
    WakeReason->InfoBufferOffset = RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8);
    WakeReason->InfoBufferSize = static_cast<ULONG>(wakePacketSize);

    auto wakePacket = reinterpret_cast<NDIS_PM_WAKE_PACKET *>(
        reinterpret_cast<UCHAR *>(WakeReason) + RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8));
    wakePacket->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    wakePacket->Header.Revision = NDIS_PM_WAKE_PACKET_REVISION_1;
    wakePacket->Header.Size = NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1;
    wakePacket->PatternId = Reason->PatternId;
    wakePacket->OriginalPacketSize = Reason->OriginalPacketSize;
    wakePacket->SavedPacketSize = static_cast<ULONG>(savedPacketSize);
    wakePacket->SavedPacketOffset = RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8);

    UCHAR * data = reinterpret_cast<UCHAR *>(wakePacket) + RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8);
    RtlCopyMemory(data, wakePacketBuffer, savedPacketSize);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxWakeReason::MakeWakeReasonMediaChange(
    NET_IF_MEDIA_CONNECT_STATE Reason,
    NDIS_PM_WAKE_REASON * WakeReason
) const
{
    LogInfo(FLAG_POWER,
        "NETADAPTER %p reported wake. Reason: Media state change (%!NET_IF_MEDIA_CONNECT_STATE!)",
        m_adapter,
        Reason);

    WakeReason->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    WakeReason->Header.Revision = NDIS_PM_WAKE_REASON_REVISION_1;
    WakeReason->Header.Size = NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1;
    switch (Reason)
    {
        case MediaConnectStateDisconnected:
            WakeReason->WakeReason = NdisWakeReasonMediaDisconnect;
            break;

        case MediaConnectStateConnected:
            WakeReason->WakeReason = NdisWakeReasonMediaConnect;
            break;

        default:
            return STATUS_INVALID_PARAMETER;
    }

    WakeReason->InfoBufferOffset = 0;
    WakeReason->InfoBufferSize = 0;

    return STATUS_SUCCESS;
}
