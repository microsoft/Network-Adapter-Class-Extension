// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the
    receive path.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxRxXlat.tmh"
#include "NxRxXlat.hpp"

#include <NetRingBuffer.h>
#include <NetPacket.h>

#include "NxPerfTuner.hpp"
#include "NxPacketLayout.hpp"
#include "NxChecksumInfo.hpp"
#include "NxNblSequence.h"

struct RX_NBL_CONTEXT
{
    NxRxXlat* Queue;
};

RX_NBL_CONTEXT*
GetRxContextFromNbl(PNET_BUFFER_LIST Nbl)
{
    return
        reinterpret_cast<RX_NBL_CONTEXT*>(&NET_BUFFER_LIST_MINIPORT_RESERVED(Nbl)[0]);
}

static_assert(sizeof(RX_NBL_CONTEXT) <= FIELD_SIZE(NET_BUFFER_LIST, MiniportReserved),
              "the size of RX_NBL_CONTEXT struct is larger than available space on NBL reserved for miniport");

struct RX_NB_CONTEXT
{
    union
    {
        //used when system fully manages the Rx buffers (doing both allocate and attach)
        LONGLONG DmaLogicalAddress;
        //used when driver manages the buffers
        PVOID RxBufferReturnContext;
    } DUMMYUNIONNAME;
};

RX_NB_CONTEXT*
GetRxContextFromNb(PNET_BUFFER Nb)
{
    return
        reinterpret_cast<RX_NB_CONTEXT*>(&NET_BUFFER_MINIPORT_RESERVED(Nb)[0]);
}

static_assert(sizeof(RX_NB_CONTEXT) <= FIELD_SIZE(NET_BUFFER, MiniportReserved),
              "the size of RX_NB_CONTEXT struct is larger than available space on NB reserved for miniport");

static
void
NetClientQueueNotify(
    PVOID Queue
    )
{
    reinterpret_cast<NxRxXlat *>(Queue)->Notify();
}

static const NET_CLIENT_QUEUE_NOTIFY_DISPATCH QueueDispatch
{
    { sizeof(NET_CLIENT_QUEUE_NOTIFY_DISPATCH) },
    &NetClientQueueNotify
};

ULONG const MAX_DYNAMIC_PAGES = 16;
ULONG const MAX_DYNAMIC_PACKET_SIZE = (MAX_DYNAMIC_PAGES - 1) * PAGE_SIZE + 1;

constexpr
USHORT
ByteSwap(
    _In_ USHORT in
    )
{
    return (USHORT)((in >> 8) | (in << 8));
}

PNET_BUFFER_LIST
GetLongestSpanWithSameQueue(
    _In_        PNET_BUFFER_LIST inputChain,
    _Out_       PNBL_QUEUE span,
    _Outptr_    NxRxXlat** queue,
    _Out_       PULONG numNbl)
{
    ndisInitializeNblQueue(span);

    NT_ASSERT(inputChain);
    *numNbl = 1;

    PNET_BUFFER_LIST first = inputChain;
    PNET_BUFFER_LIST current = first;
    PNET_BUFFER_LIST next = current->Next;

    *queue = GetRxContextFromNbl(first)->Queue;

    while (true)
    {
        if (next)
        {
            if (*queue == GetRxContextFromNbl(next)->Queue)
            {
                current = next;
                next = current->Next;
                (*numNbl)++;

                continue;
            }
            else
            {
                current->Next = nullptr;
            }
        }

        ndisAppendNblChainToNblQueueFast(span, first, current);
        break;
    }

    return next;
}

ULONG
NxNblRx::ReturnNetBufferLists(
    NET_BUFFER_LIST * NblChain,
    ULONG ReceiveCompleteFlags
)
{
    UNREFERENCED_PARAMETER((ReceiveCompleteFlags));

    ULONG ret = 0;
    PNET_BUFFER_LIST nbl = NblChain;

    do
    {
        NBL_QUEUE span;
        NxRxXlat* queue;
        ULONG numNbl;
        nbl = GetLongestSpanWithSameQueue(nbl, &span, &queue, &numNbl);
        ret += numNbl;

        queue->QueueReturnedNetBufferLists(&span);
    } while (nbl);

    return ret;
}

_Use_decl_annotations_
NxRxXlat::NxRxXlat(
    size_t QueueId,
    NET_CLIENT_DISPATCH const * Dispatch,
    NET_CLIENT_ADAPTER Adapter,
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
    ) noexcept :
    m_queueId(QueueId),
    m_dispatch(Dispatch),
    m_adapter(Adapter),
    m_adapterDispatch(AdapterDispatch),
    m_contextBuffer(m_ringBuffer)
{
    m_adapterDispatch->GetProperties(m_adapter, &m_adapterProperties);
    m_nblDispatcher = static_cast<INxNblDispatcher *>(m_adapterProperties.NblDispatcher);
    ndisInitializeNblQueue(&m_discardedNbl);

    m_shouldReportCounters = m_dispatch->NetClientQueryDriverConfigurationBoolean(RX_REPORT_PERF_COUNTERS);
    m_counterReportInterval = m_dispatch->NetClientQueryDriverConfigurationUlong(RX_PERF_COUNTERS_ITERATION_INTERVAL);
    m_shouldUpdateEcCounters = m_dispatch->NetClientQueryDriverConfigurationBoolean(EC_UPDATE_PERF_COUNTERS);

    if (m_shouldReportCounters)
    {
#ifdef _KERNEL_MODE
        LARGE_INTEGER timerDelay = {};
        KeInitializeTimer(&m_CounterReportTimer);
        KeInitializeDpc(&m_CounterReportDpc, NxRxXlat::CounterReportDpcRoutine, this);

        // in 100ns
        timerDelay.QuadPart = (-1 * MS_TO_100NS_CONVERSION * m_counterReportInterval);

        KeSetTimerEx(&
            m_CounterReportTimer,
            timerDelay,
            m_counterReportInterval,
            &m_CounterReportDpc);
#endif
    }
}

