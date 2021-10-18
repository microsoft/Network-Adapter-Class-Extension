// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements the driver-global state for the NBL-to-NET_PACKET translation
    app in NetAdapterCx.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndis/oidrequest.h>
#include <ntddndis_p.h>

#include "NxTranslationApp.tmh"

#include "NxXlat.hpp"
#include "NxTranslationApp.hpp"
#include "NxPerfTuner.hpp"

#include "NxNblDatapath.hpp"

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

_IRQL_requires_(PASSIVE_LEVEL)
static
void
NetClientAdapterWifiDestroyPeerAddressDatapath(
    _In_ PVOID ClientContext,
    _In_ SIZE_T Demux
)
{
    reinterpret_cast<NxTranslationApp *>(ClientContext)->WifiDestroyPeerAddressDatapath(Demux);
}

_IRQL_requires_(PASSIVE_LEVEL)
static
void
NetClientAdapterPauseOffloadCapabilities(
    _In_ PVOID ClientContext
)
{
    reinterpret_cast<NxTranslationApp *>(ClientContext)->PauseOffloadCapabilities();
}

_IRQL_requires_(PASSIVE_LEVEL)
static
void
NetClientAdapterResumeOffloadCapabilities(
    _In_ PVOID ClientContext
)
{
    reinterpret_cast<NxTranslationApp *>(ClientContext)->ResumeOffloadCapabilities();
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

        case OID_TCP_RSC_STATISTICS:
            *Status = app->ReportRscStatistics(*Request);
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
    &NetClientAdapterOffloadInitialize,
    &NetClientAdapterWifiDestroyPeerAddressDatapath,
    &NetClientAdapterPauseOffloadCapabilities,
    &NetClientAdapterResumeOffloadCapabilities
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
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! newApp);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        newApp->Initialize());

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
    m_nblDatapath(&m_defaultNblDatapath),
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

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NxTranslationApp::Initialize(
    void
)
{
    auto queueControl = wil::make_unique_nothrow<QueueControl>(
        *this,
        m_dispatch,
        m_adapter,
        m_adapterDispatch,
        m_txQueues,
        m_rxQueues);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! queueControl);

    m_queueControl = wistd::move(queueControl);

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
NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION
NxTranslationApp::GetTxDemuxConfiguration(
    void
) const
{
    NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION configuration;
    m_adapterDispatch->GetTxDemuxConfiguration(m_adapter, &configuration);

    return configuration;
}

_Use_decl_annotations_
PAGEDX
NTSTATUS
NxTranslationApp::ReceiveScalingInitialize(
    void
)
{
    KLockThisExclusive lock(m_lock);

    if (m_rxScaling)
    {
        // It's possible that multiple protocols will try to intialize RSS.
        // It's up to the miniport to handle this so return STATUS_SUCCESS
        // to avoid reinitialization.
        CX_RETURN_STATUS_SUCCESS_MSG("Rss has already been initalized");
    }

    auto rxScaling = wil::make_unique_nothrow<RxScaling>(
        *this,
        this->m_rxQueues,
        this->m_adapterDispatch->ReceiveScalingDispatch);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! rxScaling);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        rxScaling->Initialize());

    m_rxScaling = wistd::move(rxScaling);

    auto status = m_queueControl->CreateRxScalingQueues(m_rxScaling);

    if (status != STATUS_SUCCESS)
    {
        m_rxScaling.reset();
        return status;
    }

    m_queueControl->SignalQueueStateChange();

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReceiveScalingSetParameters(
    NDIS_OID_REQUEST const & Request
)
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_NOT_SUPPORTED,
        ! m_rxScaling);

    return m_rxScaling->SetParameters(Request);
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReceiveScalingSetIndirectionEntries(
    NDIS_OID_REQUEST & Request
)
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_NOT_SUPPORTED,
        ! m_rxScaling);

    return m_rxScaling->SetIndirectionEntries(Request);
}

