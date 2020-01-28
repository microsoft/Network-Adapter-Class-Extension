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

#include "NxExecutionContext.hpp"
#include "NxSignal.hpp"
#include "NxRingContext.hpp"
#include "NxNbl.hpp"
#include "NxNblQueue.hpp"
#include "NxExtensions.hpp"
#include "NxStatistics.hpp"
#include "metadata/MetadataTranslator.hpp"

#include <KArray.h>
#include <KStackStorage.h>

using unique_nbl = wistd::unique_ptr<NET_BUFFER_LIST, wil::function_deleter<decltype(&NdisFreeNetBufferList), NdisFreeNetBufferList>>;
using unique_nbl_pool = wil::unique_any<NDIS_HANDLE, decltype(&::NdisFreeNetBufferListPool), &::NdisFreeNetBufferListPool>;

struct MDL_EX;

class MdlPool : public KStackPool<MDL_EX*>
{
public:
    ~MdlPool() {}

    PAGED
    NTSTATUS
    Initialize(_In_ size_t MemorySize,
               _In_ size_t MdlCount);

private:

    KPoolPtrNP<BYTE>        m_MdlStorage;
    size_t                  m_MdlSize = 0;
};

class NblPool : public KStackPool<NET_BUFFER_LIST*>
{
public:
    ~NblPool() {}

    PAGED
    NTSTATUS
    Initialize(_In_ NDIS_HANDLE NdisHandle,
               _In_ size_t NdlCount,
               _In_ NDIS_MEDIUM Medium);

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

class NxRxXlat :
    public NxNonpagedAllocation<'lXRN'>
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxRxXlat(
        _In_ size_t QueueId,
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
        _In_ NxStatistics & Statistics
    ) noexcept;

    virtual
    ~NxRxXlat(
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
    IsGroupAffinitized(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    SetGroupAffinity(
        GROUP_AFFINITY const & GroupAffinity
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

private:

    size_t
        m_queueId = ~0U;

    volatile LONG
        m_groupAffinityChanged = false;

    GROUP_AFFINITY
        m_groupAffinity = {};

    struct PAGED PacketContext
    {
        //
        // this NBL's NET_BUFFER_MINIPORT_RESERVED(NET_BUFFER_LIST_FIRST_NB)[0]
        // field is used for the following purpose:
        //
        // if Rx buffer allocation mode is none, it's storing the rx buffer
        // return context supplied by the NIC driver
        //
        // if Rx buffer allocation mode is automatic, it's storing the
        // DMA logical address of the Rx buffer automatically chose by the
        // OS
        //
        // if Rx buffer allocation mode is manual, it's not used
        //
        PNET_BUFFER_LIST NetBufferList;
    };

    struct PAGED FragmentContext
    {
        MDL *
            Mdl;
    };

    struct ArmedNotifications
    {
        union
        {
            struct
            {
                bool
                    ShouldArmRxIndication : 1;

                bool
                    ShouldArmNblReturned : 1;

                UINT8
                    Reserved : 6;
            } Flags;

            UINT8 Value = 0;
        };
    } m_lastArmedNotifications;

    NxExecutionContext
        m_executionContext;

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

    NDIS_MEDIUM
        m_mediaType;

    INxNblDispatcher *
        m_nblDispatcher = nullptr;

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

    NET_CLIENT_QUEUE
        m_queue = nullptr;

    NET_CLIENT_QUEUE_DISPATCH const *
        m_queueDispatch = nullptr;

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

    NxStatistics &
        m_statistics;

    ArmedNotifications
    GetNotificationsToArm(
        void
    );

    void
    ArmNotifications(
        _In_ ArmedNotifications notifications
    );

    void
    ArmNetBufferListReturnedNotification(
        void
    );

    void
    ArmAdapterRxNotification(
        void
    );

    bool
        m_memoryPreallocated = false;

    bool
    TransferDataBufferFromNetPacketToNbl(
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
    EcUpdateAffinity(
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

    void
    EcWaitForWork(
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

    NTSTATUS
    CreateVariousPools(
        void
    );

    void
    SetupRxThreadProperties(
        void
    );

};