_Use_decl_annotations_
size_t
NxRxXlat::GetQueueId(
    void
    ) const
{
    return m_queueId;
}

NET_CLIENT_QUEUE
NxRxXlat::GetQueue(
    void
    ) const
{
    return m_queue;
}

_Use_decl_annotations_
bool
NxRxXlat::IsGroupAffinitized(
    void
    ) const
{
    return m_groupAffinity.Mask;
}

_Use_decl_annotations_
void
NxRxXlat::SetGroupAffinity(
    GROUP_AFFINITY const & GroupAffinity
    )
{
    m_groupAffinity = GroupAffinity;
    (void)InterlockedExchange(&m_groupAffinityChanged, 1);
}

NxRxXlat::ArmedNotifications
NxRxXlat::GetNotificationsToArm()
{
    ArmedNotifications notifications;

    if (m_postedPackets == 0 && m_returnedPackets == 0)
    {
        notifications.Flags.ShouldArmNblReturned = true;

        notifications.Flags.ShouldArmRxIndication = m_outstandingPackets != 0;
    }

    return notifications;
}

_Use_decl_annotations_
void
NxRxXlat::ArmNotifications(
    ArmedNotifications notifications
    )
{
    if (notifications.Flags.ShouldArmNblReturned)
    {
        ArmNetBufferListReturnedNotification();
    }

    if (notifications.Flags.ShouldArmRxIndication)
    {
        ArmAdapterRxNotification();
    }
}

void
NxRxXlat::ArmNetBufferListReturnedNotification()
{
    m_returnedNblNotification.Set();
}

void
NxRxXlat::ArmAdapterRxNotification()
{
    m_queueDispatch->SetArmed(m_queue, true);
}

_Use_decl_annotations_
void
NxRxXlat::ReinitializePacketExtensions(
    NET_PACKET* netPacket
    )
{
    RtlZeroMemory(netPacket + 1, m_ringBuffer.GetElementSize() - sizeof(NET_PACKET));
}

_Use_decl_annotations_
void
NxRxXlat::ReinitializePacket(
    NET_PACKET* netPacket
    )
{
    NetPacketReuseOne(m_descriptor, netPacket);

    DetachFragmentsFromPacket(*netPacket, *m_descriptor);

    netPacket->Layout = {};
    ReinitializePacketExtensions(netPacket);
}

void
NxRxXlat::EcReturnBuffers()
{
    m_returnedPackets = 0;

    NBL_QUEUE nblsToReturn;
    m_returnedNblQueue.DequeueAll(&nblsToReturn);
    ndisAppendNblQueueToNblQueueFast(&nblsToReturn, &m_discardedNbl);

    auto currNbl = ndisPopAllFromNblQueue(&nblsToReturn);

    while (currNbl)
    {
        ++m_returnedPackets;
        currNbl = FreeReceivedDataBuffer(currNbl);
    }

    NxRingBufferCounters delta = {};
    delta.NumberOfNetPacketsConsumed = m_returnedPackets;
    m_ringBuffer.UpdateRingbufferPacketCounters(delta);
}

void
NxRxXlat::EcPrepareBuffersForNetAdapter()
{
    m_returnedPackets = 0;

    while (auto pCurrentPacket = m_ringBuffer.GetNextPacketToGiveToNic())
    {
        NT_ASSERT(pCurrentPacket->FragmentValid == 0);

        auto & packetContext = m_contextBuffer.GetPacketContext<PacketContext>(*pCurrentPacket);
        NT_ASSERT(packetContext.NetBufferList == nullptr);

        if (!NT_SUCCESS(AttachEmptyDataBufferToNetPacket(pCurrentPacket,
                                                         &packetContext.NetBufferList)))
        {
            break;
        }

        --m_outstandingPackets;

        m_ringBuffer.GiveNextPacketToNic();
    }
}

void
NxRxXlat::EcUpdateAffinity()
{
    if (m_groupAffinityChanged)
    {
        while (InterlockedExchange(&m_groupAffinityChanged, 0))
        {
#ifdef _KERNEL_MODE
            KeSetSystemGroupAffinityThread(&m_groupAffinity, NULL);
#endif
        }
    }
}

void
NxRxXlat::EcYieldToNetAdapter()
{
    m_queueDispatch->Advance(m_queue);
}

static size_t g_NetBufferOffset = sizeof(NET_BUFFER_LIST);

void
PrefetchNblForReceiveIndication(
    _In_ NET_BUFFER_LIST *nbl)
{
    // NDIS tends to allocate a NET_BUFFER just after a NET_BUFFER_LIST.
    // This is somewhat inexact, but we only need to be within a cacheline of accuracy.
    //
    // Don't attempt to dereference this pointer, though.
    auto netBuffer = reinterpret_cast<NET_BUFFER*>((UCHAR*)nbl + g_NetBufferOffset);

    // We'll be writing Nbl->Next
    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, nbl);

    // We'll zero out the flags
    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, &nbl->NblFlags);

    // And probably write an EtherType here
    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, &nbl->NetBufferListInfo[NetBufferListFrameType]);

    // Then we'll write the DataLength field on the NET_BUFFER
    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, &netBuffer->DataLength);
}

void
PrefetchPacketPayloadForReceiveIndication(
    _In_ NET_PACKET_FRAGMENT const *firstFragment)
{
    auto payload = (UCHAR*)firstFragment->VirtualAddress + firstFragment->Offset;

    // This offset is best-suited for Ethernet, where perf is most important.
    auto firstReadFieldOffset = FIELD_OFFSET(ETHERNET_HEADER, Type);

    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, payload + firstReadFieldOffset);
}