_Use_decl_annotations_
void
NxTranslationApp::GenerateDemuxProperties(
    NET_BUFFER_LIST const * NetBufferList,
    Rtl::KArray<NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY> & Properties
)
{
    m_txScaling->GenerateDemuxProperties(NetBufferList, Properties);
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::TxScalingInitialize(

)
{
    NT_FRE_ASSERT(! m_txScaling);

    auto txScaling = wil::make_unique_nothrow<TxScaling>(
        *this,
        m_nblDatapath,
        this->m_txQueues);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! txScaling);

    auto const configuration = GetTxDemuxConfiguration();
    CX_RETURN_IF_NOT_NT_SUCCESS(
        txScaling->Initialize(configuration));

    m_txScaling = wistd::move(txScaling);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGEDX
void
NxTranslationApp::StartDatapath(
    void
)
{
    m_nblDatapath->SetRxHandler(&m_rxBufferReturn);
    m_nblDatapath->SetTxHandler(m_txScaling.get());
    m_queueControl->StartQueues();

    CX_RETURN_MSG(
        "Datapath Started"
        " Tx Queues %Iu"
        " Rx Queues %Iu",
        m_txQueues.count(),
        m_rxQueues.count());
}

_Use_decl_annotations_
PAGEDX
void
NxTranslationApp::StopDatapath(
    void
)
{
    m_nblDatapath->SetRxHandler(nullptr);
    m_nblDatapath->CloseTxHandler();
    m_queueControl->CancelQueues();
    m_nblDatapath->SetTxHandler(nullptr);
    m_queueControl->StopQueues();

    CX_RETURN_MSG(
        "Datapath Stopped"
        " Tx Queues %Iu"
        " Rx Queues %Iu",
        m_txQueues.count(),
        m_rxQueues.count());
}

_Use_decl_annotations_
PAGEDX
NTSTATUS
NxTranslationApp::CreateDatapath(
    void
)
{
    KLockThisExclusive lock(m_lock);

    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties = {};
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    InterlockedExchangePointer(
        reinterpret_cast<PVOID *>(&m_nblDatapath),
        static_cast<INxNblDispatcher *>(adapterProperties.NblDispatcher));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        TxScalingInitialize());

    auto const numberOfTxQueues = m_txScaling->GetNumberOfQueues() > 1 ? 0U : 1U;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_queueControl->CreateQueues(numberOfTxQueues, 1U));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_queueControl->ProvisionOnDemandQueues(m_txScaling->GetNumberOfQueues()));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_queueControl->CreateRxScalingQueues(m_rxScaling));

    m_datapathCreated = true;

    CX_RETURN_STATUS_SUCCESS_MSG(
        "Datapath Created"
        " Tx Queues %Iu"
        " Rx Queues %Iu",
        m_txQueues.count(),
        m_rxQueues.count());
}

