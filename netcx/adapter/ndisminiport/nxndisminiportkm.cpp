// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"

#include <KRegKey.h>
#include <NdisBindRegistry.h>
#include <ndis/lwminiportapi.h>
#include <wil/resource.h>

#include "NxNdisMiniport.hpp"

#include "NxUtility.hpp"

#ifdef NETCX_REDIRECTOR
#include "RdAdapter.hpp"
#else
#include "NxAdapter.hpp"
#endif

#include "NxDevice.hpp"
#include "NxDriver.hpp"
#include "NxPrivateGlobals.hpp"

#include "NxNdisMiniportKm.tmh"

#define NETSETUP_MAX_REGISTRY_PATH_CCH 256

_Use_decl_annotations_
NxNdisMiniport::NxNdisMiniport(
    NxOrRdAdapter * NetAdapter
)
    : m_adapter(NetAdapter)
{
}

void
NxNdisMiniport::CompleteMiniportPause(
    void
)
{
    NdisMPauseComplete(m_ndisAdapterHandle);
}

void
NxNdisMiniport::CompleteMiniportRestart(
    void
)
{
    NdisMRestartComplete(
        m_ndisAdapterHandle,
        NdisConvertNtStatusToNdisStatus(STATUS_SUCCESS));
}

_Use_decl_annotations_
void
NxNdisMiniport::CompleteOidRequest(
    NDIS_OID_REQUEST * OidRequest,
    NDIS_STATUS Status
)
{
    NdisMOidRequestComplete(m_ndisAdapterHandle, OidRequest, Status);
}

_Use_decl_annotations_
void
NxNdisMiniport::CompleteDirectOidRequest(
    NDIS_OID_REQUEST * OidRequest,
    NDIS_STATUS Status
)
{
    NdisMDirectOidRequestComplete(m_ndisAdapterHandle, OidRequest, Status);
}

_Use_decl_annotations_
void
NxNdisMiniport::IndicateMiniportStatus(
    NDIS_STATUS_INDICATION * NdisStatusIndication
)
{
    NdisMIndicateStatusEx(m_ndisAdapterHandle, NdisStatusIndication);
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
#ifndef NETCX_REDIRECTOR
    g_NetAdapterCxTriageBlock.NxNdisMiniportHandleOffset = FIELD_OFFSET(NxNdisMiniport, m_ndisAdapterHandle);
#endif
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
    NT_FRE_ASSERT(m_ndisAdapterHandle == nullptr);

    WDFDRIVER driver = WdfDeviceGetDriver(m_device);

    NDIS_WDF_ADD_DEVICE_INFO addDeviceInfo = {};
    addDeviceInfo.DriverObject = WdfDriverWdmGetDriverObject(driver);
    addDeviceInfo.PhysicalDeviceObject = WdfDeviceWdmGetPhysicalDevice(m_device);
    addDeviceInfo.MiniportAdapterContext = reinterpret_cast<NDIS_HANDLE>(m_adapter);

    return NdisWdfPnPAddDevice(&addDeviceInfo, &m_ndisAdapterHandle);
}

NTSTATUS
NxNdisPnpMiniport::StartNdisMiniport(
    void
)
{
    return NdisWdfPnpPowerEventHandler(
        m_ndisAdapterHandle,
        NdisWdfActionPnpStart,
        NdisWdfActionPowerNone);
}

void
NxNdisPnpMiniport::StartNdisMiniportDataPath(
    void
)
{
    NdisWdfMiniportDataPathStart(m_ndisAdapterHandle);
}

void
NxNdisPnpMiniport::StopNdisMiniport(
    void
)
{
    NdisWdfPnpPowerEventHandler(
        m_ndisAdapterHandle,
        NdisWdfActionPnpStop,
        NdisWdfActionPowerNone);
}

NTSTATUS
NxNdisPnpMiniport::RemoveNdisMiniport(
    void
)
{
    return NdisWdfPnpPowerEventHandler(
        m_ndisAdapterHandle,
        NdisWdfActionPnpRemove,
        NdisWdfActionPowerNone);
}