void
NxRxXlat::EcIndicateNblsToNdis()
{
    NxNblSequence nblsToIndicate;

    auto pRing = m_ringBuffer.Get();
    auto packetIndex = m_ringBuffer.GetNextOSIndex();
    auto isLastPacket = packetIndex == pRing->BeginIndex;

    while (true)
    {
        if (isLastPacket)
        {
            m_ringBuffer.GetNextOSIndex() = packetIndex;
            break;
        }

        auto nextPacketIndex = NetRingBufferIncrementIndex(pRing, packetIndex);
        isLastPacket = (nextPacketIndex == pRing->BeginIndex);

        if (!isLastPacket)
            PrefetchNblForReceiveIndication(m_contextBuffer.GetPacketContext<PacketContext>(nextPacketIndex).NetBufferList);

        auto completed = NetRingBufferGetPacketAtIndex(pRing, packetIndex);

        bool shouldIndicate = false;
        PNET_BUFFER_LIST nbl = nullptr;

        if (!completed->IgnoreThisPacket)
        {
            auto & packetContext = m_contextBuffer.GetPacketContext<PacketContext>(*completed);

            nbl = packetContext.NetBufferList;
            packetContext.NetBufferList = nullptr;

            shouldIndicate = TransferDataBufferFromNetPacketToNbl(completed, nbl);
        }

        if (shouldIndicate)
        {
            nblsToIndicate.AddNbl(nbl);
        }
        else if (nbl)
        {
            ndisAppendNblChainToNblQueueFast(&m_discardedNbl, nbl, nbl);
        }

        ReinitializePacket(completed);

        packetIndex = nextPacketIndex;
    }

    m_postedPackets = nblsToIndicate.GetCount();

    if (!nblsToIndicate)
        return;

    NxRingBufferCounters delta = {};
    delta.NumberOfNetPacketsProduced = m_postedPackets;
    m_ringBuffer.UpdateRingbufferPacketCounters(delta);

    m_outstandingPackets += nblsToIndicate.GetCount();

    if (!m_nblDispatcher->IndicateReceiveNetBufferLists(
            nblsToIndicate.GetNblQueue().First,
            NDIS_DEFAULT_PORT_NUMBER,
            nblsToIndicate.GetCount(),
            nblsToIndicate.GetReceiveFlags()))
    {
        // While stopping the queue, the NBL packet gate may close while this thread
        // is still trying to indicate a receive.
        //
        // If that happens, we're in the process of tearing down this queue, so just
        // mark the NBLs as returned and bail out.
        ndisAppendNblQueueToNblQueueFast(&m_discardedNbl, &nblsToIndicate.GetNblQueue());
    }
}

void
NxRxXlat::WaitForWork()
{
    auto notificationsToArm = GetNotificationsToArm();

    // In order to handle race conditions, the notifications that should
    // be armed at halt cannot change between the halt preparation and the
    // actual halt. If they do change, re-arm the necessary notifications
    // and loop again.
    if (notificationsToArm.Value != 0 && notificationsToArm.Value == m_lastArmedNotifications.Value)
    {
        m_executionContext.WaitForWork();

        // after halting, don't arm any notifications
        notificationsToArm.Value = 0;
    }

    ArmNotifications(notificationsToArm);

    m_lastArmedNotifications = notificationsToArm;
}

static EC_START_ROUTINE NetAdapterReceiveThread;

static
EC_RETURN
NetAdapterReceiveThread(
    PVOID StartContext
    )
{
    reinterpret_cast<NxRxXlat*>(StartContext)->ReceiveThread();
    return EC_RETURN();
}

void
NxRxXlat::SetupRxThreadProperties()
{
#if _KERNEL_MODE
    // setup thread prioirty;
    ULONG threadPriority =
        m_dispatch->NetClientQueryDriverConfigurationUlong(RX_THREAD_PRIORITY);

    KeSetBasePriorityThread(KeGetCurrentThread(), threadPriority - (LOW_REALTIME_PRIORITY + LOW_PRIORITY) / 2);

    BOOLEAN setThreadAffinity =
        m_dispatch->NetClientQueryDriverConfigurationBoolean(RX_THREAD_AFFINITY_ENABLED);

    ULONG threadAffinity =
        m_dispatch->NetClientQueryDriverConfigurationUlong(RX_THREAD_AFFINITY);

    if (setThreadAffinity != FALSE)
    {
        GROUP_AFFINITY Affinity = { 0 };
        GROUP_AFFINITY old;
        PROCESSOR_NUMBER CpuNum = { 0 };
        KeGetProcessorNumberFromIndex(threadAffinity, &CpuNum);
        Affinity.Group = CpuNum.Group;
        Affinity.Mask =
            (threadAffinity != THREAD_AFFINITY_NO_MASK) ?
                AFFINITY_MASK(CpuNum.Number) : ((ULONG_PTR)-1);
        KeSetSystemGroupAffinityThread(&Affinity, &old);
    }
#endif
}

