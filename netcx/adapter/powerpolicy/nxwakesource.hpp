// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxPowerEntry.hpp"

class NxWakeSource : public NxPowerEntry
{

public:

    NxWakeSource(
        _In_ NET_WAKE_SOURCE_TYPE Type,
        _In_ NETADAPTER Adapter
    );

    NETWAKESOURCE
    GetHandle(
        void
    );

    NET_WAKE_SOURCE_TYPE
    GetType(
        void
    ) const;

protected:

    NET_WAKE_SOURCE_TYPE const
        m_type;
};

class NxWakeMediaChange : public NxWakeSource
{

public:

    NxWakeMediaChange(
        _In_ NETADAPTER Adapter
    );

    void
    GetParameters(
        _Inout_ NET_WAKE_SOURCE_MEDIA_CHANGE_PARAMETERS * Parameters
    ) const;

    void
    SetWakeUpFlags(
        _In_ ULONG WakeUpFlags
    );

private:

    bool
        m_wakeOnMediaConnect = false;

    bool
        m_wakeOnMediaDisconnect = false;
};

class NxWakePacketFilterMatch : public NxWakeSource
{

public:

    NxWakePacketFilterMatch(
        _In_ NETADAPTER Adapter
    );

    void
    SetWakeUpFlags(
        _In_ ULONG WakeUpFlags
    );

};

class NxWakePattern : public NxWakeSource
{

public:

    NxWakePattern(
        _In_ NET_WAKE_SOURCE_TYPE Type,
        _In_ NETADAPTER Adapter,
        _In_ ULONG Id
    );

    ULONG
    GetID(
        void
    ) const;

protected:

    ULONG const
        m_id;

};

class NxWakeBitmapPattern : public NxWakePattern
{

public:

    NxWakeBitmapPattern(
        _In_ NETADAPTER Adapter,
        _In_ ULONG Id,
        _In_ wistd::unique_ptr<unsigned char[]> Mask,
        _In_ size_t MaskSize,
        _In_ wistd::unique_ptr<unsigned char[]> Pattern,
        _In_ size_t PatternSize
    );

    void
    GetParameters(
        _In_ NET_WAKE_SOURCE_BITMAP_PARAMETERS * Parameters
    ) const;

    static
    NTSTATUS
    CreateFromNdisWoLPattern(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_PM_WOL_PATTERN const * NdisPattern,
        _Outptr_result_nullonfailure_ NxWakePattern ** BitmapPattern
    );

private:

    wistd::unique_ptr<unsigned char[]>
        m_pattern;

    size_t const
        m_patternSize;

    wistd::unique_ptr<unsigned char[]>
        m_mask;

    size_t const
        m_maskSize;
};

class NxWakeMagicPacket : public NxWakePattern
{

public:

    static
    NTSTATUS
    CreateFromNdisWoLPattern(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_PM_WOL_PATTERN const * NdisPattern,
        _Outptr_result_nullonfailure_ NxWakePattern ** MagicPacket
    );

public:

    NxWakeMagicPacket(
        _In_ NETADAPTER Adapter,
        _In_ ULONG Id
    );

};