NDIS_STATUS
NxNdisPnpMiniport::SetRegistrationAttributes(
    void
)
{
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES adapterRegistration = {};

    //
    // Set the registration attributes for the network adapter.
    //
    // Note: Unlike a traditional NDIS miniport, the NetAdapterCx client
    // must not set the adapterRegistration.MiniportAdapterContext value.
    //
    adapterRegistration.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    adapterRegistration.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;
    adapterRegistration.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;

    adapterRegistration.AttributeFlags =
        NDIS_MINIPORT_ATTRIBUTES_SURPRISE_REMOVE_OK |
        NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM |
        NDIS_MINIPORT_ATTRIBUTES_NO_PAUSE_ON_SUSPEND;

    adapterRegistration.InterfaceType = NdisInterfacePNPBus;

    return NdisMSetMiniportAttributes(
        m_ndisAdapterHandle,
        reinterpret_cast<NDIS_MINIPORT_ADAPTER_ATTRIBUTES *>(&adapterRegistration));
}

_Use_decl_annotations_
NTSTATUS
NxNdisMiniport::SetNative80211Attributes(
    NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES* Attributes
)
{
    NDIS_MINIPORT_ADAPTER_ATTRIBUTES miniportAttributes;

    RtlZeroMemory(&miniportAttributes, sizeof(miniportAttributes));

    RtlCopyMemory(
        &miniportAttributes.Native_802_11_Attributes,
        Attributes,
        sizeof(NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES));

    return NdisMSetMiniportAttributes(m_ndisAdapterHandle, &miniportAttributes);
}

NDIS_STATUS
NxNdisMiniport::SetGeneralAttributes(
    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES & AdapterGeneral
)
{
    return NdisMSetMiniportAttributes(
        m_ndisAdapterHandle,
        reinterpret_cast<PNDIS_MINIPORT_ADAPTER_ATTRIBUTES>(&AdapterGeneral));
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
NxNdisLwMiniport::SetupAdapterProperties(
    void
)
{
    WCHAR interfacePath[NETSETUP_MAX_REGISTRY_PATH_CCH];
    KRegKey interfaceKey;
    ULONG ifType;
    ULONG netLuidIndex;
    ULONG mediaType;

    swprintf_s(
        interfacePath,
        NETSETUP_MAX_REGISTRY_PATH_CCH,
        NDIS_BIND_REGISTRY_CCS_KEY
            L"\\" NDIS_BIND_REGISTRY_CONTROL_KEY
                L"\\" NDIS_BIND_REGISTRY_NETWORKSETUP_KEY
                    L"\\" NDIS_BIND_REGISTRY_INTERFACES_KEY
                        L"\\{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"
                            L"\\" NDIS_BIND_REGISTRY_KERNEL_KEY,
        m_interfaceGuid.Data1, m_interfaceGuid.Data2, m_interfaceGuid.Data3,
        m_interfaceGuid.Data4[0], m_interfaceGuid.Data4[1],
        m_interfaceGuid.Data4[2], m_interfaceGuid.Data4[3],
        m_interfaceGuid.Data4[4], m_interfaceGuid.Data4[5],
        m_interfaceGuid.Data4[6], m_interfaceGuid.Data4[7]);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        interfaceKey.Open(KEY_QUERY_VALUE, interfacePath),
        "Failed to open registry path to interface in NetSetup store");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        interfaceKey.QueryValueUlong(L"IfType", &ifType),
        "Failed to query IfType in NetSetup store");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        interfaceKey.QueryValueUlong(L"NetLuidIndex", &netLuidIndex),
        "Failed to query NetLuidIndex in NetSetup store");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        interfaceKey.QueryValueUlong(L"MediaType", &mediaType),
        "Failed to query MediaType in NetSetup store");

    NDIS_WDF_COMPLETE_ADD_PARAMS params;
    params.InterfaceGuid = m_interfaceGuid;
    params.NetLuid.Info.Reserved = 0;
    params.NetLuid.Info.IfType = ifType;
    params.NetLuid.Info.NetLuidIndex = netLuidIndex;
    params.MediaType = static_cast<NDIS_MEDIUM>(mediaType);
    RtlInitUnicodeString(&params.BaseName, L"");
    RtlInitUnicodeString(&params.AdapterInstanceName, L"hostvnic");
    RtlInitUnicodeString(&params.DriverImageName, L"");

#ifndef NETCX_REDIRECTOR
    params.ExecutionContextKnobs = &NxDriver::DevicelessEcRuntimeKnobs;
