// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"
#include <BridgeToKernel.hpp>
#include "NxNdisMiniport.hpp"
#include "NxNdisMiniportUm.tmh"
#include "OidDispatchContext.hpp"



_Use_decl_annotations_
NxNdisMiniport::NxNdisMiniport(
    NxOrRdAdapter * NetAdapter
)
    : m_adapter(NetAdapter)
{
}

void NxNdisMiniport::SetBridgeToKernel(
    _In_ BridgeToKernel * Bridge
)
{
    m_bridgeToKernel = Bridge;
}

void
NxNdisMiniport::CompleteMiniportPause(
    void
)
{
    m_bridgeToKernel->CompleteMiniportPause();
}

void
NxNdisMiniport::CompleteMiniportRestart(
    void
)
{
    m_bridgeToKernel->CompleteMiniportRestart();
}

_Use_decl_annotations_
void
NxNdisMiniport::CompleteOidRequest(
    NDIS_OID_REQUEST * OidRequest,
    NDIS_STATUS Status
)
{
    OidDispatchContext* oidContext = CONTAINING_RECORD(OidRequest, OidDispatchContext, OidRequestCopy);
    m_bridgeToKernel->CompleteOidRequest(oidContext, Status);
}

_Use_decl_annotations_
void
NxNdisMiniport::IndicateMiniportStatus(
    NDIS_STATUS_INDICATION * NdisStatusIndication
)
{
    m_bridgeToKernel->IndicateMiniportStatusChange(*NdisStatusIndication);
}

_Use_decl_annotations_
NTSTATUS
NxNdisMiniport::SetNative80211Attributes(
    NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES* Attributes
)
{
    return m_bridgeToKernel->SetNative80211Attributes(Attributes);
}

NDIS_HANDLE
NxNdisMiniport::GetNdisHandle(
    void
) const
{
    return m_ndisAdapterHandle;
}

void
NxNdisMiniport::GetTriageInfo(
    void
)
{
    g_NetAdapterCxTriageBlock.NxNdisMiniportHandleOffset = FIELD_OFFSET(NxNdisMiniport, m_ndisAdapterHandle);
}

_Use_decl_annotations_
NDIS_STATUS
NxNdisMiniport::SetGeneralAttributes(
    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES & AdapterGeneral
)
{
    return m_bridgeToKernel->SetGeneralAttributes(AdapterGeneral);
}

_Use_decl_annotations_
NxNdisPnpMiniport::NxNdisPnpMiniport(
    NxOrRdAdapter * NetAdapter,
    WDFDEVICE Device
)
    : NxNdisMiniport(NetAdapter)
    , m_device(Device)
{
}

NTSTATUS
NxNdisPnpMiniport::CreateNdisMiniport(
    void
)
{
    return m_bridgeToKernel->CreateNdisMiniport();
}

NTSTATUS
NxNdisPnpMiniport::StartNdisMiniport(
    void
)
{
    return m_bridgeToKernel->StartNdisMiniport();
}

void
NxNdisPnpMiniport::StartNdisMiniportDataPath(
    void
)
{
    m_bridgeToKernel->StartNdisMiniportDataPath();
}

void
NxNdisPnpMiniport::StopNdisMiniport(
    void
)
{
    m_bridgeToKernel->StopNdisMiniport();
}

NTSTATUS
NxNdisPnpMiniport::RemoveNdisMiniport(
    void
)
{
    return m_bridgeToKernel->RemoveNdisMiniport();
}

NDIS_STATUS
NxNdisPnpMiniport::SetRegistrationAttributes(
    void
)
{
    return m_bridgeToKernel->SetRegistrationAttributes();
}

_Use_decl_annotations_
NxNdisLwMiniport::NxNdisLwMiniport(
    NxOrRdAdapter * NetAdapter,
    GUID InterfaceGuid
)
    : NxNdisMiniport(NetAdapter)
    , m_interfaceGuid(InterfaceGuid)
{
}

NTSTATUS
NxNdisLwMiniport::CreateNdisMiniport(
    void
)
{
    return STATUS_SUCCESS;
}

NTSTATUS
NxNdisLwMiniport::StartNdisMiniport(
    void
)
{
    return STATUS_SUCCESS;
}

void
NxNdisLwMiniport::StartNdisMiniportDataPath(
    void
)
{
}

void
NxNdisLwMiniport::StopNdisMiniport(
    void
)
{
}

NTSTATUS
NxNdisLwMiniport::RemoveNdisMiniport(
    void
)
{
    return STATUS_SUCCESS;
}

NDIS_STATUS
NxNdisLwMiniport::SetRegistrationAttributes(
    void
)
{
    return STATUS_SUCCESS;
}

NTSTATUS
NxNdisLwMiniport::SetupAdapterProperties(
    void
)
{
    return STATUS_SUCCESS;
}
