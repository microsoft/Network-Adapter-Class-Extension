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
void
NetClientAdapterCreateDatapath(
    _In_ PVOID ClientContext
    )
{
    auto app = reinterpret_cast<NxTranslationApp *>(ClientContext);
    NTSTATUS status = app->CreateDatapath();

    if (! NT_SUCCESS(status))
    {
        app->SetDeviceFailed(status);
    }
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
    auto app = reinterpret_cast<NxTranslationApp *>(ClientContext);
    bool handled = false;

    switch (Request->RequestType)
    {

    case NdisRequestSetInformation:

        switch (Request->DATA.SET_INFORMATION.Oid)
        {

        case OID_GEN_RECEIVE_SCALE_INITIALIZE:
            *Status = app->ReceiveScalingInitialize();
            handled = true;
            break;

        case OID_GEN_RECEIVE_SCALE_PARAMETERS_V2:
            *Status = app->ReceiveScalingSetParameters(*Request);
            handled = true;
            break;

        case OID_TCP_OFFLOAD_PARAMETERS:
            *Status = app->OffloadSetActiveCapabilities(*Request);
            handled = true;
            break;
        }
        break;

    case NdisRequestMethod:

        switch (Request->DATA.SET_INFORMATION.Oid)
        {

        case OID_GEN_RSS_SET_INDIRECTION_TABLE_ENTRIES:
            *Status = app->ReceiveScalingSetIndirectionEntries(*Request);
            handled = true;
            break;
        }

    }

    return handled;
}

_IRQL_requires_(PASSIVE_LEVEL)
static
NTSTATUS
NetClientAdapterOffloadInitialize(
    _In_ PVOID ClientContext
    )
{
    return reinterpret_cast<NxTranslationApp *>(ClientContext)->OffloadInitialize();
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
    &NetClientAdapterOffloadInitialize
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
    m_adapterDispatch(AdapterDispatch),
    m_offload(*this, AdapterDispatch->OffloadDispatch)
{
}

_Use_decl_annotations_
NET_CLIENT_ADAPTER
NxTranslationApp::GetAdapter(
    void
    ) const
{
    return m_adapter;
}

_Use_decl_annotations_
void
NxTranslationApp::SetDeviceFailed(
    NTSTATUS Status
    ) const
{
    m_adapterDispatch->SetDeviceFailed(m_adapter, Status);
}

_Use_decl_annotations_
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
NTSTATUS
NxTranslationApp::ReceiveScalingInitialize(
    void
    )
{
    NT_FRE_ASSERT(! m_receiveScaling);
    NT_FRE_ASSERT(! m_receiveScalingDatapath);

    auto receiveScaling = wil::make_unique_nothrow<NxReceiveScaling>(
        *this,
        this->m_rxQueues,
        this->m_adapterDispatch->ReceiveScalingDispatch);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! receiveScaling);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        receiveScaling->Initialize());

    m_receiveScaling = wistd::move(receiveScaling);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateReceiveScalingQueues());

    StartReceiveScalingQueues();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReceiveScalingSetParameters(
    NDIS_OID_REQUEST const & Request
    )
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_NOT_SUPPORTED,
        ! m_receiveScaling);

    CX_RETURN_NTSTATUS_IF(
        STATUS_SUCCESS,
        ! m_receiveScalingDatapath);

    return m_receiveScaling->SetParameters(Request);
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReceiveScalingSetIndirectionEntries(
    NDIS_OID_REQUEST const & Request
    )
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_NOT_SUPPORTED,
        ! m_receiveScaling);

    CX_RETURN_NTSTATUS_IF(
        STATUS_SUCCESS,
        ! m_receiveScalingDatapath);

    return m_receiveScaling->SetIndirectionEntries(Request);
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
PAGEDX
void
NxTranslationApp::StartDefaultQueues(
    void
    )
{
    m_txQueue->Start();
    m_rxQueues[0]->Start();
}

//
// Creates receive scaling queues, configures and starts queues if
// the data path is available and receive scaling has been initialized.
//
// This method may be called during creation of the data path to
// create the queues needed for receive scaling or it may be called
// during receive scaling initialization.
//
// It is important to understand that receive scaling initialization
// can occur even when the data path is not available. The following
// two cases can occur.
//
// 1. The data path is not available we optimistically succeed and
//    expect this method will be called again during data path creation.
//    If during data path creation this method fails then the
//    translator revokes receive scaling at that time.
//
// 2. The data path is available we attempt to complete all necessary
//    operations and return the outcome.
//
_Use_decl_annotations_
PAGEDX
NTSTATUS
NxTranslationApp::CreateReceiveScalingQueues(
    void
    )
{
    //
    // receive scaling is not initialized
    //
    auto receiveScaling = wistd::move(m_receiveScaling);
    if (! receiveScaling)
    {
        return STATUS_SUCCESS;
    }

    Rtl::KArray<wistd::unique_ptr<NxRxXlat>> queues;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! queues.resize(receiveScaling->GetNumberOfQueues() - m_rxQueues.count()));

    for (auto i = m_rxQueues.count(); i < receiveScaling->GetNumberOfQueues(); i++)
    {
        auto rxQueue = wil::make_unique_nothrow<NxRxXlat>(
            i,
            m_dispatch,
            m_adapter,
            m_adapterDispatch);

        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! rxQueue);

        CX_RETURN_IF_NOT_NT_SUCCESS(
            rxQueue->Initialize());

        queues[i - m_rxQueues.count()] = wistd::move(rxQueue);
    }

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_rxQueues.reserve(receiveScaling->GetNumberOfQueues()));

    for (auto & queue : queues)
    {
        NT_FRE_ASSERT(m_rxQueues.append(wistd::move(queue)));
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        receiveScaling->Configure());

    m_receiveScaling = wistd::move(receiveScaling);
    m_receiveScalingDatapath = true;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGEDX
void
NxTranslationApp::StartReceiveScalingQueues(
    void
    )
{
    if (m_datapathStarted && m_receiveScalingDatapath)
    {
        for (size_t i = 1; i < m_rxQueues.count(); i++)
        {
            m_rxQueues[i]->Start();
        }
    }
}

_Use_decl_annotations_
void
NxTranslationApp::StartDatapath(
    void
    )
{
    if (! m_datapathCreated)
    {
        return;
    }

    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties;
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);
    m_NblDispatcher = static_cast<INxNblDispatcher *>(adapterProperties.NblDispatcher);
    m_NblDispatcher->SetRxHandler(&m_rxBufferReturn);
    m_NblDispatcher->SetTxHandler(m_txQueue.get());

    StartDefaultQueues();

    m_datapathStarted = true;

    StartReceiveScalingQueues();
}

_Use_decl_annotations_
void
NxTranslationApp::StopDatapath(
    void
    )
{
    if (! m_datapathCreated)
    {
        return;
    }

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

    m_datapathStarted = false;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::CreateDatapath(
    void
    )
{
    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateDefaultQueues());

    (void)CreateReceiveScalingQueues();

    m_datapathCreated = true;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxTranslationApp::DestroyDatapath(
    void
    )
{
    m_datapathCreated = false;
    m_receiveScalingDatapath = false;

    m_txQueue.reset();
    m_rxQueues.clear();
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::OffloadInitialize(
    void
    )
{
    return m_offload.Initialize();
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::OffloadSetActiveCapabilities(
    NDIS_OID_REQUEST const & Request
    )
{
   return  m_offload.SetActiveCapabilities(Request);
}
