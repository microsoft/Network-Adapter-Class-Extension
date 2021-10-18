// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the
    receive path.

--*/

#pragma once

#include <net/packet.h>
#include <net/fragment.h>
#include <net/virtualaddress.h>

#include <NetClientAdapter.h>

#include "Queue.hpp"
#include "NxSignal.hpp"
#include "NxRingContext.hpp"
#include "NxNbl.hpp"
#include "NxNblQueue.hpp"
#include "NxExtensions.hpp"
#include "NxStatistics.hpp"
#include "metadata/MetadataTranslator.hpp"
#include "coalescing/Coalescing.hpp"

#include <KArray.h>
#include <KStackStorage.h>

using unique_nbl = wistd::unique_ptr<NET_BUFFER_LIST, wil::function_deleter<decltype(&NdisFreeNetBufferList), NdisFreeNetBufferList>>;
using unique_nbl_pool = wil::unique_any<NDIS_HANDLE, decltype(&::NdisFreeNetBufferListPool), &::NdisFreeNetBufferListPool>;

struct MDL_EX;
class NxNblSequence;
typedef struct _PKTMON_EDGE_CONTEXT PKTMON_EDGE_CONTEXT;

class MdlPool 
    : public KStackPool<MDL_EX*>
{
public:

    PAGED
    NTSTATUS
    Initialize(
        _In_ size_t MemorySize,
        _In_ size_t MdlCount,
        _In_ bool IsPreBuilt
    );

    bool
    IsPreBuilt(
        void
    ) const { return m_IsPreBuilt; };

private:

    KPoolPtrNP<BYTE>
        m_MdlStorage;

    size_t
        m_MdlSize = 0;

    bool
        m_IsPreBuilt = false;
};

class NblPool 
    : public KStackPool<NET_BUFFER_LIST*>
{
public:

    PAGED
    NTSTATUS
    Initialize(
        _In_ NDIS_HANDLE NdisHandle,
        _In_ size_t NdlCount,
        _In_ NDIS_MEDIUM Medium
    );

private:

    unique_nbl_pool m_NblStorage;
};


class NxNblRx :
    public INxNblRx,
    public NxNonpagedAllocation<'lXRN'>
{
    //
    // INxNblRx
    //
    virtual
    ULONG
    ReturnNetBufferLists(
        _In_ NET_BUFFER_LIST * NblChain,
        _In_ ULONG ReceiveCompleteFlags
    );
};