_Use_decl_annotations_
PAGEDX
void
NxTranslationApp::DestroyDatapath(
    void
)
{
    m_datapathCreated = false;

    m_txScaling.reset();
    UpdateTxCounters();
    UpdateRxCounters();
    m_queueControl->DestroyQueues();

    CX_RETURN_MSG(
        "Datapath Destroyed"
        " Tx Queues %Iu"
        " Rx Queues %Iu",
        m_txQueues.count(),
        m_rxQueues.count());
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
    return m_nblDatapath->QueryStatisticsInfo(Request);
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReportTxPacketStatistics(
    NDIS_OID_REQUEST & Request
) const
{
    auto const & counters = static_cast<NxNblDatapath const *>(m_nblDatapath)->Counters.Tx;
    auto const count = counters.Unicast.Packets + counters.Multicast.Packets + counters.Broadcast.Packets;

    if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG64))
    {
        *(ULONG64*)Request.DATA.QUERY_INFORMATION.InformationBuffer = count;
        Request.DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG64);
    }
    else if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG32))
    {
        *(ULONG32*)Request.DATA.QUERY_INFORMATION.InformationBuffer = (ULONG32)count;
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
    auto const & counters = static_cast<NxNblDatapath const *>(m_nblDatapath)->Counters.Rx;
    auto const count = counters.Unicast.Packets + counters.Multicast.Packets + counters.Broadcast.Packets;

    if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG64))
    {
        *(ULONG64*)Request.DATA.QUERY_INFORMATION.InformationBuffer = count;
        Request.DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG64);
    }
    else if (Request.DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG32))
    {
        *(ULONG32*)Request.DATA.QUERY_INFORMATION.InformationBuffer = (ULONG32)count;
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
NxTranslationApp::ReportRscStatistics(
    NDIS_OID_REQUEST & Request
) const
{
    return m_nblDatapath->QueryRscStatisticsInfo(Request);
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
    auto const buffer = reinterpret_cast<UCHAR *>(Request.DATA.SET_INFORMATION.InformationBuffer);
    auto const bufferLength = Request.DATA.SET_INFORMATION.InformationBufferLength;

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        bufferLength % addressLength != 0,
        "InformationBuffer contains invalid multicast address based on length check.");

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        ! (buffer + bufferLength > buffer),
        "InformationBuffer + InformationBufferLength results in integer overflow.");

    auto addressCount = bufferLength / addressLength;

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_multicastAddressList.resize(addressCount),
        "Insufficient resource to store new multicast addresses.");

    for (size_t i = 0U; i < addressCount; i++)
    {
        m_multicastAddressList[i].Length = addressLength;
        RtlCopyMemory(
            &m_multicastAddressList[i].Address,
            buffer + i * addressLength,
            addressLength);
    }

    auto addressList = m_multicastAddressList.count() > 0U
        ? &m_multicastAddressList[0]
        : reinterpret_cast<IF_PHYSICAL_ADDRESS const *>(nullptr);

    m_adapterDispatch->SetReceiveFilter(
        m_adapter,
        m_packetFilter,
        addressCount,
        addressList);

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

    NETADAPTER_QUEUE_PC perfCounter = {};

    ULONG rxQueueCount = static_cast<ULONG>(m_rxQueues.count());
    for (size_t i = 0; i < m_rxQueues.count(); i++)
    {
        auto rxStatistics = m_rxQueues[i]->GetStatistics();
        if (InstanceId == PCW_ANY_INSTANCE_ID ||
            InstanceId == i)
        {
            (void) RtlUnicodeStringPrintf(&name, L"App %lu - RX %zu", m_AppId, i);

            rxStatistics.GetPerfCounter(perfCounter);

            AddNetAdapterCxQueueCounterSet(
                pcwInfoLocal->EnumerateInstances.Buffer,
                &name,
                static_cast<ULONG>(i),
                &perfCounter);
        }
    }

    for (size_t i = 0; i < m_txQueues.count(); i++)
    {
        auto txStatistics = m_txQueues[i]->GetStatistics();
        if (InstanceId == PCW_ANY_INSTANCE_ID ||
            InstanceId == (i + rxQueueCount))
        {
            (void) RtlUnicodeStringPrintf(&name, L"App %lu - TX %zu", m_AppId, i + rxQueueCount);

            txStatistics.GetPerfCounter(perfCounter);

            AddNetAdapterCxQueueCounterSet(
                pcwInfoLocal->EnumerateInstances.Buffer,
                &name,
                static_cast<ULONG>(i + rxQueueCount),
                &perfCounter);
        }
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
    auto const buffer = reinterpret_cast<ULONG *>(Request.DATA.SET_INFORMATION.InformationBuffer);
    auto const bufferLength = Request.DATA.SET_INFORMATION.InformationBufferLength;

    CX_RETURN_NTSTATUS_IF(
        STATUS_BUFFER_TOO_SMALL,
        bufferLength < sizeof(ULONG));

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        ! (buffer + bufferLength > buffer),
        "InformationBuffer + InformationBufferLength results in integer overflow.");

    m_packetFilter = *buffer;
    auto addressList = m_multicastAddressList.count() > 0U
        ? &m_multicastAddressList[0]
        : reinterpret_cast<IF_PHYSICAL_ADDRESS const *>(nullptr);

    return m_adapterDispatch->SetReceiveFilter(
       m_adapter,
       m_packetFilter,
       m_multicastAddressList.count(),
       addressList);
}

