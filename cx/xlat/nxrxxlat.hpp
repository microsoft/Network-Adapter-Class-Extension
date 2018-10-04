// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the
    receive path.

--*/

#pragma once

#include <NetClientAdapter.h>

#include "NxExecutionContext.hpp"
#include "NxSignal.hpp"
#include "NxRingBuffer.hpp"
#include "NxContextBuffer.hpp"
#include "NxNbl.hpp"
#include "NxNblQueue.hpp"

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
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
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

    void
    ReportCounters(
        void
        );

private:

    size_t m_queueId = ~0U;

    volatile LONG m_groupAffinityChanged = false;

    GROUP_AFFINITY m_groupAffinity = {};

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

    struct ArmedNotifications
    {
        union
        {
            struct
            {
                bool ShouldArmRxIndication : 1;
                bool ShouldArmNblReturned : 1;
                UINT8 Reserved : 6;
            } Flags;

            UINT8 Value = 0;
        };
    } m_lastArmedNotifications;

    NxExecutionContext m_executionContext;

    NET_CLIENT_DISPATCH const * m_dispatch = nullptr;
    NET_CLIENT_ADAPTER m_adapter = nullptr;
    NET_CLIENT_ADAPTER_DISPATCH const * m_adapterDispatch = nullptr;
    NET_CLIENT_ADAPTER_PROPERTIES m_adapterProperties = {};
    NET_CLIENT_BUFFER_POOL m_bufferPool = nullptr;
    NET_CLIENT_BUFFER_POOL_DISPATCH const * m_bufferPoolDispatch = nullptr;

    NDIS_MEDIUM m_mediaType;
    INxNblDispatcher *m_nblDispatcher = nullptr;

    unique_nbl_pool m_netBufferListPool;
    size_t m_NumOfNblsInUse = 0;
    Rtl::KArray<PNET_BUFFER_LIST, NonPagedPoolNx> m_NblLookupTable;
    KPoolPtrNP<MDL> m_MdlPool;

    NET_CLIENT_MEMORY_MANAGEMENT_MODE m_rxBufferAllocationMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER;
    size_t m_rxDataBufferSize = 0;
    UINT32 m_rxNumDataBuffers = 0;
    UINT32 m_rxNumNbls = 0;
    UINT32 m_rxNumPackets = 0;
    UINT32 m_rxNumFragments = 0;
    size_t m_backfillSize = 0;

    NBL_QUEUE m_discardedNbl;
    NxNblQueue m_returnedNblQueue;

    NET_CLIENT_QUEUE m_queue = nullptr;
    NET_CLIENT_QUEUE_DISPATCH const * m_queueDispatch = nullptr;
    size_t m_checksumOffset = NET_PACKET_EXTENSION_INVALID_OFFSET;

    NET_DATAPATH_DESCRIPTOR const * m_descriptor;
    NxRingBuffer m_ringBuffer;
    NxContextBuffer m_contextBuffer;

    // changes as translation routine runs
    ULONG m_outstandingPackets = 0;
    ULONG m_postedPackets = 0;
    ULONG m_returnedPackets = 0;

#ifdef _KERNEL_MODE
    KTIMER m_CounterReportTimer;
    KDPC m_CounterReportDpc;
#endif

    // notification signals
    NxInterlockedFlag m_returnedNblNotification;

    ArmedNotifications
    GetNotificationsToArm();

    void
    ArmNotifications(
        _In_ ArmedNotifications notifications
        );

    void
    ArmNetBufferListReturnedNotification();

    void
    ArmAdapterRxNotification();

    bool m_memoryPreallocated = false;

    NTSTATUS
    AttachEmptyDataBufferToNetPacket(
        _Inout_ PNET_PACKET Packet,
        _Out_ PNET_BUFFER_LIST* Nbl);

    bool
    TransferDataBufferFromNetPacketToNbl(
        _In_ PNET_PACKET Packet,
        _In_ PNET_BUFFER_LIST Nbl);

    bool
    ReInitializeMdlForDataBuffer(
        _In_ PNET_BUFFER nb,
        _In_ NET_PACKET_FRAGMENT const* fragment,
        _In_ PMDL fragmentMdl,
        _In_ bool isFirstFragment);

    PNET_BUFFER_LIST
    DrawNblFromPool();

    void
    ReturnNblToPool(_In_ PNET_BUFFER_LIST);

    PNET_BUFFER_LIST
    FreeReceivedDataBuffer(_In_ PNET_BUFFER_LIST nbl);

    void
    ReinitializePacketExtensions(
        _In_ NET_PACKET* netPacket
        );

    void
    ReinitializePacket(
        _In_ NET_PACKET* netPacket
        );

    // functions called within the EC thread
    void
    EcReturnBuffers();

    void
    EcRecoverBuffers();

    void
    EcPrepareBuffersForNetAdapter();

    void
    EcUpdateAffinity();

    void
    EcYieldToNetAdapter();

    void
    EcIndicateNblsToNdis();

    void
    WaitForWork();

    NTSTATUS
    CreateVariousPools();

    NTSTATUS
    PreparePacketExtensions(
        _Inout_ Rtl::KArray<NET_CLIENT_PACKET_EXTENSION>& addedPacketExtensions
        );

    size_t
    GetPacketExtensionOffsets(
        PCWSTR ExtensionName,
        ULONG ExtensionVersion
        ) const;

    bool
    IsPacketChecksumEnabled() const;

    void
    SetupRxThreadProperties();

    bool m_shouldReportCounters = false;
    ULONG m_counterReportInterval = 0;
    bool m_shouldUpdateEcCounters = false;

#ifdef  _KERNEL_MODE
    static
    KDEFERRED_ROUTINE CounterReportDpcRoutine;
#endif //  _KERNEL_MODE
};

