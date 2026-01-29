// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"

#include "NxNdisPnpPower.hpp"

#include "NxNdisPnpPowerKm.tmh"


_Use_decl_annotations_
NxNdisPnpPower::NxNdisPnpPower(
    NDIS_HANDLE NdisMiniportHandle,
    WDFDEVICE Device
)
    : m_ndisAdapterHandle(NdisMiniportHandle)
    , m_device(Device)
{
}

void
NxNdisPnpPower::NdisMiniportDeviceStarted(
    void
)
{
    NdisWdfMiniportStarted(m_ndisAdapterHandle);
}

_Use_decl_annotations_
void
NxNdisPnpPower::SetNdisMiniportPower(
    DEVICE_POWER_STATE PowerState
)
{
    NdisWdfMiniportSetPower(
        m_ndisAdapterHandle,
        WdfDeviceGetSystemPowerAction(m_device),
        PowerState);
}

_Use_decl_annotations_
NTSTATUS
NxNdisPnpPower::NotifyNdisPowerOperation(
    NdisWdfPnpPowerAction PowerActionOccured,
    NdisWdfPnpPowerAction LastPowerDownAction
)
{
    return NdisWdfPnpPowerEventHandler(
        m_ndisAdapterHandle,
        PowerActionOccured,
        LastPowerDownAction);
}

_Use_decl_annotations_
void
NxNdisPnpPower::NotifyAsyncPowerReferenceCompletion(
    NTSTATUS StatusOfOperation
)
{
    NdisWdfAsyncPowerReferenceCompleteNotification(
        m_ndisAdapterHandle,
        StatusOfOperation);
}

void
NxNdisPnpPower::NotifyWmiNdisMiniportArrival(
    void
)
{
    NdisWdfNotifyWmiAdapterArrival(m_ndisAdapterHandle);
}

void
NxNdisPnpPower::ReportNdisMiniportSurpriseRemove(
    void
)
{
    NdisWdfPnpPowerEventHandler(
        m_ndisAdapterHandle,
        NdisWdfActionPnpSurpriseRemove,
        NdisWdfActionPowerNone);
}

bool
NxNdisPnpPower::TryReferenceNdisMiniport(
    void
) const
{
    return !!NdisWdfMiniportTryReference(m_ndisAdapterHandle);
}

void
NxNdisPnpPower::DereferenceNdisMiniport(
    void
) const
{
    return NdisWdfMiniportDereference(m_ndisAdapterHandle);
}

unique_miniport_reference
NxNdisPnpPower::GetMiniportReference(
    void
) const
{
    if (!NdisWdfMiniportTryReference(m_ndisAdapterHandle))
    {
        return  {};
    }

    return unique_miniport_reference{ m_ndisAdapterHandle };
}

_Use_decl_annotations_
NTSTATUS
NxNdisPnpPower::HandleQueryAllDataIrp(
    NDIS_GUID * NdisGuid,
    GUID * Guid,
    void * Wnode,
    ULONG BufferSize,
    ULONG * ReturnSize
)
{
    return NdisWdfQueryAllData(m_ndisAdapterHandle, NdisGuid, Guid, Wnode, BufferSize, ReturnSize);
}
