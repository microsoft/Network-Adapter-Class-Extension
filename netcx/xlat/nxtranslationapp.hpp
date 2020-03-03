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

class NxTranslationApp :
    public INxApp
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxTranslationApp(
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
    ) noexcept;

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

};

