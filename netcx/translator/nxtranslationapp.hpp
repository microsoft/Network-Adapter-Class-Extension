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
#include "NxReceiveScaling.hpp"
#include "NxOffload.hpp"
#include "NxStatistics.hpp"
#include "NxCollection.hpp"

#include <KLockHolder.h>

class NxTranslationApp :
    public INxApp
{

    friend class NxTranslationAppFactory;
    friend class NxCollection<NxTranslationApp>;

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
    NTSTATUS
    CreateDatapath(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    StartDatapath(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    StopDatapath(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    DestroyDatapath(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReceiveScalingInitialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReceiveScalingSetParameters(
        _In_ NDIS_OID_REQUEST const & Request
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    ReceiveScalingSetIndirectionEntries(
        _In_ NDIS_OID_REQUEST const & Request
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
    ReportUlong(
        _In_ NDIS_OID_REQUEST & Request,
        _In_ ULONG Value
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportVendorDriverDescription(
        _In_ NDIS_OID_REQUEST & Request
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
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

private:

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    CreateDefaultQueues(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    StartDefaultQueues(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    NTSTATUS
    CreateReceiveScalingQueues(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    StartReceiveScalingQueues(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    ULONG64
    GetRxCounter(
        NxStatisticsCounters CounterType
    ) const;

    wistd::unique_ptr<NxTxXlat>
        m_txQueue;

    Rtl::KArray<wistd::unique_ptr<NxRxXlat>, NonPagedPoolNx>
        m_rxQueues;

    NET_CLIENT_DISPATCH const *
        m_dispatch = nullptr;

    NET_CLIENT_ADAPTER
        m_adapter;

    NET_CLIENT_ADAPTER_DISPATCH const *
        m_adapterDispatch = nullptr;

    INxNblDispatcher *
        m_NblDispatcher = nullptr;

    NxNblRx
        m_rxBufferReturn;

    wistd::unique_ptr<NxReceiveScaling>
        m_receiveScaling;

    bool
        m_receiveScalingDatapath = false;

    bool
        m_datapathCreated = false;

    bool
        m_datapathStarted = false;

    NxTaskOffload
        m_offload;

    NxStatistics
        m_txStatistics;

    Rtl::KArray<NxStatistics, NonPagedPoolNxCacheAligned>
        m_rxStatistics;

    mutable KPushLock
        m_statisticsLock;

    LIST_ENTRY
        m_Linkage = {};

    static ULONG
        s_LastAppId;

public:

    const ULONG
        m_AppId = InterlockedIncrement((LONG *) &s_LastAppId);

    static NxCollection<NxTranslationApp> *
        s_AppCollection;
};