void
NxRxXlat::ReceiveThread()
{
    SetupRxThreadProperties();

    while (! m_executionContext.IsTerminated())
    {
        m_queueDispatch->Start(m_queue);

        auto cancelIssued = false;

        while (true)
        {
            EcReturnBuffers();

            // provide buffers to NetAdapter only if running
            if (! m_executionContext.IsStopping())
            {
                EcPrepareBuffersForNetAdapter();
            }

            EcUpdateAffinity();
            EcYieldToNetAdapter();

            // update ringbuffer counters;
            m_ringBuffer.UpdateRingbufferDepthCounters();

            EcIndicateNblsToNdis();

            if (m_shouldUpdateEcCounters)
            {
                // end of iteration, update execution context counters;
                m_executionContext.UpdateCounters(
                    m_postedPackets == 0 && m_returnedPackets == 0);
            }

            WaitForWork();

            // This represents the wind down of Rx
            if (m_executionContext.IsStopping())
            {
                if (!cancelIssued)
                {
                    // Indicate cancellation to the adapter
                    // and drop all outstanding NBLs.
                    //
                    // One NBL may remain that has been partially programmed into the NIC.
                    // So that NBL is kept around until the end.

                    m_queueDispatch->Cancel(m_queue);

                    cancelIssued = true;
                }

                // The termination condition is that all packets have been returned from the NIC.
                if (! m_ringBuffer.AnyNicPackets())
                {
                    EcRecoverBuffers();
                    m_queueDispatch->Stop(m_queue);
                    m_executionContext.SignalStopped();
                    break;
                }
            }
        }
    }
}

NTSTATUS
NxRxXlat::Initialize(
    void
    )
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(CreateVariousPools(),
                                    "Failed to create pools");

    NET_CLIENT_QUEUE_CONFIG config;
    NET_CLIENT_QUEUE_CONFIG_INIT(
        &config,
        m_rxNumPackets,
        m_rxNumFragments);

    Rtl::KArray<NET_CLIENT_PACKET_EXTENSION> addedPacketExtensions;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PreparePacketExtensions(addedPacketExtensions),
        "Failed to add packet extensions to the RxQueue");

    if (addedPacketExtensions.count() != 0)
    {
        config.PacketExtensions = &addedPacketExtensions[0];
        config.NumberOfPacketExtensions = addedPacketExtensions.count();
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_adapterDispatch->CreateRxQueue(
            m_adapter,
            this,
            &QueueDispatch,
            &config,
            &m_queue,
            &m_queueDispatch),
        "Failed to create Rx queue. NxRxXlat=%p", this);

    // now we use the app to store offsets, however, this should be a in per-queue context
    // once NET_CLIENT has it's own queue context.

    // checksum offset
    m_checksumOffset = GetPacketExtensionOffsets(
        NET_PACKET_EXTENSION_CHECKSUM_NAME, NET_PACKET_EXTENSION_CHECKSUM_VERSION_1);

    m_descriptor = m_queueDispatch->GetNetDatapathDescriptor(m_queue);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_ringBuffer.Initialize(NET_DATAPATH_DESCRIPTOR_GET_PACKET_RING_BUFFER(m_descriptor)),
        "Failed to initialize packet ring buffer.");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_contextBuffer.Initialize(sizeof(PacketContext)),
        "Failed to initialize private context.");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_executionContext.Initialize(this, NetAdapterReceiveThread),
        "Failed to start Rx execution context. NxRxXlat=%p", this);

    m_executionContext.SetDebugNameHint(L"Receive", GetQueueId(), m_adapterProperties.NetLuid);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxRxXlat::Start(
    void
    )
{
    m_executionContext.Start();
}

_Use_decl_annotations_
void
NxRxXlat::Cancel(
    void
    )
{
    m_executionContext.Cancel();
}

_Use_decl_annotations_
void
NxRxXlat::Stop(
    void
    )
{
    m_executionContext.Stop();
}

//
// DUMMY_VA is the argument we pass as the base address to MmSizeOfMdl when we
// haven't allocated space yet.  (PAGE_SIZE - 1) gives a worst case value for
// number of pages required.
//
#define DUMMY_VA UlongToPtr(PAGE_SIZE - 1)

