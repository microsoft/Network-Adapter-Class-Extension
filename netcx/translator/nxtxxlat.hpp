// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the
    transmit path.

--*/

#pragma once

#include <NetClientAdapter.h>

#include "NxExecutionContext.hpp"
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

#include <KArray.h>

class NxTxXlat :
    public INxNblTx,
    public NxNonpagedAllocation<'xTxN'>
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxTxXlat(
        _In_ size_t QueueId,
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
        _In_ NxStatistics & Statistics
    ) noexcept;

    virtual
    ~NxTxXlat(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    size_t
    GetQueueId(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Start(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Cancel(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Stop(
        void
    );

    void
    TransmitThread(
        void
    );

    void
    Notify(
        void
    );

    //
    // INxNblTx
    //

    virtual
    void
    SendNetBufferLists(
        _In_ NET_BUFFER_LIST * NblChain,
        _In_ ULONG PortNumber,
        _In_ ULONG NumberOfNbls,
        _In_ ULONG SendFlags
    );

private:

    size_t m_queueId = ~0U;

    NxExecutionContext m_executionContext;

    NET_CLIENT_DISPATCH const *
        m_dispatch = nullptr;

    NET_CLIENT_ADAPTER
        m_adapter = nullptr;

    NET_CLIENT_ADAPTER_DISPATCH const *
        m_adapterDispatch = nullptr;

    NET_CLIENT_ADAPTER_PROPERTIES
        m_adapterProperties = {};

    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES
        m_datapathCapabilities = {};

    NDIS_MEDIUM
        m_mediaType;

    INxNblDispatcher *
        m_nblDispatcher = nullptr;

    NxInterlockedFlag
        m_queueNotification;

    NxNblQueue
        m_synchronizedNblQueue;

    NxNblTranslationStats
        m_nblTranslationStats;

    NET_CLIENT_QUEUE
        m_queue = nullptr;

    NET_CLIENT_QUEUE_DISPATCH const *
        m_queueDispatch = nullptr;

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

    struct ArmedNotifications
    {
        union
        {
            struct
            {
                bool ShouldArmTxCompletion : 1;
                bool ShouldArmNblArrival : 1;
                UINT8 Reserved : 6;
            } Flags;

            UINT8 Value = 0;
        };
    } m_lastArmedNotifications;

    NxStatistics &
        m_statistics;

    void
    ArmNetBufferListArrivalNotification(
        void
    );

    void
    ArmAdapterTxNotification(
        void
    );

    ArmedNotifications
    GetNotificationsToArm(
        void
    );

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

    void
    WaitForWork(
        void
    );

    // These operations are used exclusively while
    // winding down the Tx path
    void
    DropQueuedNetBufferLists(
        void
    );

    void
    AbortNbls(
        _In_opt_ NET_BUFFER_LIST * nblChain
    );

    // drain queue
    PNET_BUFFER_LIST
    DequeueNetBufferListQueue(
        void
    );

    void
    ArmNotifications(
        _In_ ArmedNotifications reasons
    );

    void
    SetupTxThreadProperties(
        void
    );

};

