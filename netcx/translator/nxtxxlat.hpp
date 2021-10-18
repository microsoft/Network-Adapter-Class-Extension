// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the
    transmit path.

--*/

#pragma once

#include <NetClientAdapter.h>

#include "Queue.hpp"
#include "NxSignal.hpp"
#include "NxRingBuffer.hpp"
#include "NxRingContext.hpp"
#include "NxNblQueue.hpp"
#include "NxNbl.hpp"
#include "NxNblTranslation.hpp"
#include "NxDma.hpp"
#include "NxPerfTuner.hpp"
#include "NxExtensions.hpp"
#include "NxStatistics.hpp"

#include "checksum/Checksum.hpp"
#include "segmentation/Segmentation.hpp"
#include <KArray.h>
#include <KWaitEvent.h>

struct QueueControlSynchronize;
typedef struct _PKTMON_COMPONENT_CONTEXT PKTMON_COMPONENT_CONTEXT;

class NxTxXlat
    : public Queue
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxTxXlat(
        _In_ NxTranslationApp & App,
        _In_ bool OnDemand,
        _In_ size_t QueueId,
        _In_ QueueControlSynchronize & QueueControl,
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
    ) noexcept;

    ~NxTxXlat(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Create(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    PAGEDX
    void
    Destroy(
        void
    );

    bool
    IsCreated(
        void
    ) const;

    bool
    IsOnDemand(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsQueuingNetBufferLists(
        void
    ) const;

    // Polling function executed by execution context
    void
    TransmitNbls(
        _Inout_ EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
    );

    void
    TransmitNblsStopping(
        _Inout_ EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
    );

    void
    Notify(
        void
    );

    NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY const *
    NxTxXlat::GetTxDemuxProperty(
        NET_CLIENT_QUEUE_TX_DEMUX_TYPE Type
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    QueueNetBufferList(
        _In_ NET_BUFFER_LIST * NetBufferList
    );

    const NxTxCounters &
    GetStatistics(
        void
    ) const;

    void
    DropQueuedNetBufferLists(
        void
    );

private:

    bool
        m_created = false;

    bool const
        m_onDemand = false;

    QueueControlSynchronize &
        m_queueControl;

    NET_CLIENT_DISPATCH const *
        m_dispatch = nullptr;

    NET_CLIENT_ADAPTER
        m_adapter = nullptr;

    NET_CLIENT_ADAPTER_DISPATCH const *
        m_adapterDispatch = nullptr;

    NxTxCounters
        m_statistics = {};

    NET_CLIENT_ADAPTER_PROPERTIES
        m_adapterProperties = {};

    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES
        m_datapathCapabilities = {};

    INxNblDispatcher *
        m_nblDispatcher = nullptr;

    NxNblQueue
        m_synchronizedNblQueue;

    TxExtensions
        m_extensions = {};

    NET_RING_COLLECTION
        m_rings;

    NxRingBuffer
        m_packetRing;

    NxRingContext
        m_packetContext;

    NxBounceBufferPool
        m_bounceBufferPool;

    wistd::unique_ptr<NxDmaAdapter>
        m_dmaAdapter;

    PKTMON_COMPONENT_CONTEXT const *
        m_pktmonComponentContext = nullptr;

    //
    // Datapath variables
    // All below will change as TransmitThread runs
    //
    NET_BUFFER_LIST *
        m_currentNbl = nullptr;

    NET_BUFFER *
        m_currentNetBuffer = nullptr;

    // These are used to determine when to arm notifications
    // and halt queue operation
    ULONG
        m_producedPackets = 0;

    ULONG
        m_completedPackets = 0;

    Rtl::KArray<UINT8, NonPagedPoolNx>
        m_layoutParseBuffer;

    Rtl::KArray<NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY>
        m_properties;

    Checksum
        m_checksumContext;

    GenericSegmentationOffload
        m_gsoContext;

    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES
        m_checksumHardwareCapabilities = {};

    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        m_txChecksumHardwareCapabilities = {};

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES
        m_gsoHardwareCapabilities = {};

    // The high level operations of the NBL Tx translation
    void
    PollNetBufferLists(
        void
    );

    void
    DrainCompletions(
        void
    );

    void
    TranslateNbls(
        void
    );

    void
    YieldToNetAdapter(
        void
    );

    void
    UpdatePerfCounter(
        void
    );

    // These operations are used exclusively while
    // winding down the Tx path
    void
    AbortNbls(
        _In_opt_ NET_BUFFER_LIST * nblChain
    );

    // drain queue
    PNET_BUFFER_LIST
    DequeueNetBufferListQueue(
        void
    );

};