NTSTATUS
NxRxXlat::CreateVariousPools()
{
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES datapathCapabilities;
    m_adapterDispatch->GetDatapathCapabilities(m_adapter, &datapathCapabilities);

    NX_PERF_RX_NIC_CHARACTERISTICS perfCharacteristics = {};
    NX_PERF_RX_TUNING_PARAMETERS perfParameters;
    perfCharacteristics.Nic.IsDriverVerifierEnabled = !!m_adapterProperties.DriverIsVerifying;
    perfCharacteristics.Nic.MediaType = m_mediaType;
    perfCharacteristics.FragmentRingNumberOfElementsHint = datapathCapabilities.PreferredRxFragmentRingSize;
    perfCharacteristics.MaximumFragmentBufferSize = datapathCapabilities.MaximumRxFragmentSize;
    perfCharacteristics.NominalLinkSpeed = datapathCapabilities.NominalMaxRxLinkSpeed;
    perfCharacteristics.MaxPacketSizeWithRsc = datapathCapabilities.MaximumRxFragmentSize + m_backfillSize;

    NxPerfTunerCalculateRxParameters(&perfCharacteristics, &perfParameters);
    m_rxNumPackets = perfParameters.PacketRingElementCount;
    m_rxNumFragments = perfParameters.FragmentRingElementCount;
    m_rxNumDataBuffers = perfParameters.NumberOfBuffers;
    m_rxNumNbls = perfParameters.NumberOfNbls;

    m_backfillSize = 0;
    m_rxDataBufferSize = datapathCapabilities.MaximumRxFragmentSize + m_backfillSize;
    m_rxBufferAllocationMode = datapathCapabilities.RxMemoryManagementMode;

    NET_BUFFER_LIST_POOL_PARAMETERS poolParameters = {};

    poolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    poolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
    poolParameters.Header.Size = sizeof(NET_BUFFER_LIST_POOL_PARAMETERS);
    poolParameters.ProtocolId = 0;
    poolParameters.fAllocateNetBuffer = TRUE;
    poolParameters.PoolTag = 'xRxN';
    poolParameters.ContextSize = 0;
    poolParameters.DataSize = 0;

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_NblLookupTable.reserve(m_rxNumNbls));

    m_netBufferListPool.reset(NdisAllocateNetBufferListPool(m_adapterProperties.NdisAdapterHandle,
                                                            &poolParameters));
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_netBufferListPool);

    size_t totalSize = 0;
    size_t mdlSize = ALIGN_UP(MmSizeOfMdl(DUMMY_VA, m_rxDataBufferSize), PVOID);
    CX_RETURN_IF_NOT_NT_SUCCESS(RtlSizeTMult(mdlSize, m_rxNumDataBuffers, &totalSize));

    m_MdlPool = MakeSizedPoolPtrNP<MDL>('prxc', totalSize);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_MdlPool);
    RtlZeroMemory(m_MdlPool.get(), totalSize);

    if (m_rxBufferAllocationMode != NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER)
    {
        // create buffer pool if the driver wants the OS to allocate Rx buffer
        NET_CLIENT_BUFFER_POOL_CONFIG bufferPoolConfig = {
            &datapathCapabilities.RxMemoryConstraints,
            m_rxNumDataBuffers,
            m_rxDataBufferSize,
            m_backfillSize,
            0,
            MM_ANY_NODE_OK,                      //default numa node
            NET_CLIENT_BUFFER_POOL_FLAGS_NONE   //non-serialized version
        };

        CX_RETURN_IF_NOT_NT_SUCCESS(
            m_dispatch->NetClientCreateBufferPool(&bufferPoolConfig,
                                                  &m_bufferPool,
                                                  &m_bufferPoolDispatch));
    }

    for (size_t i = 0; i < m_rxNumNbls; i++)
    {
        PNET_BUFFER_LIST nbl =
            NdisAllocateNetBufferAndNetBufferList(m_netBufferListPool.get(),
                                                  0,
                                                  0,
                                                  nullptr,
                                                  0,
                                                  0);

        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !nbl);

        PNET_BUFFER nb = NET_BUFFER_LIST_FIRST_NB(nbl);
        PMDL mdl = reinterpret_cast<PMDL>(((size_t) m_MdlPool.get()) + i * mdlSize);
        NET_BUFFER_FIRST_MDL(nb) = NET_BUFFER_CURRENT_MDL(nb) = mdl;

        auto internalAllocationOffset = (UCHAR*)nb - (UCHAR*)nbl;
        if (internalAllocationOffset < 4 * sizeof(NET_BUFFER_LIST))
            g_NetBufferOffset = internalAllocationOffset;

        if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH)
        {
            //
            // pre-built MDL if the driver wants the OS to automatic attach the Rx buffer
            // to the NET_PACKETs
            //






            NET_PACKET_FRAGMENT data;

            CX_RETURN_NTSTATUS_IF(
                STATUS_INSUFFICIENT_RESOURCES,
                1 != m_bufferPoolDispatch->NetClientAllocateBuffers(m_bufferPool,
                                                                    &data,
                                                                    1));

            GetRxContextFromNb(nb)->DmaLogicalAddress = data.Mapping.DmaLogicalAddress.QuadPart;

            MmInitializeMdl(mdl, data.VirtualAddress, data.Capacity);
            MmBuildMdlForNonPagedPool(mdl);
        }

        m_NblLookupTable.append(nbl);
    }

    return STATUS_SUCCESS;
}

void
NxRxXlat::Notify()
{
    m_executionContext.SignalWork();
}

size_t
NxRxXlat::GetPacketExtensionOffsets(
    PCWSTR ExtensionName,
    ULONG ExtensionVersion
    ) const
{
    NET_CLIENT_PACKET_EXTENSION extension = {};
    extension.Name = ExtensionName;
    extension.Version = ExtensionVersion;
    return m_queueDispatch->GetPacketExtensionOffset(m_queue, &extension);
}

NTSTATUS
NxRxXlat::PreparePacketExtensions(
    _Inout_ Rtl::KArray<NET_CLIENT_PACKET_EXTENSION>& addedPacketExtensions
    )
{
    // checksum
    NET_CLIENT_PACKET_EXTENSION extension = {};
    extension.Name = NET_PACKET_EXTENSION_CHECKSUM_NAME;
    extension.Version = NET_PACKET_EXTENSION_CHECKSUM_VERSION_1;

    //
    // Need to figure out how this would inter-op with capabilities interception
    // ideally here we should check both (Registered) && (Enabled) before we add
    // this extension to queue config.
    //
    if (NT_SUCCESS(m_adapterDispatch->QueryRegisteredPacketExtension(m_adapter, &extension)))
    {
        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            !addedPacketExtensions.append(extension));
    }

    // more to come later!
    return STATUS_SUCCESS;
}

void
NxRxXlat::EcRecoverBuffers()
{
    // Return any packets come back from upper layer after the EC stops
    EcReturnBuffers();

    while (auto completed = m_ringBuffer.TakeNextPacketFromNic())
    {
        if (!completed->IgnoreThisPacket &&
             m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER)
        {
            for (UINT32 i = 0; NOTHING; i++)
            {
                auto fragment = NET_PACKET_GET_FRAGMENT(completed, m_descriptor, i);
                m_adapterDispatch->ReturnRxBuffer(m_adapter, fragment->VirtualAddress, fragment->RxBufferReturnContext);
                fragment->RxBufferReturnContext = nullptr;
                if (fragment->LastFragmentOfFrame)
                    break;
            }
        }

        auto & packetContext = m_contextBuffer.GetPacketContext<PacketContext>(*completed);
        ReturnNblToPool(packetContext.NetBufferList);
        packetContext.NetBufferList = nullptr;
        ReinitializePacket(completed);
    }

    NT_ASSERT(!m_ringBuffer.AnyNicPackets());
    NT_ASSERT(m_NumOfNblsInUse == 0);
}

