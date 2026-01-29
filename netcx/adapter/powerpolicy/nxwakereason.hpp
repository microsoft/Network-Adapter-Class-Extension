// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <NxNew.hpp>

#define NX_WAKE_REASON_TAG 'RwxN'

class NxWakeReason
    : public NxNonpagedAllocation<NX_WAKE_REASON_TAG>
{

private:

    NETADAPTER const
        m_adapter;

    WDFMEMORY
        m_wakeReasonMemory = WDF_NO_HANDLE;

public:

    NxWakeReason(
        _In_ NETADAPTER Adapter
    );

    ~NxWakeReason(
        void
    ) = default;

    NxWakeReason(
        _In_ NxWakeReason const & Other
    ) = delete;

    NxWakeReason(
        _In_ NxWakeReason && Other
    ) = delete;

    NxWakeReason &
    operator=(
        _In_ NxWakeReason const & Other
    ) = delete;

    NxWakeReason &
    operator=(
        _In_ NxWakeReason && Other
    ) = delete;

    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    MakeWakeReasonPacket(
        _In_ NET_ADAPTER_WAKE_REASON_PACKET const * Reason,
        _Outptr_ NDIS_PM_WAKE_REASON ** WakeReason,
        _Out_ ULONG * WakeReasonSize
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    MakeWakeReasonMediaChange(
        _In_ NET_IF_MEDIA_CONNECT_STATE MediaEvent,
        _Out_ NDIS_PM_WAKE_REASON * WakeReason
    ) const;
};
