// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements the driver-global state for the NBL-to-NET_PACKET translation
    app in NetAdapterCx.

--*/

#pragma once

#include "NxApp.hpp"
#include "NxTxXlat.hpp"
#include "NxRxXlat.hpp"
#include "RxScaling.hpp"
#include "NxOffload.hpp"
#include "NxStatistics.hpp"
#include "NxCollection.hpp"
#include "TxScaling.hpp"
#include "QueueControl.hpp"

#include "NxNblDatapath.hpp"

#include "NxNblDatapath.hpp"

#include <KLockHolder.h>
#include <KWaitEvent.h>
#include <KArray.h>

class NxTranslationApp :
    public INxApp
{

private:

    friend class NxTranslationAppFactory;
    friend class NxCollection<NxTranslationApp>;

    NxNblDatapath
        m_defaultNblDatapath;

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxTranslationApp(
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
    ) noexcept;

    _IRQL_requires_(PASSIVE_LEVEL)
    ~NxTranslationApp();

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_CLIENT_ADAPTER
    GetAdapter(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetDeviceFailed(
        _In_ NTSTATUS Status
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_ADAPTER_PROPERTIES
    GetProperties(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES
    GetDatapathCapabilities(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES
    GetReceiveScalingCapabilities(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION
    GetTxDemuxConfiguration(
        void
    ) const;

    void
    GenerateDemuxProperties(
        _In_ NET_BUFFER_LIST const * NetBufferList,
        _In_ Rtl::KArray<NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY> & Properties
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    CreateDatapath(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    StartDatapath(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    StopDatapath(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    DestroyDatapath(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    ReceiveScalingInitialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    ReceiveScalingSetParameters(
        _In_ NDIS_OID_REQUEST const & Request
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    ReceiveScalingSetIndirectionEntries(
        _Inout_ NDIS_OID_REQUEST & Request
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    TxScalingInitialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    OffloadInitialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    OffloadSetActiveCapabilities(
        _In_ NDIS_OID_REQUEST const & Request
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    OffloadSetEncapsulation(
        _In_ NDIS_OID_REQUEST const & Request
        );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportInterruptModerationNotSupported(
        _In_ NDIS_OID_REQUEST & Request
        );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportGeneralStatistics(
        _In_ NDIS_OID_REQUEST & Request
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportTxPacketStatistics(
        _In_ NDIS_OID_REQUEST & Request
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportRxPacketStatistics(
        _In_ NDIS_OID_REQUEST & Request
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportRscStatistics(
        _In_ NDIS_OID_REQUEST & Request
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportUlong(
        _In_ NDIS_OID_REQUEST & Request,
        _In_ ULONG Value
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportVendorDriverDescription(
        _In_ NDIS_OID_REQUEST & Request
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    SetMulticastList(
        _In_ NDIS_OID_REQUEST const & Request
    );

    _IRQL_requires_max_(APC_LEVEL)
    void
    FillPerfCounter(
        _In_ HANDLE PcwInfo,
        _In_ ULONG InstanceId
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    SetPacketFilter(
        _In_ NDIS_OID_REQUEST const & Request
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    size_t
    WifiTxPeerAddressDemux(
        NET_CLIENT_EUI48_ADDRESS const * Address
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    WifiDestroyPeerAddressDatapath(
        size_t Demux
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    PauseOffloadCapabilities(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    ResumeOffloadCapabilities(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    ULONG
    GetMaximumPacketSize(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    ULONG
    GetTxFrameSize(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    ULONG
    GetRxFrameSize(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    ULONG
    GetTxFragmentRingSize(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    ULONG
    GetRxFragmentRingSize(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NxRxCounters
    GetRxCounters(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NxTxCounters
    GetTxCounters(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UpdateRxCounters(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UpdateTxCounters(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsActiveRscIPv4Enabled(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsActiveRscIPv6Enabled(
        void
    ) const;

private:

    KPushLock
        m_lock;

    Rtl::KArray<wistd::unique_ptr<NxTxXlat>, NonPagedPoolNx>
        m_txQueues;

    Rtl::KArray<wistd::unique_ptr<NxRxXlat>, NonPagedPoolNx>
        m_rxQueues;

    NET_CLIENT_DISPATCH const *
        m_dispatch = nullptr;

    NET_CLIENT_ADAPTER
        m_adapter;

    NET_CLIENT_ADAPTER_DISPATCH const *
        m_adapterDispatch = nullptr;

    INxNblDispatcher *
        m_nblDatapath = nullptr;

    NxNblRx
        m_rxBufferReturn;

    wistd::unique_ptr<RxScaling>
        m_rxScaling;

    wistd::unique_ptr<TxScaling>
        m_txScaling;

    bool
        m_datapathCreated = false;

    wistd::unique_ptr<QueueControl>
        m_queueControl;

    NxTaskOffload
        m_offload;

    NxStatistics
        m_statistics = {};

    LIST_ENTRY
        m_linkage = {};

    static ULONG
        s_LastAppId;

    Rtl::KArray<IF_PHYSICAL_ADDRESS, NonPagedPoolNx>
        m_multicastAddressList;

    ULONG
        m_packetFilter = 0U;
public:

    const ULONG
        m_AppId = InterlockedIncrement((LONG *) &s_LastAppId);

    static NxCollection<NxTranslationApp> *
        s_AppCollection;
};

