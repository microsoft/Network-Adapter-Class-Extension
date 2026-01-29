// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"

#include <BridgeToKernel.hpp>
#include "NxNdisPnpPower.hpp"

#include "NxNdisPnpPowerUm.tmh"


_Use_decl_annotations_
NxNdisPnpPower::NxNdisPnpPower(
    NDIS_HANDLE NdisMiniportHandle,
    WDFDEVICE Device
)
    : m_ndisAdapterHandle(NdisMiniportHandle)
    , m_device(Device)
{
}

_Use_decl_annotations_
void
NxNdisPnpPower::SetBridgeToKernel(
    BridgeToKernel * Bridge
)
{
    m_bridgeToKernel = Bridge;
}

void
NxNdisPnpPower::NdisMiniportDeviceStarted(
    void
)
{
    m_bridgeToKernel->MiniportStarted();
}

_Use_decl_annotations_
void
NxNdisPnpPower::SetNdisMiniportPower(
    DEVICE_POWER_STATE PowerState
)
{
    m_bridgeToKernel->MiniportSetPower(
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
    return m_bridgeToKernel->MiniportPowerOperation(
        PowerActionOccured,
        LastPowerDownAction);
}

void
NxNdisPnpPower::NotifyWmiNdisMiniportArrival(
    void
)
{
    m_bridgeToKernel->MiniportArrived();
}

void
NxNdisPnpPower::ReportNdisMiniportSurpriseRemove(
    void
)
{
    m_bridgeToKernel->MiniportSurpriseRemove();
}

bool
NxNdisPnpPower::TryReferenceNdisMiniport(
    void
) const
{
    return true;
}

void
NxNdisPnpPower::DereferenceNdisMiniport(
    void
) const
{
}

unique_miniport_reference
NxNdisPnpPower::GetMiniportReference(
    void
) const
{
    if (!TryReferenceNdisMiniport())
    {
        return  {};
    }

    return unique_miniport_reference{ m_bridgeToKernel };
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
    return m_bridgeToKernel->WmiQueryAllData(
        NdisGuid,
        Guid,
        Wnode,
        BufferSize,
        ReturnSize);
}