_Use_decl_annotations_
size_t
NxTranslationApp::WifiTxPeerAddressDemux(
    NET_CLIENT_EUI48_ADDRESS const * Address
) const
{
    return m_adapterDispatch->WifiTxPeerDemux(
        m_adapter,
        Address);
}

_Use_decl_annotations_
void
NxTranslationApp::WifiDestroyPeerAddressDatapath(
    size_t Demux
)
{
    NT_FRE_ASSERT(Demux != SIZE_T_MAX);

    Demux += 1U;

    LogInfo(FLAG_TRANSLATOR,
        "Tx Destroy Peer Datapath %Iu",
        Demux);

    for (auto const & queue : m_txQueues)
    {
        auto const property = queue->GetTxDemuxProperty(TxDemuxTypePeerAddress);
        if (property && property->Value == Demux)
        {
            m_queueControl->DestroyOnDemandQueue(queue->GetQueueId());
        }
    }
}

_Use_decl_annotations_
void
NxTranslationApp::PauseOffloadCapabilities(
    void
)
{
    m_offload.PauseOffloadCapabilities();
}

_Use_decl_annotations_
void
NxTranslationApp::ResumeOffloadCapabilities(
    void
)
{
    m_offload.ResumeOffloadCapabilities();
}

_Use_decl_annotations_
ULONG
NxTranslationApp::GetMaximumPacketSize(
    void
) const
{
    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties = {};
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    ULONG maxL2HeaderSize;
    switch(adapterProperties.MediaType)
    {
        case NdisMedium802_3:
            maxL2HeaderSize = 14; // sizeof(ETHERNET_HEADER)
            break;

        case NdisMediumNative802_11:
            maxL2HeaderSize = 32; // sizeof(DOT11_QOS_DATA_LONG_HEADER)
            break;

        default:
            maxL2HeaderSize = 0;
    }

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
    perfCharacteristics.MaxPacketSizeWithGso = capabilities.MtuWithGso;

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

_Use_decl_annotations_
void
NxTranslationApp::UpdateRxCounters(
    void
)
{
    for (auto &queue : m_rxQueues)
    {
        m_statistics.UpdateRxCounters(queue.get()->GetStatistics());
    }
}

_Use_decl_annotations_
void
NxTranslationApp::UpdateTxCounters(
    void
)
{
    for (auto &queue : m_txQueues)
    {
        m_statistics.UpdateTxCounters(queue.get()->GetStatistics());
    }
}

_Use_decl_annotations_
NxRxCounters
NxTranslationApp::GetRxCounters(
    void
) const
{
    NxRxCounters statistics = {};
    statistics.Add(m_statistics.GetRxCounters());

    if (!m_datapathCreated)
    {
        return statistics;
    }

    for (auto &queue: m_rxQueues)
    {
        statistics.Add(queue.get()->GetStatistics());
    }

    return statistics;
}

_Use_decl_annotations_
NxTxCounters
NxTranslationApp::GetTxCounters(
    void
) const
{
    NxTxCounters statistics = {};
    statistics.Add(m_statistics.GetTxCounters());

    if (!m_datapathCreated)
    {
        return statistics;
    }

    for (auto &queue : m_txQueues)
    {
        statistics.Add(queue.get()->GetStatistics());
    }

    return statistics;
}

_Use_decl_annotations_
bool
NxTranslationApp::IsActiveRscIPv4Enabled(
    void
) const
{
    return m_offload.IsActiveRscIPv4Enabled();
}

_Use_decl_annotations_
bool
NxTranslationApp::IsActiveRscIPv6Enabled(
    void
) const
{
    return m_offload.IsActiveRscIPv6Enabled();
}
