// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements the driver-global state for the NBL-to-NET_PACKET translation
    app in NetAdapterCx.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndis/oidrequesttypes.h>
#include <ntddndis_p.h>

#include "NxTranslationApp.tmh"

#include "NxXlat.hpp"
#include "NxTranslationApp.hpp"
#include "NxPerfTuner.hpp"
#include <ntstrsafe.h>

TRACELOGGING_DEFINE_PROVIDER(
    g_hNetAdapterCxXlatProvider,
    "Microsoft.Windows.Ndis.NetAdapterCx.Translator",
    // {32723328-9a9f-5889-a637-bcc3f77c1324}
    (0x32723328, 0x9a9f, 0x5889, 0xa6, 0x37, 0xbc, 0xc3, 0xf7, 0x7c, 0x13, 0x24));

#ifdef _KERNEL_MODE

#include <pcwdata.h>
#include "NetAdapterCxPc.h"

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS NTAPI
NetAdapterCxQueueCounterSetCallback(
    _In_ PCW_CALLBACK_TYPE PcwType,
    _In_ PPCW_CALLBACK_INFORMATION PcwInfo,
    _In_opt_ PVOID)
{
    NTSTATUS Status = STATUS_SUCCESS;

    switch (PcwType)
    {
    case PcwCallbackAddCounter:
    case PcwCallbackRemoveCounter:
        break;

    case PcwCallbackEnumerateInstances:

        NxTranslationApp::s_AppCollection->ForEach(
            [PcwInfo](const NxTranslationApp& app)
            {
                app.FillPerfCounter(PcwInfo, PCW_ANY_INSTANCE_ID);
            });

        break;

    case PcwCallbackCollectData:

        NxTranslationApp::s_AppCollection->ForEach(
            [PcwInfo](const NxTranslationApp& app)
            {
                app.FillPerfCounter(PcwInfo, PcwInfo->CollectData.InstanceId);
            });

        break;
    }; // switch(PcwType)

    return Status;
}

#endif

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

    case NdisRequestQueryStatistics:
    case NdisRequestQueryInformation:

        switch (Request->DATA.QUERY_INFORMATION.Oid)
        {
        // we don't support disable interrupt moderation in v2.0
        case OID_GEN_INTERRUPT_MODERATION:
            *Status = app->ReportInterruptModerationNotSupported(*Request);
            handled = true;
            break;

        case OID_GEN_STATISTICS:
            *Status = app->ReportGeneralStatistics(*Request);
            handled = true;
            break;

        case OID_GEN_XMIT_OK:
            *Status = app->ReportTxPacketStatistics(*Request);
            handled = true;
            break;

        case OID_GEN_RCV_OK:
            *Status = app->ReportRxPacketStatistics(*Request);
            handled = true;
            break;

        case OID_GEN_VENDOR_DRIVER_VERSION:
            *Status = app->ReportUlong(*Request, 0);
            handled = true;
            break;

        case OID_GEN_VENDOR_DESCRIPTION:
            *Status = app->ReportVendorDriverDescription(*Request);
            handled = true;
            break;

        case OID_GEN_VENDOR_ID:
            *Status = app->ReportUlong(*Request, 0xFFFFFF);
            handled = true;
            break;

        case OID_GEN_MAXIMUM_TOTAL_SIZE:
            *Status = app->ReportUlong(*Request, app->GetMaximumPacketSize());
            handled = true;
            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:
            *Status = app->ReportUlong(*Request, app->GetTxFrameSize());
            handled = true;
            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:
            *Status = app->ReportUlong(*Request, app->GetRxFrameSize());
            handled = true;
            break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:
            *Status = app->ReportUlong(*Request,
                app->GetTxFrameSize() * app->GetTxFragmentRingSize());
            handled = true;
            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:
            *Status = app->ReportUlong(*Request,
                app->GetRxFrameSize() * app->GetRxFragmentRingSize());
            handled = true;
            break;
        }
        break;

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

        case OID_OFFLOAD_ENCAPSULATION:
            *Status = app->OffloadSetEncapsulation(*Request);
            handled = true;
            break;

        case OID_GEN_INTERRUPT_MODERATION:
            *Status = STATUS_NOT_SUPPORTED;
            handled = true;
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
            *Status = STATUS_SUCCESS;
            handled = true;
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            *Status = app->SetPacketFilter(*Request);
            handled = true;
            break;

        case OID_802_3_MULTICAST_LIST:
            *Status = app->SetMulticastList(*Request);
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
    if (NxTranslationApp::s_AppCollection)
    {
        ExFreePoolWithTag(NxTranslationApp::s_AppCollection, 'pAxN');
    }

    NxPerfTunerCleanup();