class NxRxXlat
    : public Queue
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxRxXlat(
        _In_ NxTranslationApp & App,
        _In_ size_t QueueId,
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
    ) noexcept;

    ~NxRxXlat(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    // the EC thread function
    void
    ReceiveThread(
        void
    );

    NET_CLIENT_QUEUE
    GetQueue(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsAffinitized(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SetAffinity(
        PROCESSOR_NUMBER const & Affinity
    );

    void
    Notify(
        void
    );

    void
    QueueReturnedNetBufferLists(
        _In_ NBL_QUEUE* NblChain
    );

    PMDL
    GetCorrespondingMdl(
        _In_ UINT32 fragmentIndex
    );

    const NxRxCounters &
    GetStatistics(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    ReceiveNbls(
        _Inout_ EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    ReceiveNblsStopping(
        _Inout_ EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
    );

private:

    bool
        m_affinitySet = false;

    struct PacketContext
    {
        PNET_BUFFER_LIST NetBufferList;
    };

    struct FragmentContext
    {
        MDL *
            Mdl;
    };

    NET_CLIENT_DISPATCH const *
        m_dispatch = nullptr;

    NET_CLIENT_ADAPTER
        m_adapter = nullptr;

    NET_CLIENT_ADAPTER_DISPATCH const *
        m_adapterDispatch = nullptr;

    NET_CLIENT_ADAPTER_PROPERTIES
        m_adapterProperties = {};

    NET_CLIENT_BUFFER_POOL
        m_bufferPool = nullptr;

    NET_CLIENT_BUFFER_POOL_DISPATCH const *
        m_bufferPoolDispatch = nullptr;

    INxNblDispatcher *
        m_nblDispatcher = nullptr;

    PKTMON_EDGE_CONTEXT const *
        m_pktmonLowerEdgeContext = nullptr;

    MdlPool
        m_MdlPool;

    NblPool
        m_NblPool;

    NET_CLIENT_MEMORY_MANAGEMENT_MODE
        m_rxBufferAllocationMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER;

    size_t
        m_rxDataBufferSize = 0;

    UINT32
        m_rxNumPackets = 0;

    UINT32
        m_rxNumFragments = 0;

    UINT32
        m_rxNumDataBuffers = 0;

    size_t
        m_backfillSize = 0;

    NBL_QUEUE
        m_discardedNbl;

    NxNblQueue
        m_returnedNblQueue;

    RxExtensions
        m_extensions = {};

    NET_RING_COLLECTION
        m_rings;

    RxMetadataTranslator
        m_metadataTranslator;

    NxRingContext
        m_packetContext;

    NxRingContext
        m_fragmentContext;

    // changes as translation routine runs
    ULONG
        m_outstandingNbls = 0;

    ULONG
        m_returnedNbls = 0;

    ULONG
        m_completedPackets = 0;

    // notification signals
    NxInterlockedFlag
        m_returnedNblNotification;

    NxRxCounters
        m_statistics = {};

    bool
        m_memoryPreallocated = false;

    ReceiveSegmentCoalescing
        m_rscContext;

    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES
        m_rscHardwareCapabilities = {};

    bool
    TransferDataBufferFromNetPacketToNbl(
        _Inout_ RxPacketCounters & Counters,
        _In_ NET_PACKET * Packet,
        _In_ PNET_BUFFER_LIST Nbl,
        _In_ UINT32 PacketIndex
    );

    bool
    ReInitializeMdlForDataBuffer(
        _In_ NET_FRAGMENT const * fragment,
        _In_ NET_FRAGMENT_VIRTUAL_ADDRESS const * VirtualAddress,
        _In_ NET_FRAGMENT_RETURN_CONTEXT const * ReturnContext,
        _In_ PMDL fragmentMdl,
        _In_ bool isFirstFragment
    );

    PNET_BUFFER_LIST
    FreeReceivedDataBuffer(
        _In_ PNET_BUFFER_LIST nbl
    );

    // functions called within the EC thread
    void
    EcReturnBuffers(
        void
    );

    void
    EcRecoverBuffers(
        void
    );

    void
    EcPrepareBuffersForNetAdapter(
        void
    );

    void
    EcYieldToNetAdapter(
        void
    );

    void
    EcIndicateNblsToNdis(
        void
    );

    void
    EcUpdatePerfCounter(
        void
    );

    static
    void
    EnumerateBufferPoolCallback(
        _In_ PVOID Context,
        _In_ SIZE_T Index,
        _In_ UINT64 LogicalAddress,
        _In_ PVOID VirtualAddress
    );

    void ReclaimSkippedFragments(
        _In_ UINT32 CurrentIndex
    );

    NTSTATUS
    CreateVariousPools(
        void
    );

    void
    UpdateRscStatisticsPreSoftwareFallback(
        _In_ NET_BUFFER_LIST const & NetBufferList,
        _In_ UINT64 HeaderLength,
        _Inout_ NblRscStatistics & RscStatistics
    );

    void
    UpdateRscStatisticsPostSoftwareFallback(
        _In_ ULONG PreRscFallbackNblCount,
        _Inout_ NblRscStatistics & RscStatistics
    );

    void
    CoalesceNblsToIndicate(
        _Inout_ NxNblSequence & NblSequence
    );

    void
    UncoalesceReturnedNbls(
        _Inout_ NBL_QUEUE & NblQueue
    );

    friend class NxNblRx;
};