NxRxXlat::~NxRxXlat()
{
#ifdef _KERNEL_MODE
    if (m_shouldReportCounters)
    {
        KeCancelTimer(&m_CounterReportTimer);
        KeFlushQueuedDpcs();
    }
#endif

    // stop the EC and wait for wind down.
    m_executionContext.Terminate();

    for (size_t i = 0; i < m_NblLookupTable.count(); i++)
    {
        PNET_BUFFER_LIST nbl = m_NblLookupTable[i];

        if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH)
        {
            PVOID va = MmGetMdlVirtualAddress(NET_BUFFER_CURRENT_MDL(NET_BUFFER_LIST_FIRST_NB(nbl)));
            m_bufferPoolDispatch->NetClientFreeBuffers(m_bufferPool,
                                                       &va,
                                                       1);
        }

        NdisFreeNetBufferList(nbl);
    }

    if (m_bufferPool)
    {
        m_bufferPoolDispatch->NetClientDestroyBufferPool(m_bufferPool);
        m_bufferPool = nullptr;
    }

    if (m_queue)
    {
        m_adapterDispatch->DestroyQueue(m_adapter, m_queue);
    }
}

void
NxRxXlat::QueueReturnedNetBufferLists(
    _In_ NBL_QUEUE* NblChain
    )
{
    NBL_COUNTED_QUEUE countedQueue;
    countedQueue.Queue.First = NblChain->First;
    countedQueue.Queue.Last = NblChain->Last;
    countedQueue.NblCount = 0; // don't care about count here.
    m_returnedNblQueue.Enqueue(&countedQueue);

    if (m_returnedNblNotification.TestAndClear())
    {
        m_executionContext.SignalWork();
    }
}

NTSTATUS
NxRxXlat::AttachEmptyDataBufferToNetPacket(
    _Inout_ PNET_PACKET Packet,
    _Out_ PNET_BUFFER_LIST* Nbl)
{
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          m_NumOfNblsInUse == m_rxNumDataBuffers);

    if (!NET_PACKET_GET_FRAGMENT_VALID(Packet))
    {
        auto& fragmentRing = *NET_DATAPATH_DESCRIPTOR_GET_FRAGMENT_RING_BUFFER(m_descriptor);
        auto availableFragments = NetRbFragmentRange::OsRange(fragmentRing);

        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, availableFragments.Count() == 0);

        auto it = availableFragments.begin();

        auto& fragmentToAttach = *it;
        RtlZeroMemory(&fragmentToAttach, NetPacketFragmentGetSize());
        fragmentToAttach.LastFragmentOfFrame = TRUE;

        // Update the packet with EndIndex and increment the availableFragments iterator
        Packet->FragmentOffset = (it++).GetIndex();
        Packet->FragmentValid = TRUE;

        fragmentRing.EndIndex = it.GetIndex();
    }

    *Nbl = DrawNblFromPool();

    auto fragment = NET_PACKET_GET_FRAGMENT(Packet, m_descriptor, 0);
    NT_ASSERT(fragment->LastFragmentOfFrame == TRUE);

    if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH)
    {
        PNET_BUFFER nb = NET_BUFFER_LIST_FIRST_NB(*Nbl);
        PMDL mdl = NET_BUFFER_CURRENT_MDL(nb);
        fragment->VirtualAddress = MmGetMdlVirtualAddress(mdl);
        fragment->Mapping.DmaLogicalAddress.QuadPart = GetRxContextFromNb(nb)->DmaLogicalAddress;
        fragment->Capacity = MmGetMdlByteCount(mdl);
        fragment->Offset = m_backfillSize;
    }
    else
    {
        fragment->VirtualAddress = nullptr;
        fragment->Mapping.DmaLogicalAddress.QuadPart = 0;
        fragment->Capacity = 0;
        fragment->Offset = 0;
    }

    return STATUS_SUCCESS;
}

static
USHORT
CalculateNblFrameTypeForEthernetPacket(
    _In_ NET_DATAPATH_DESCRIPTOR const * descriptor,
    _In_ NET_PACKET const *packet
    )
{
    NT_ASSERT(packet->Layout.Layer2HeaderLength >= sizeof(ETHERNET_HEADER));

    USHORT ethertype;
    if (!NxGetPacketEtherType(descriptor, packet, &ethertype))
        return 0;

    return RtlUshortByteSwap(ethertype);
}

static
USHORT
CalculateNblFrameTypeForPacket(
    _In_ NET_DATAPATH_DESCRIPTOR const * descriptor,
    _In_ NET_PACKET const &packet
    )
{
    switch (packet.Layout.Layer3Type)
    {
    case NET_PACKET_LAYER3_TYPE_IPV4_UNSPECIFIED_OPTIONS:
    case NET_PACKET_LAYER3_TYPE_IPV4_WITH_OPTIONS:
    case NET_PACKET_LAYER3_TYPE_IPV4_NO_OPTIONS:
        return ByteSwap(ETHERNET_TYPE_IPV4);

    case NET_PACKET_LAYER3_TYPE_IPV6_UNSPECIFIED_EXTENSIONS:
    case NET_PACKET_LAYER3_TYPE_IPV6_WITH_EXTENSIONS:
    case NET_PACKET_LAYER3_TYPE_IPV6_NO_EXTENSIONS:
        return ByteSwap(ETHERNET_TYPE_IPV6);

    case NET_PACKET_LAYER3_TYPE_UNSPECIFIED:
    default:
        // The NIC didn't tell us the frame type. Keep going and we'll try
        // to compute it ourselves.
        break;
    }

    // Next try to compute it by parsing the layer2 header.
    switch (packet.Layout.Layer2Type)
    {
    case NET_PACKET_LAYER2_TYPE_ETHERNET:
        return CalculateNblFrameTypeForEthernetPacket(descriptor, &packet);

    case NET_PACKET_LAYER2_TYPE_UNSPECIFIED:
    case NET_PACKET_LAYER2_TYPE_NULL:
        // We don't know how to compute the Frame Type for non-Ethernet.
        __fallthrough;
    default:
        return 0;
    }
}