#ifdef _KERNEL_MODE
    UnregisterNetAdapterCxQueueCounterSet();
#endif

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

    if (NT_SUCCESS(status))
    {
        PVOID memory =
            ExAllocatePoolWithTag(PagedPool, sizeof(NxCollection<NxTranslationApp>), 'pAxN');

        if (memory)
        {
            NxTranslationApp::s_AppCollection =  new (memory) NxCollection<NxTranslationApp>();
        }
        else
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (!NT_SUCCESS(status))
    {
        TraceLoggingUnregister(g_hNetAdapterCxXlatProvider);
    }

#ifdef _KERNEL_MODE

    status = RegisterNetAdapterCxQueueCounterSet(
                NetAdapterCxQueueCounterSetCallback,
                nullptr);
    NT_FRE_ASSERT(NT_SUCCESS(status));

#endif

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

    CX_RETURN_IF_NOT_NT_SUCCESS(newApp->Initialize());

    *ClientContext = newApp.release();
    *ClientDispatch = &ControlDispatch;

    return STATUS_SUCCESS;
}


NxCollection<NxTranslationApp>* NxTranslationApp::s_AppCollection = nullptr;
ULONG NxTranslationApp::s_LastAppId = 0;

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
    NxTranslationApp::s_AppCollection->Add(this);
}

