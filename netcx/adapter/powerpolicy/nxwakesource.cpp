// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NxWakeSource.hpp"

#include "NxWakeSource.tmh"

_Use_decl_annotations_
NxWakeSource::NxWakeSource(
    NET_WAKE_SOURCE_TYPE Type,
    NETADAPTER Adapter
)
    : NxPowerEntry(Adapter)
    , m_type(Type)
{
}

NETWAKESOURCE
NxWakeSource::GetHandle(
    void
)
{
    return reinterpret_cast<NETWAKESOURCE>(this);
}

NET_WAKE_SOURCE_TYPE
NxWakeSource::GetType(
    void
) const
{
    return m_type;
}

_Use_decl_annotations_
NxWakeMediaChange::NxWakeMediaChange(
    NETADAPTER Adapter
)
    : NxWakeSource(NetWakeSourceTypeMediaChange, Adapter)
{
}

_Use_decl_annotations_
void
NxWakeMediaChange::SetWakeUpFlags(
    ULONG WakeUpFlags
)
{
    m_wakeOnMediaConnect = !!(WakeUpFlags & NDIS_PM_WAKE_ON_LINK_CHANGE_ENABLED);
    m_wakeOnMediaDisconnect = !!(WakeUpFlags & NDIS_PM_WAKE_ON_MEDIA_DISCONNECT_ENABLED);

    auto const enabled = m_wakeOnMediaConnect || m_wakeOnMediaDisconnect;

    SetEnabled(enabled);
}

_Use_decl_annotations_
void
NxWakeMediaChange::GetParameters(
    NET_WAKE_SOURCE_MEDIA_CHANGE_PARAMETERS * Parameters
) const
{
    Parameters->MediaConnect = m_wakeOnMediaConnect;
    Parameters->MediaDisconnect = m_wakeOnMediaDisconnect;
}

_Use_decl_annotations_
NxWakePacketFilterMatch::NxWakePacketFilterMatch(
    NETADAPTER Adapter
)
    : NxWakeSource(NetWakeSourceTypePacketFilterMatch, Adapter)
{
}

_Use_decl_annotations_
void
NxWakePacketFilterMatch::SetWakeUpFlags(
    ULONG WakeUpFlags
)
{
    auto const enabled = !!(WakeUpFlags & (NDIS_PM_SELECTIVE_SUSPEND_ENABLED | NDIS_PM_AOAC_NAPS_ENABLED));
    SetEnabled(enabled);
}

_Use_decl_annotations_
NxWakePattern::NxWakePattern(
    NET_WAKE_SOURCE_TYPE Type,
    NETADAPTER Adapter,
    ULONG Id
)
    : NxWakeSource(Type, Adapter)
    , m_id(Id)
{
}

ULONG
NxWakePattern::GetID(
    void
) const
{
    return m_id;
}

_Use_decl_annotations_
NxWakeBitmapPattern::NxWakeBitmapPattern(
    NETADAPTER Adapter,
    ULONG Id,
    wistd::unique_ptr<unsigned char[]> Mask,
    size_t MaskSize,
    wistd::unique_ptr<unsigned char[]> Pattern,
    size_t PatternSize
)
    : NxWakePattern(NetWakeSourceTypeBitmapPattern, Adapter, Id)
    , m_mask(wistd::move(Mask))
    , m_maskSize(MaskSize)
    , m_pattern(wistd::move(Pattern))
    , m_patternSize(PatternSize)
{
}

_Use_decl_annotations_
NTSTATUS
NxWakeBitmapPattern::CreateFromNdisWoLPattern(
    NETADAPTER Adapter,
    NDIS_PM_WOL_PATTERN const * NdisPattern,
    NxWakePattern ** BitmapPattern
)
{
    *BitmapPattern = nullptr;

    NT_FRE_ASSERT(NdisPattern->WoLPacketType == NdisPMWoLPacketBitmapPattern);

    auto const basePointer = reinterpret_cast<unsigned char const *>(NdisPattern);

    auto const ndisMask = basePointer + NdisPattern->WoLPattern.WoLBitMapPattern.MaskOffset;
    auto const ndisPattern = basePointer + NdisPattern->WoLPattern.WoLBitMapPattern.PatternOffset;
    auto const maskSize = NdisPattern->WoLPattern.WoLBitMapPattern.MaskSize;
    auto const patternSize = NdisPattern->WoLPattern.WoLBitMapPattern.PatternSize;

    auto mask = wistd::unique_ptr<unsigned char[]>(new(std::nothrow, 'PBxN') unsigned char[maskSize]);
    auto pattern = wistd::unique_ptr<unsigned char[]>(new(std::nothrow, 'PBxN') unsigned char[patternSize]);

    if (!mask || !pattern)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(
        mask.get(),
        ndisMask,
        maskSize);

    RtlCopyMemory(
        pattern.get(),
        ndisPattern,
        patternSize);

    auto bitmapPattern = wil::make_unique_nothrow<NxWakeBitmapPattern>(
        Adapter,
        NdisPattern->PatternId,
        wistd::move(mask),
        maskSize,
        wistd::move(pattern),
        patternSize);

    if (!bitmapPattern)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *BitmapPattern = bitmapPattern.release();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxWakeBitmapPattern::GetParameters(
    NET_WAKE_SOURCE_BITMAP_PARAMETERS * Parameters
) const
{
    Parameters->Id = m_id;
    Parameters->MaskSize = m_maskSize;
    Parameters->Mask = m_mask.get();
    Parameters->PatternSize = m_patternSize;
    Parameters->Pattern = m_pattern.get();
}

_Use_decl_annotations_
NTSTATUS
NxWakeMagicPacket::CreateFromNdisWoLPattern(
    NETADAPTER Adapter,
    NDIS_PM_WOL_PATTERN const * NdisPattern,
    NxWakePattern ** MagicPacket
)
{
    *MagicPacket = nullptr;

    NT_FRE_ASSERT(NdisPattern->WoLPacketType == NdisPMWoLPacketMagicPacket);

    auto magicPacket = wil::make_unique_nothrow<NxWakeMagicPacket>(Adapter, NdisPattern->PatternId);

    if (!magicPacket)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *MagicPacket = magicPacket.release();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxWakeMagicPacket::NxWakeMagicPacket(
    NETADAPTER Adapter,
    ULONG Id
)
    : NxWakePattern(NetWakeSourceTypeMagicPacket, Adapter, Id)
{
}

_Use_decl_annotations_
NTSTATUS
NxWakeEapolPacket::CreateFromNdisWoLPattern(
    NETADAPTER Adapter,
    NDIS_PM_WOL_PATTERN const * NdisPattern,
    NxWakePattern ** EapolPacket
)
{
    *EapolPacket = nullptr;

    NT_FRE_ASSERT(NdisPattern->WoLPacketType == NdisPMWoLPacketEapolRequestIdMessage);

    auto eapolPacket = wil::make_unique_nothrow<NxWakeEapolPacket>(Adapter, NdisPattern->PatternId);

    if (!eapolPacket)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *EapolPacket = eapolPacket.release();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxWakeEapolPacket::NxWakeEapolPacket(
    NETADAPTER Adapter,
    ULONG Id
)
    : NxWakePattern(NetWakeSourceTypeEapolPacket, Adapter, Id)
{
}

