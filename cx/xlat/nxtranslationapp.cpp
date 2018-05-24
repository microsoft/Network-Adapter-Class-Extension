// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements the driver-global state for the NBL-to-NET_PACKET translation
    app in NetAdapterCx.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndisoidtypes.h>

#include "NxTranslationApp.tmh"

#include "NxXlat.hpp"
#include "NxTranslationApp.hpp"
#include "NxPerfTuner.hpp"

TRACELOGGING_DEFINE_PROVIDER(
    g_hNetAdapterCxXlatProvider,
    "Microsoft.Windows.Ndis.NetAdapterCx.Translator",
    // {32723328-9a9f-5889-a637-bcc3f77c1324}
    (0x32723328, 0x9a9f, 0x5889, 0xa6, 0x37, 0xbc, 0xc3, 0xf7, 0x7c, 0x13, 0x24));

_IRQL_requires_(PASSIVE_LEVEL)
static
NTSTATUS
NetClientAdapterCreateDatapath(
    _In_ PVOID ClientContext
    )
{
    return reinterpret_cast<NxTranslationApp *>(ClientContext)->CreateDatapath();
}

_IRQL_requires_(PASSIVE_LEVEL)
static
void
NetClientAdapterDestroyDatapath(
    _In_ PVOID ClientContext
    )
{
    reinterpret_cast<NxTranslationApp *>(ClientContext)->DestroyDatapath();
}

_IRQL_requires_(PASSIVE_LEVEL)
static
void
NetClientAdapterStartDatapath(
    _In_ PVOID ClientContext
    )
{
    reinterpret_cast<NxTranslationApp *>(ClientContext)->StartDatapath();
}

_IRQL_requires_(PASSIVE_LEVEL)
static
void
NetClientAdapterStopDatapath(
    _In_ PVOID ClientContext
    )
{
    reinterpret_cast<NxTranslationApp *>(ClientContext)->StopDatapath();
}

_IRQL_requires_max_(DISPATCH_LEVEL)
static
BOOLEAN
NetClientAdapterNdisOidRequestHandler(
    _In_ PVOID ClientContext,
    _In_ NDIS_OID_REQUEST * Request,
    _Out_ NTSTATUS * Status
    )
{
    UNREFERENCED_PARAMETER(ClientContext);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Status);

    return false;
}

static const NET_CLIENT_CONTROL_DISPATCH ControlDispatch =
{
    sizeof(NET_CLIENT_CONTROL_DISPATCH),
    &NetClientAdapterCreateDatapath,
    &NetClientAdapterDestroyDatapath,
    &NetClientAdapterStartDatapath,
    &NetClientAdapterStopDatapath,
    &NetClientAdapterNdisOidRequestHandler,
    &NetClientAdapterNdisOidRequestHandler,
    &NetClientAdapterNdisOidRequestHandler,
};

_Use_decl_annotations_
NxTranslationAppFactory::~NxTranslationAppFactory(
    void
    )
{
    NxPerfTunerCleanup();
    TraceLoggingUnregister(g_hNetAdapterCxXlatProvider);
}

_Use_decl_annotations_
NTSTATUS
NxTranslationAppFactory::Initialize(
    void
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    TraceLoggingRegister(g_hNetAdapterCxXlatProvider);
    status  = NxPerfTunerInitialize();

    if (!NT_SUCCESS(status))
    {
        TraceLoggingUnregister(g_hNetAdapterCxXlatProvider);
    }

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationAppFactory::CreateApp(
    NET_CLIENT_DISPATCH const * Dispatch,
    NET_CLIENT_ADAPTER Adapter,
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
    void ** ClientContext,
    NET_CLIENT_CONTROL_DISPATCH const ** ClientDispatch
    )
{
    auto newApp = wil::make_unique_nothrow<NxTranslationApp>(Dispatch, Adapter, AdapterDispatch);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !newApp);

    *ClientContext = newApp.release();
    *ClientDispatch = &ControlDispatch;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxTranslationApp::NxTranslationApp(
    NET_CLIENT_DISPATCH const * Dispatch,
    NET_CLIENT_ADAPTER Adapter,
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
    ) noexcept :
    m_dispatch(Dispatch),
    m_adapter(Adapter),
    m_adapterDispatch(AdapterDispatch)
{
}

NET_CLIENT_ADAPTER_PROPERTIES
NxTranslationApp::GetProperties(
    void
    ) const
{
    NET_CLIENT_ADAPTER_PROPERTIES properties;
    m_adapterDispatch->GetProperties(m_adapter, &properties);

    return properties;
}

_Use_decl_annotations_
NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES
NxTranslationApp::GetDatapathCapabilities(
    void
    ) const
{
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES capabilities;
    m_adapterDispatch->GetDatapathCapabilities(m_adapter, &capabilities);

    return capabilities;
}

_Use_decl_annotations_
NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES
NxTranslationApp::GetReceiveScalingCapabilities(
    void
    ) const
{
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES capabilities;
    m_adapterDispatch->GetReceiveScalingCapabilities(m_adapter, &capabilities);

    return capabilities;
}

_Use_decl_annotations_
PAGEDX
NTSTATUS
NxTranslationApp::CreateDefaultQueues(
    void
    )
{
    auto txQueue = wil::make_unique_nothrow<NxTxXlat>(
        0,
        m_dispatch,
        m_adapter,
        m_adapterDispatch);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! txQueue);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        txQueue->Initialize());

    auto rxQueue = wil::make_unique_nothrow<NxRxXlat>(
        0,
        m_dispatch,
        m_adapter,
        m_adapterDispatch);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! rxQueue);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! rxQueue);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_rxQueues.resize(1));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        rxQueue->Initialize());

    m_txQueue = wistd::move(txQueue);
    m_rxQueues[0] = wistd::move(rxQueue);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxTranslationApp::StartDatapath(
    void
    )
{
    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties;
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);
    m_NblDispatcher = static_cast<INxNblDispatcher *>(adapterProperties.NblDispatcher);
    m_NblDispatcher->SetRxHandler(&m_rxBufferReturn);
    m_NblDispatcher->SetTxHandler(m_txQueue.get());

    m_txQueue->Start();

    for (auto & queue : m_rxQueues)
    {
        queue->Start();
    }
}

_Use_decl_annotations_
void
NxTranslationApp::StopDatapath(
    void
    )
{
    m_NblDispatcher->SetRxHandler(nullptr);

    m_txQueue->Cancel();
    m_NblDispatcher->SetTxHandler(nullptr);
    m_txQueue->Stop();

    for (auto & queue : m_rxQueues)
    {
        queue->Cancel();
    }

    for (auto & queue : m_rxQueues)
    {
        queue->Stop();
    }
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::CreateDatapath(
    void
    )
{
    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateDefaultQueues());

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxTranslationApp::DestroyDatapath(
    void
    )
{
    m_txQueue.reset();
    m_rxQueues.clear();
}