bool
NxRxXlat::TransferDataBufferFromNetPacketToNbl(
    _In_ PNET_PACKET Packet,
    _In_ PNET_BUFFER_LIST Nbl)
{
    PNET_BUFFER nb = NET_BUFFER_LIST_FIRST_NB(Nbl);
    bool shouldIndicate = false;
    // ensure the NBL chain is broken
    Nbl->Next = nullptr;

    const auto firstFragment = NET_PACKET_GET_FRAGMENT(Packet, m_descriptor, 0);
    PrefetchPacketPayloadForReceiveIndication(firstFragment);







    // Always compute packet layout in software on RX path now.
    Packet->Layout = NxGetPacketLayout(m_adapterProperties.MediaType, m_descriptor, Packet);

    Nbl->NetBufferListInfo[TcpIpChecksumNetBufferListInfo] = 0;

    if (IsPacketChecksumEnabled())
    {
        Nbl->NetBufferListInfo[TcpIpChecksumNetBufferListInfo] = NxTranslateRxPacketChecksum(Packet, m_checksumOffset).Value;
    }

    Nbl->NblFlags = 0;

    auto frameType = CalculateNblFrameTypeForPacket(m_descriptor, *Packet);
    Nbl->NetBufferListInfo[NetBufferListFrameType] = (PVOID)frameType;
    switch (frameType)
    {
    case ByteSwap(ETHERNET_TYPE_IPV4):
        NdisSetNblFlag(Nbl, NDIS_NBL_FLAGS_IS_IPV4);
        break;
    case ByteSwap(ETHERNET_TYPE_IPV6):
        NdisSetNblFlag(Nbl, NDIS_NBL_FLAGS_IS_IPV6);
        break;
    default:
        break;
    }

    // store which queue this NB comes from
    GetRxContextFromNbl(Nbl)->Queue = this;

    //
    //2. packet's first fragment
    //

    PMDL currMdl = NET_BUFFER_CURRENT_MDL(nb);
    NET_BUFFER_DATA_LENGTH(nb) = firstFragment->ValidLength;
    NET_BUFFER_DATA_OFFSET(nb) = firstFragment->Offset;

    NET_BUFFER_CURRENT_MDL_OFFSET(nb) = firstFragment->Offset;

    shouldIndicate = ReInitializeMdlForDataBuffer(nb,
                                                  firstFragment,
                                                  currMdl,
                                                  true);

    //
    //3. packet's additonal fragments
    //
    auto fragmentCount = NetPacketGetFragmentCount(m_descriptor, Packet);
    for (UINT32 i = 1; i < fragmentCount; ++i)
    {
        auto currFragment = NET_PACKET_GET_FRAGMENT(Packet, m_descriptor, i);
        if (currFragment->LastFragmentOfFrame)
        {
            break;
        }

        NET_BUFFER_DATA_LENGTH(nb) += (ULONG)currFragment->ValidLength;





        currMdl = NDIS_MDL_LINKAGE(currMdl);

        shouldIndicate &= ReInitializeMdlForDataBuffer(nb,
            currFragment,
            currMdl,
            false);

    }

    return shouldIndicate;
}

bool
NxRxXlat::ReInitializeMdlForDataBuffer(
    _In_ PNET_BUFFER nb,
    _In_ NET_PACKET_FRAGMENT const* fragment,
    _In_ PMDL fragmentMdl,
    _In_ bool isFirstFragment)
{
    bool mdlBuilt = false;

    if (isFirstFragment || (fragment->Offset == 0)) //only the first fragment can have an offset
    {
        if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH) //data buffer allocated by the translator
        {
            //all MDLs are pre-built
            mdlBuilt = true;
        }
        else //data buffer allocated by the NIC
        {
            //the data buffer size must confront to the rx capability declared by the NIC
            if (fragment->Capacity <= m_rxDataBufferSize)
            {
                MmInitializeMdl(fragmentMdl, fragment->VirtualAddress, fragment->Capacity);
                MmBuildMdlForNonPagedPool(fragmentMdl);

                NT_ASSERT(fragmentMdl->Next == nullptr);

                if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER)
                {
                    if (isFirstFragment)
                    {
                        //first fragment's rx return context will be used for all fragments
                        GetRxContextFromNb(nb)->RxBufferReturnContext = fragment->RxBufferReturnContext;
                        mdlBuilt = true;
                    }
                    else
                    {
                        //if the NB has more than 1 fragment, all framgents must have the same
                        //return context
                        if (GetRxContextFromNb(nb)->RxBufferReturnContext == fragment->RxBufferReturnContext)
                        {
                            mdlBuilt = true;
                        }
                        else
                        {
                            //if the return context doesn't match, mark this packet to be dropped
                            mdlBuilt = false;
                        }
                    }
                }
            }
            else
            {
                //if the packet received is larger than the driver claimed to be able to support,
                //mark this packet to be dropped
                mdlBuilt = false;

                //we still need to return the rx data buffer of the dropped packet back to the NIC
                GetRxContextFromNb(nb)->RxBufferReturnContext = fragment->RxBufferReturnContext;
            }
        }
    }
    else
    {
        mdlBuilt = false;
    }

    return mdlBuilt;
}