_Use_decl_annotations_
NxTranslationApp::~NxTranslationApp(
)
{
    NxTranslationApp::s_AppCollection->Remove(this);
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::Initialize(
    void
)
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_rxStatistics.resize(1));

    return STATUS_SUCCESS;
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
        m_adapterDispatch,
        m_txStatistics);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! txQueue);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        txQueue->Initialize());

    auto rxQueue = wil::make_unique_nothrow<NxRxXlat>(
        0,
        m_dispatch,
        m_adapter,
        m_adapterDispatch,
        m_rxStatistics[0]);

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

    KLockThisExclusive lock(m_statisticsLock);

    if (receiveScaling->GetNumberOfQueues() > m_rxStatistics.count())
    {
        CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_rxStatistics.resize(receiveScaling->GetNumberOfQueues()));
    }

    lock.Release();

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
            m_adapterDispatch,
            m_rxStatistics[i]);

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

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::OffloadSetEncapsulation(
    NDIS_OID_REQUEST const & Request
)
{
   return  m_offload.SetEncapsulation(Request);
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReportInterruptModerationNotSupported(
    NDIS_OID_REQUEST & Request
    )
{
    ULONG byteWritten = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;

    NT_FRE_ASSERT(Request.DATA.QUERY_INFORMATION.InformationBufferLength >= byteWritten);

    NDIS_INTERRUPT_MODERATION_PARAMETERS* params =
        (NDIS_INTERRUPT_MODERATION_PARAMETERS*) Request.DATA.QUERY_INFORMATION.InformationBuffer;

    params->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    params->Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
    params->Header.Size = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
    params->Flags = 0;
    params->InterruptModeration = NdisInterruptModerationNotSupported;

    Request.DATA.QUERY_INFORMATION.BytesWritten = byteWritten;

    return STATUS_SUCCESS;
};

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReportGeneralStatistics(
    NDIS_OID_REQUEST & Request
) const
{
    ULONG byteWritten = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;

    if (Request.DATA.QUERY_INFORMATION.InformationBufferLength < byteWritten)
    {
        Request.DATA.QUERY_INFORMATION.BytesNeeded = byteWritten;
        return STATUS_BUFFER_TOO_SMALL;
    }

    NDIS_STATISTICS_INFO* statistics =
        (NDIS_STATISTICS_INFO*) Request.DATA.QUERY_INFORMATION.InformationBuffer;

    statistics->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    statistics->Header.Revision = NDIS_STATISTICS_INFO_REVISION_1;
    statistics->Header.Size = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;

    // Set everything as supported even if we support only total bytes and
    // errors now. When we provide the API for client drivers, they will
    // be responsible for populating  all the statistics.
    statistics->SupportedStatistics =
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV  |
        NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV            |
        NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS         |
        NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR            |
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT|
        NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT|
        NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT           |
        NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR           |
        NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS        |
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV   |
        NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV  |
        NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV  |
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT  |
        NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT;

    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties = {};
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    // Populate receive statistics
    KLockThisShared lock(m_statisticsLock);

    statistics->ifHCInOctets = GetRxCounter(NxStatisticsCounters::BytesOfData);

    // Report total packets received as the number of directed packets for MBB. Statmon UI
    // displays only number of directed packets received.
    if (adapterProperties.MediaType == NdisMediumWirelessWan)
    {
        statistics->ifHCInUcastPkts = GetRxCounter(NxStatisticsCounters::NumberOfPackets);
    }

    lock.Release();

    // Populate transmit statistics
    statistics->ifOutErrors = m_txStatistics.GetCounter(NxStatisticsCounters::NumberOfErrors);
    statistics->ifHCOutOctets = m_txStatistics.GetCounter(NxStatisticsCounters::BytesOfData);


    Request.DATA.QUERY_INFORMATION.BytesWritten = byteWritten;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReportTxPacketStatistics(
    NDIS_OID_REQUEST & Request
) const
{
    if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG64))
    {
        *(ULONG64*)Request.DATA.QUERY_INFORMATION.InformationBuffer = m_txStatistics.GetCounter(NxStatisticsCounters::NumberOfPackets);
        Request.DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG64);
    }
    else if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG32))
    {
        *(ULONG32*)Request.DATA.QUERY_INFORMATION.InformationBuffer = (ULONG32) m_txStatistics.GetCounter(NxStatisticsCounters::NumberOfPackets);
        Request.DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG32);
    }
    else
    {
        Request.DATA.QUERY_INFORMATION.BytesNeeded = sizeof(ULONG32);
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReportRxPacketStatistics(
    NDIS_OID_REQUEST & Request
) const
{
        if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG64))
    {
        *(ULONG64*)Request.DATA.QUERY_INFORMATION.InformationBuffer = GetRxCounter(NxStatisticsCounters::NumberOfPackets);
        Request.DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG64);
    }
    else if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG32))
    {
        *(ULONG32*)Request.DATA.QUERY_INFORMATION.InformationBuffer = (ULONG32) GetRxCounter(NxStatisticsCounters::NumberOfPackets);
        Request.DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG32);
    }
    else
    {
        Request.DATA.QUERY_INFORMATION.BytesNeeded = sizeof(ULONG32);
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
ULONG64
NxTranslationApp::GetRxCounter(
    NxStatisticsCounters CounterType
) const
{
    ULONG64 count = 0;
    for (size_t i = 0; i < m_rxStatistics.count(); i++)
    {
        count += m_rxStatistics[i].GetCounter(CounterType);
    }

    return count;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReportUlong(
    NDIS_OID_REQUEST & Request,
    ULONG Value
) const
{
    if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG))
    {
        *(ULONG*)Request.DATA.QUERY_INFORMATION.InformationBuffer = Value;
        Request.DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG);
    }
    else
    {
        Request.DATA.QUERY_INFORMATION.BytesNeeded = sizeof(ULONG);
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReportVendorDriverDescription(
    NDIS_OID_REQUEST & Request
) const
{
    if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof('\0'))
    {
        *(char *)Request.DATA.QUERY_INFORMATION.InformationBuffer = '\0';
        Request.DATA.QUERY_INFORMATION.BytesWritten = sizeof('\0');
    }
    else
    {
        Request.DATA.QUERY_INFORMATION.BytesNeeded = sizeof('\0');
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::SetMulticastList(
    NDIS_OID_REQUEST const & Request
)
{
    auto const addressLength = 6;

    if (Request.DATA.SET_INFORMATION.InformationBufferLength % addressLength != 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    auto addressCount = Request.DATA.SET_INFORMATION.InformationBufferLength/addressLength;

    if (addressCount == 0)
    {
        m_adapterDispatch->SetMulticastList(
            m_adapter,
            addressCount,
            nullptr);

        return STATUS_SUCCESS;
    }

    void * memory =
        ExAllocatePoolWithTag(PagedPool, sizeof(IF_PHYSICAL_ADDRESS) * addressCount, 'pAxN');

    IF_PHYSICAL_ADDRESS * addressList;

    if (memory)
    {
        addressList =  new (memory) IF_PHYSICAL_ADDRESS();
    }
    else
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(addressList, sizeof(IF_PHYSICAL_ADDRESS) * addressCount);

    for (size_t i = 0; i < addressCount; i++)
    {
        addressList[i].Length = addressLength;
        RtlCopyMemory(&(addressList[i].Address),
            reinterpret_cast<UCHAR *>(Request.DATA.SET_INFORMATION.InformationBuffer) + (i * addressLength),
            addressLength);
    }

    m_adapterDispatch->SetMulticastList(
        m_adapter,
        addressCount,
        addressList);

    ExFreePoolWithTag(addressList, 'pAxN');

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxTranslationApp::FillPerfCounter(
    _In_ HANDLE PcwInfo,
    _In_ ULONG InstanceId
) const
{
#ifdef _KERNEL_MODE

    DECLARE_UNICODE_STRING_SIZE(name, 256);

    PPCW_CALLBACK_INFORMATION pcwInfoLocal =
        (PPCW_CALLBACK_INFORMATION) PcwInfo;

    for (size_t i = 0; i < m_rxStatistics.count(); i++)
    {
        if (InstanceId == PCW_ANY_INSTANCE_ID ||
            InstanceId == m_rxStatistics[i].m_StatId)
        {
            (void) RtlUnicodeStringPrintf(&name, L"App %d - RX %d", m_AppId, m_rxStatistics[i].m_StatId);

            NETADAPTER_QUEUE_PC perfCounter = {};
            m_rxStatistics[i].GetPerfCounter(&perfCounter);

            AddNetAdapterCxQueueCounterSet(
                pcwInfoLocal->EnumerateInstances.Buffer,
                &name,
                m_rxStatistics[i].m_StatId,
                &perfCounter);
        }
    }

    if (InstanceId == PCW_ANY_INSTANCE_ID ||
        InstanceId == m_txStatistics.m_StatId)
    {
        (void) RtlUnicodeStringPrintf(&name, L"App %d - TX %d", m_AppId, m_txStatistics.m_StatId);

        NETADAPTER_QUEUE_PC perfCounter = {};
        m_txStatistics.GetPerfCounter(&perfCounter);

        AddNetAdapterCxQueueCounterSet(
            pcwInfoLocal->EnumerateInstances.Buffer,
            &name,
            m_txStatistics.m_StatId,
            &perfCounter);
    }
#else

    UNREFERENCED_PARAMETER(PcwInfo);
    UNREFERENCED_PARAMETER(InstanceId);

#endif
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::SetPacketFilter(
    NDIS_OID_REQUEST const & Request
)
{
    if (Request.DATA.SET_INFORMATION.InformationBufferLength < sizeof(ULONG)) 
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

   return m_adapterDispatch->SetPacketFilter(
       m_adapter,
       *reinterpret_cast<ULONG *>(Request.DATA.QUERY_INFORMATION.InformationBuffer));
}

_Use_decl_annotations_
ULONG
NxTranslationApp::GetMaximumPacketSize(
    void
) const
{
    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties = {};
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    auto const maxL2HeaderSize =
        adapterProperties.MediaType == NdisMedium802_3
            ? 22 // sizeof(ETHERNET_HEADER) + sizeof(SNAP_HEADER)
            : 0;
    auto const capabilities = GetDatapathCapabilities();
    return capabilities.NominalMtu + maxL2HeaderSize;
}

_Use_decl_annotations_
ULONG
NxTranslationApp::GetTxFrameSize(
    void
) const
{
    auto const capabilities = GetDatapathCapabilities();
    return static_cast<ULONG>(capabilities.TxPayloadBackfill) + GetMaximumPacketSize();
}

_Use_decl_annotations_
ULONG
NxTranslationApp::GetRxFrameSize(
    void
) const
{
    auto const capabilities = GetDatapathCapabilities();
    return static_cast<ULONG>(capabilities.MaximumRxFragmentSize);
}

_Use_decl_annotations_
ULONG
NxTranslationApp::GetTxFragmentRingSize(
    void
) const
{
    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties = {};
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    NX_PERF_TX_NIC_CHARACTERISTICS perfCharacteristics = {};
    NX_PERF_TX_TUNING_PARAMETERS perfParameters;
    perfCharacteristics.Nic.IsDriverVerifierEnabled = !!adapterProperties.DriverIsVerifying;

    auto const capabilities = GetDatapathCapabilities();
    perfCharacteristics.FragmentRingNumberOfElementsHint = capabilities.PreferredTxFragmentRingSize;
    perfCharacteristics.NominalLinkSpeed = capabilities.NominalMaxTxLinkSpeed;
    perfCharacteristics.MaxPacketSizeWithLso = capabilities.MtuWithLso;

    NxPerfTunerCalculateTxParameters(&perfCharacteristics, &perfParameters);

    return perfParameters.FragmentRingElementCount;
}

_Use_decl_annotations_
ULONG
NxTranslationApp::GetRxFragmentRingSize(
    void
) const
{
    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties = {};
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    NX_PERF_RX_NIC_CHARACTERISTICS perfCharacteristics = {};
    NX_PERF_RX_TUNING_PARAMETERS perfParameters;
    perfCharacteristics.Nic.IsDriverVerifierEnabled = !!adapterProperties.DriverIsVerifying;

    auto const capabilities = GetDatapathCapabilities();
    perfCharacteristics.FragmentRingNumberOfElementsHint = capabilities.PreferredRxFragmentRingSize;
    perfCharacteristics.NominalLinkSpeed = capabilities.NominalMaxRxLinkSpeed;

    NxPerfTunerCalculateRxParameters(&perfCharacteristics, &perfParameters);

    return perfParameters.FragmentRingElementCount;
}