#endif

    m_adapter->NdisMiniportCreationComplete(params);

    return STATUS_SUCCESS;
}

NTSTATUS
NxNdisLwMiniport::CreateNdisMiniport(
    void
)
{
#ifndef NETCX_REDIRECTOR
    NT_FRE_ASSERT(m_interfaceGuid != GUID{});
    NT_FRE_ASSERT(m_ndisAdapterHandle == nullptr);

    NX_PRIVATE_GLOBALS * privateGlobals = m_adapter->GetPrivateGlobals();
    NDIS_HANDLE lwmDriverHandle = privateGlobals->NxDriver->GetNdisLwmDriverHandle();
    NDIS_HANDLE miniportAdapterContext = reinterpret_cast<NDIS_HANDLE>(m_adapter);

    auto const ndisStatus = NdisLWMInitializeNetworkInterface(
        lwmDriverHandle,
        miniportAdapterContext,
        &m_interfaceGuid,
        &m_ndisAdapterHandle);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NdisConvertNdisStatusToNtStatus(ndisStatus),
        "NdisLWMInitializeNetworkInterface failed");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        SetupAdapterProperties(),
        "Failed to get adapter properties from NetSetup");
#endif
    return STATUS_SUCCESS;
}

NTSTATUS
NxNdisLwMiniport::StartNdisMiniport(
    void
)
{
#ifndef NETCX_REDIRECTOR
    //
    // NDIS_LWM_DRIVER_CHARACTERISTICS have no InitializeHandlerEx field, so NDIS won't invoke
    // EvtNdisInitializeEx to NetAdapterCx. We invoke it here manually.
    // Notice operations in NdisInitialize have been overridden for NxNdisLwMiniport.
    //
    CX_RETURN_IF_NOT_NT_SUCCESS(m_adapter->NdisInitialize());

    auto const ndisStatus = NdisLWMStartNetworkInterface(m_ndisAdapterHandle);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NdisConvertNdisStatusToNtStatus(ndisStatus),
        "NdisLWMStartNetworkInterface failed");

    m_adapter->PnpPower.IoStartNoWait();
#endif

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
#ifndef NETCX_REDIRECTOR
    NdisLWMUninitializeNetworkInterface(m_ndisAdapterHandle);
    m_ndisAdapterHandle = nullptr;

    //
    // NDIS_LWM_DRIVER_CHARACTERISTICS have no HaltHandlerEx field, so NDIS won't invoke
    // EvtNdisHaltEx to NetAdapterCx. We enqueue NdisMiniportHalt event here so that NxAdapterSM
    // can proceede to Halting and Halted state.
    //

    m_adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportHalt);
    m_adapter->PnpPower.IoStopNoWait();
#endif
}

NTSTATUS
NxNdisLwMiniport::RemoveNdisMiniport(
    void
)
{
    //
    // If devieless NETADAPTER is not started, NdisLWMUninitializeNetworkInterface won't be invoked by
    // stopping network interface (e.g. AdapterPnpPower::StopNetworkInterface),
    // and AdapterPnpPower will be in IoStopped:InterfaceStopped state.
    // We call NdisLWMUninitializeNetworkInterface like we call NdisWdfPnpPowerEventHandler for PnP miniport.
    //
    if (m_ndisAdapterHandle != nullptr)
    {
        NdisLWMUninitializeNetworkInterface(m_ndisAdapterHandle);
        m_ndisAdapterHandle = nullptr;
    }

    return STATUS_SUCCESS;
}

NDIS_STATUS
NxNdisLwMiniport::SetRegistrationAttributes(
    void
)
{
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES adapterRegistration;

    RtlZeroMemory(&adapterRegistration, sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES));
    adapterRegistration.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    adapterRegistration.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;
    adapterRegistration.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;
    adapterRegistration.AttributeFlags = NDIS_MINIPORT_ATTRIBUTES_NO_HALT_ON_SUSPEND;
    adapterRegistration.InterfaceType = NdisInterfaceInternal;

    return NdisMSetMiniportAttributes(
        m_ndisAdapterHandle,
        reinterpret_cast<NDIS_MINIPORT_ADAPTER_ATTRIBUTES *>(&adapterRegistration));
}
