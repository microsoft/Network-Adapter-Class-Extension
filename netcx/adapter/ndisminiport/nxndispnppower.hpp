// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <NxNew.hpp>

#include "MiniportReference.hpp"

#ifdef NETCX_UMDF
#include <WudfIrp.h>
typedef WUDF_IRP IRP;
#endif

class BridgeToKernel;

class NxNdisPnpPower
    : public NxNonpagedAllocation<'rNxN'>
{

private:

    NDIS_HANDLE const
        m_ndisAdapterHandle;

    WDFDEVICE const
        m_device;

#ifdef NETCX_UMDF
    BridgeToKernel *
        m_bridgeToKernel = nullptr;
#endif

public:

    NxNdisPnpPower(
        _In_ NDIS_HANDLE NdisMiniportHandle,
        _In_ WDFDEVICE Device
    );

    ~NxNdisPnpPower(
        void
    ) = default;

#ifdef NETCX_UMDF
    void
    SetBridgeToKernel(
        _In_ BridgeToKernel * Bridge
    );
#endif

    void
    NdisMiniportDeviceStarted(
        void
    );

    void
    SetNdisMiniportPower(
        _In_ DEVICE_POWER_STATE PowerState
    );

    NTSTATUS
    NotifyNdisPowerOperation(
        _In_ NdisWdfPnpPowerAction PowerActionOccured,
        _In_ NdisWdfPnpPowerAction LastPowerDownAction
    );

    void
    NotifyAsyncPowerReferenceCompletion(
        _In_ NTSTATUS StatusOfOperation
    );

    void
    NotifyWmiNdisMiniportArrival(
        void
    );

    void
    ReportNdisMiniportSurpriseRemove(
        void
    );

    bool
    TryReferenceNdisMiniport(
        void
    ) const;

    void
    DereferenceNdisMiniport(
        void
    ) const;

    unique_miniport_reference
    GetMiniportReference(
        void
    ) const;

    NTSTATUS
    HandleCreateIrp(
        _Inout_ IRP * Irp
    );

    NTSTATUS
    HandleQueryAllDataIrp(
        _In_ NDIS_GUID * NdisGuid,
        _In_ GUID * Guid,
        _Inout_ void * Wnode,
        _In_ ULONG BufferSize,
        _Out_ ULONG * ReturnSize
    );
};