PNET_BUFFER_LIST
NxRxXlat::FreeReceivedDataBuffer(PNET_BUFFER_LIST nbl)
{
    switch (m_rxBufferAllocationMode)
    {
        case NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH:
            NOTHING
            break;

        case NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE:
        {
            PMDL currMdl = NET_BUFFER_CURRENT_MDL(NET_BUFFER_LIST_FIRST_NB(nbl));

            while (currMdl)
            {
                PVOID va = MmGetMdlVirtualAddress(currMdl);

                m_bufferPoolDispatch->NetClientFreeBuffers(m_bufferPool,
                                                           &va,
                                                           1);

                currMdl = NDIS_MDL_LINKAGE(currMdl);
            }

            break;
        }

        case NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER:
        {
            PNET_BUFFER nb = NET_BUFFER_LIST_FIRST_NB(nbl);

            PVOID rxReturnContext = GetRxContextFromNb(nb)->RxBufferReturnContext;
            PMDL currMdl = NET_BUFFER_CURRENT_MDL(NET_BUFFER_LIST_FIRST_NB(nbl));

            NT_ASSERT(currMdl->Next == nullptr);

            while (currMdl)
            {
                PVOID va = MmGetMdlVirtualAddress(currMdl);
                m_adapterDispatch->ReturnRxBuffer(m_adapter, va, rxReturnContext);

                currMdl = NDIS_MDL_LINKAGE(currMdl);
            }

            GetRxContextFromNb(nb)->RxBufferReturnContext = nullptr;

            break;
        }
    }

    PNET_BUFFER_LIST next = nbl->Next;
    ReturnNblToPool(nbl);

    return next;
}

PNET_BUFFER_LIST
NxRxXlat::DrawNblFromPool()
{
    return m_NblLookupTable[m_NumOfNblsInUse++];
}

void
NxRxXlat::ReturnNblToPool(_In_ PNET_BUFFER_LIST nbl)
{
    nbl->Next = nullptr;
    m_NblLookupTable[--m_NumOfNblsInUse] = nbl;
}

bool
NxRxXlat::IsPacketChecksumEnabled() const
{
    return m_checksumOffset != NET_PACKET_EXTENSION_INVALID_OFFSET;
}

#ifdef _KERNEL_MODE
_Use_decl_annotations_
VOID
NxRxXlat::CounterReportDpcRoutine(
    _In_     struct _KDPC *Dpc,
    _In_opt_ PVOID        DeferredContext,
    _In_opt_ PVOID        SystemArgument1,
    _In_opt_ PVOID        SystemArgument2
    )
{
    UNREFERENCED_PARAMETER((Dpc, SystemArgument1, SystemArgument2));
    NxRxXlat* rx = static_cast<NxRxXlat*>(DeferredContext);
    rx->ReportCounters();
}
#endif

void
NxRxXlat::ReportCounters()
{
    NxRingBufferCounters localRBCounters;
    NxExecutionContextCounters localECCounters;

    localECCounters = m_executionContext.GetExecutionContextCounters();
    localRBCounters = m_ringBuffer.GetRingbufferCounters();
    m_ringBuffer.ResetRingbufferCounters();

    ULONG64 totalRingbufferStateSamples =
        localRBCounters.RingbufferEmptyCount +
        localRBCounters.RingbufferFullyOccupiedCount +
        localRBCounters.RingbufferPartiallyOccupiedCount;

    ULONG64 usefulIterationCount =
        localECCounters.IterationCount - localECCounters.BusyWaitIterationCount;

    TraceLoggingWrite(
        g_hNetAdapterCxXlatProvider,
        "RxTranslationCounterUpdates",
        TraceLoggingDescription("RX ETW performance counter event"),
        TraceLoggingUInt32(m_executionContext.GetExecutionContextIdentifier(), "threadID"),
        TraceLoggingUInt64(localRBCounters.IterationCountInLastInterval, "ringbufferUpdateIterationCount"),
        TraceLoggingUInt64(localRBCounters.CumulativeRingBufferDepthInLastInterval, "cumulativeRingBufferDepth"),
        TraceLoggingUInt64(totalRingbufferStateSamples, "totalNumberOfRingbufferStateSamples"),
        TraceLoggingUInt64(localRBCounters.RingbufferEmptyCount, "numberOfRingbufferEmptySamples"),
        TraceLoggingUInt64(localRBCounters.RingbufferFullyOccupiedCount, "numberOfRingbufferFullSamples"),
        TraceLoggingUInt64(localRBCounters.RingbufferPartiallyOccupiedCount, "numberOfRingbufferPartiallyUsedSamples"),
        TraceLoggingUInt64(localRBCounters.NumberOfNetPacketsProduced, "totalNumberOfNetpacketsProducedForReceive"),
        TraceLoggingUInt64(localRBCounters.NumberOfNetPacketsConsumed, "totalNumberOfNetpacketsCompletedReceiving"),
        TraceLoggingUInt64(localECCounters.IterationCount, "ecUpdateIterationCount"),
        TraceLoggingUInt64(localECCounters.BusyWaitIterationCount, "numberOfIterationsPolledWithNoPackets"),
        TraceLoggingUInt64(usefulIterationCount, "numberOfIterationsPolledWithPackets"),
        TraceLoggingUInt64(localECCounters.TotalCpuCycleTime, "totalNumberOfCpuCyclesRecord"),
        TraceLoggingUInt64(localECCounters.BusyWaitCycles, "numberOfCpuCyclesPolledWithNoPackets"),
        TraceLoggingUInt64(localECCounters.ProcessingCycles, "numberOfCpuCyclesSpentProcessingPackets"),
        TraceLoggingUInt64(localECCounters.IdleCycles, "numberOfCpuCyclesSleeping")
    );
}

