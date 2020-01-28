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

#include <net/ring.h>
#include <net/packet.h>
#include <netiodef.h>

#include <net/checksumtypes_p.h>
#include <net/logicaladdress_p.h>
#include <net/virtualaddress_p.h>
#include <net/mdl_p.h>
#include <net/returncontext_p.h>
#include <net/rsctypes_p.h>
#include <net/databuffer_p.h>

#include "NxPerfTuner.hpp"
#include "NxNblSequence.h"
#include "NxPacketLayout.hpp"

struct EX_MDL_CONTEXT
{
    union
    {
        size_t BufferIndex; //used when NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE
        NET_FRAGMENT_RETURN_CONTEXT_HANDLE RxBufferReturnContext; //used when driver manages the buffers
    } DUMMYUNIONNAME;
};

struct MDL_EX
{
    EX_MDL_CONTEXT MdlContext;
    UINT64  LogicalAddress;
    MDL     Mdl;
};

//
// DUMMY_VA is the argument we pass as the base address to MmSizeOfMdl when we
// haven't allocated space yet.  (PAGE_SIZE - 1) gives a worst case value for
// number of pages required.
//
#define DUMMY_VA UlongToPtr(PAGE_SIZE - 1)

static size_t g_NetBufferOffset = sizeof(NET_BUFFER_LIST);

PAGED
NTSTATUS
MdlPool::Initialize(_In_ size_t MemorySize,
                    _In_ size_t MdlCount)
{
    size_t slabSize;

    m_MdlSize = ALIGN_UP(RTL_SIZEOF_THROUGH_FIELD(MDL_EX, LogicalAddress) +
                         MmSizeOfMdl(DUMMY_VA, MemorySize),
                         PVOID);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                          !resize(MdlCount));

    CX_RETURN_IF_NOT_NT_SUCCESS(RtlSizeTMult(m_MdlSize, MdlCount, &slabSize));

    m_MdlStorage = MakeSizedPoolPtrNP<BYTE>('sksk', slabSize);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_MdlStorage);

    for (size_t i = 0; i < MdlCount; i++)
    {
        PushToReturn(reinterpret_cast<MDL_EX*>(m_MdlStorage.get() + i * m_MdlSize));
    }

    return STATUS_SUCCESS;
}

PAGED
NTSTATUS
NblPool::Initialize(_In_ NDIS_HANDLE NdisHandle,
                    _In_ size_t NdlCount,
                    _In_ NDIS_MEDIUM Medium)
{
    NET_BUFFER_LIST_POOL_PARAMETERS poolParameters = {};

    poolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    poolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
    poolParameters.Header.Size = sizeof(NET_BUFFER_LIST_POOL_PARAMETERS);
    poolParameters.ProtocolId = 0;
    poolParameters.fAllocateNetBuffer = TRUE;
    poolParameters.PoolTag = 'xRxN';
    poolParameters.ContextSize = 0;
    poolParameters.DataSize = 0;

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! resize(NdlCount));

    m_NblStorage.reset(NdisAllocateNetBufferListPool(NdisHandle,
                       &poolParameters));
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_NblStorage);

    for (size_t i = 0; i < NdlCount; i++)
    {
        PNET_BUFFER_LIST nbl =
            NdisAllocateNetBufferAndNetBufferList(m_NblStorage.get(),
                                                  0,
                                                  0,
                                                  nullptr,
                                                  0,
                                                  0);

        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !nbl);

        PNET_BUFFER nb = NET_BUFFER_LIST_FIRST_NB(nbl);
        NET_BUFFER_FIRST_MDL(nb) = NET_BUFFER_CURRENT_MDL(nb) = nullptr;

        auto internalAllocationOffset = (UCHAR*)nb - (UCHAR*)nbl;
        if (internalAllocationOffset < 4 * sizeof(NET_BUFFER_LIST))
            g_NetBufferOffset = internalAllocationOffset;

        PushToReturn(nbl);

        if (Medium == NdisMediumNative802_11)
        {
            auto dot11Context = MakePoolPtr<DOT11_EXTSTA_RECV_CONTEXT>('eWtX');

            CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !dot11Context);

            dot11Context->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
            dot11Context->Header.Revision = DOT11_EXTSTA_RECV_CONTEXT_REVISION_1;
            dot11Context->Header.Size = sizeof(*dot11Context);
            dot11Context->uPhyId = dot11_phy_type_erp;
            dot11Context->uChCenterFrequency = 2437;
            dot11Context->usNumberOfMPDUsReceived = 1;

            // Transfer ownership to the NBL. The memory will be freed in ~NxRxXlat()
            nbl->NetBufferListInfo[MediaSpecificInformation] = dot11Context.release();
        }
    }

    return STATUS_SUCCESS;
}

static
NET_CLIENT_EXTENSION RxQueueExtensions[] = {
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_PACKET_EXTENSION_CHECKSUM_NAME,
        NET_PACKET_EXTENSION_CHECKSUM_VERSION_1,
        0,
        NET_PACKET_EXTENSION_CHECKSUM_VERSION_1_SIZE,
        NetExtensionTypePacket,
    },
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_PACKET_EXTENSION_RSC_NAME,
        NET_PACKET_EXTENSION_RSC_VERSION_1,
        0,
        NET_PACKET_EXTENSION_RSC_VERSION_1_SIZE,
        NetExtensionTypePacket,
    },
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_PACKET_EXTENSION_RSC_TIMESTAMP_NAME,
        NET_PACKET_EXTENSION_RSC_TIMESTAMP_VERSION_1,
        0,
        NET_PACKET_EXTENSION_RSC_TIMESTAMP_VERSION_1_SIZE,
        NetExtensionTypePacket,
    },
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_FRAGMENT_EXTENSION_DATA_BUFFER_NAME,
        NET_FRAGMENT_EXTENSION_DATA_BUFFER_VERSION_1,
        0,
        NET_FRAGMENT_EXTENSION_DATA_BUFFER_VERSION_1_SIZE,
        NetExtensionTypeFragment,
    },
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
        NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
        0,
        NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1_SIZE,
        NetExtensionTypeFragment,
    },
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
        0,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1_SIZE,
        NetExtensionTypeFragment,
    },
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_NAME,
        NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_VERSION_1,
        0,
        NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_VERSION_1_SIZE,
        NetExtensionTypeFragment,
    },
};

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
    PNET_BUFFER_LIST next = NET_BUFFER_LIST_NEXT_NBL(current);

    *queue = GetRxContextFromNbl(first)->Queue;

    while (true)
    {
        if (next)
        {
            if (*queue == GetRxContextFromNbl(next)->Queue)
            {
                current = next;
                next = NET_BUFFER_LIST_NEXT_NBL(current);
                (*numNbl)++;

                continue;
            }
            else
            {
                NET_BUFFER_LIST_NEXT_NBL(current) = nullptr;
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
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
    NxStatistics & Statistics
) noexcept :
    m_queueId(QueueId),
    m_dispatch(Dispatch),
    m_adapter(Adapter),
    m_adapterDispatch(AdapterDispatch),
    m_metadataTranslator(m_rings, m_extensions),
    m_packetContext(m_rings, NetRingTypePacket),
    m_fragmentContext(m_rings, NetRingTypeFragment),
    m_statistics(Statistics)
{
    m_adapterDispatch->GetProperties(m_adapter, &m_adapterProperties);
    m_nblDispatcher = static_cast<INxNblDispatcher *>(m_adapterProperties.NblDispatcher);
    ndisInitializeNblQueue(&m_discardedNbl);
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

    if (m_completedPackets == 0 && m_returnedNbls == 0)
    {
        notifications.Flags.ShouldArmNblReturned = m_outstandingNbls != 0;

        notifications.Flags.ShouldArmRxIndication = true;
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

void
NxRxXlat::EcReturnBuffers()
{
    m_returnedNbls = 0;

    NBL_QUEUE nblsToReturn;
    m_returnedNblQueue.DequeueAll(&nblsToReturn);
    ndisAppendNblQueueToNblQueueFast(&nblsToReturn, &m_discardedNbl);

    auto currNbl = ndisPopAllFromNblQueue(&nblsToReturn);

    while (currNbl)
    {
        --m_outstandingNbls;
        ++m_returnedNbls;
        currNbl = FreeReceivedDataBuffer(currNbl);
    }
}

void
NxRxXlat::EcPrepareBuffersForNetAdapter()
{
    auto pr = NetRingCollectionGetPacketRing(&m_rings);
    auto fr = NetRingCollectionGetFragmentRing(&m_rings);
    auto const prLastIndex = (pr->OSReserved0 - 1) & pr->ElementIndexMask;
    auto const frLastIndex = (fr->OSReserved0 - 1) & fr->ElementIndexMask;

    //1. prepare the packet ring
    for (; ! m_NblPool.IsEmpty() && pr->EndIndex != prLastIndex;
         pr->EndIndex = NetRingIncrementIndex(pr, pr->EndIndex))
    {

        auto & context = m_packetContext.GetContext<PacketContext>(pr->EndIndex);
        auto packet = NetRingGetPacketAtIndex(pr, pr->EndIndex);

        NT_FRE_ASSERT(context.NetBufferList == nullptr);

        context.NetBufferList = m_NblPool.PopToUse();
        NT_FRE_ASSERT(NET_BUFFER_LIST_NEXT_NBL(context.NetBufferList) == nullptr);

        // XXX we need to review whether we zero the entire packet + extension
        RtlZeroMemory(packet, pr->ElementStride);
    }

    //2. prepare the fragment ring
    for (; fr->EndIndex != frLastIndex;
         fr->EndIndex = NetRingIncrementIndex(fr, fr->EndIndex))
    {
        auto fragment = NetRingGetFragmentAtIndex(fr, fr->EndIndex);

        if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE)
        {
            // if the client driver wants to allocate from NetCx buffer pool and attach itself,
            // nothing NetCx need to do to prepare the fragment ring

            // XXX we need to review whether we zero the entire fragment
            RtlZeroMemory(fragment, fr->ElementStride);
        }
        else
        {
            // if the client driver wants to manage the buffer itself or it wants the OS to attach
            // an empty buffer to the fragment ring on its behalf, we first need to make sure that
            // we still have MDLs available
            if (m_MdlPool.IsEmpty())
            {
                break;
            }

            // Grab next available MDL and store it in the fragment context ring
            MDL_EX* mdlEx = m_MdlPool.PopToUse();
            NT_FRE_ASSERT(mdlEx->Mdl.Next == nullptr);

            auto& context = m_fragmentContext.GetContext<FragmentContext>(fr->EndIndex);
            NT_FRE_ASSERT(context.Mdl == nullptr);
            context.Mdl = &mdlEx->Mdl;

            if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH)
            {
                // if the client driver wants to the OS to attach buffers to the fragment
                // ring, we have already setup this MDL to point to a empty buffer, so now
                // we just need to update the fragment element in the ring to point to this buffer

                fragment->Capacity = MmGetMdlByteCount(context.Mdl);
                fragment->Offset = m_backfillSize;
                fragment->Scratch = 0;

                auto virtualAddress = NetExtensionGetFragmentVirtualAddress(
                    &m_extensions.Extension.VirtualAddress, fr->EndIndex);
                virtualAddress->VirtualAddress = MmGetMdlVirtualAddress(context.Mdl);

                if (m_extensions.Extension.LogicalAddress.Enabled)
                {
                    auto logicalAddress = NetExtensionGetFragmentLogicalAddress(
                        &m_extensions.Extension.LogicalAddress, fr->EndIndex);

                    logicalAddress->LogicalAddress = mdlEx->LogicalAddress;
                }
            }
            else
            {
                // XXX we need to review whether we zero the entire fragment
                RtlZeroMemory(fragment, fr->ElementStride);
            }
        }
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
    _In_ void * Address,
    _In_ size_t Offset
)
{
    auto payload = static_cast<unsigned char *>(Address) + Offset;

    // This offset is best-suited for Ethernet, where perf is most important.
    auto firstReadFieldOffset = FIELD_OFFSET(ETHERNET_HEADER, Type);

    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, payload + firstReadFieldOffset);
}

void
NxRxXlat::EcIndicateNblsToNdis()
{
    auto pr = NetRingCollectionGetPacketRing(&m_rings);

    NxNblSequence nblsToIndicate;

    m_completedPackets = NetRingGetRangeCount(pr, pr->OSReserved0, pr->BeginIndex);

    for (; pr->OSReserved0 != pr->BeginIndex;
         pr->OSReserved0 = NetRingIncrementIndex(pr, pr->OSReserved0))
    {
        auto & context = m_packetContext.GetContext<PacketContext>(pr->OSReserved0);
        auto packet = NetRingGetPacketAtIndex(pr, pr->OSReserved0);

        NT_FRE_ASSERT(context.NetBufferList != nullptr);
        NT_FRE_ASSERT(NET_BUFFER_LIST_NEXT_NBL(context.NetBufferList) == nullptr);

        if (! packet->Ignore &&
            TransferDataBufferFromNetPacketToNbl(packet, context.NetBufferList, pr->OSReserved0))
        {
            nblsToIndicate.AddNbl(context.NetBufferList);
            m_statistics.Increment(NxStatisticsCounters::NumberOfPackets);
        }
        else
        {
            ndisAppendSingleNblToNblQueue(&m_discardedNbl, context.NetBufferList);
        }

        context.NetBufferList = nullptr;
    }

    if (nblsToIndicate)
    {
        m_outstandingNbls += nblsToIndicate.GetCount();

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
}

void
NxRxXlat::EcUpdatePerfCounter()
{
    auto pr = NetRingCollectionGetPacketRing(&m_rings);

    m_statistics.Increment(NxStatisticsCounters::IterationCount);
    m_statistics.IncrementBy(NxStatisticsCounters::NblPending, m_outstandingNbls);
    m_statistics.IncrementBy(NxStatisticsCounters::PacketsCompleted, m_returnedNbls);
    m_statistics.IncrementBy(NxStatisticsCounters::QueueDepth, NetRingGetRangeCount(pr, pr->BeginIndex, pr->EndIndex));
}

void
NxRxXlat::EcWaitForWork()
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
            EcIndicateNblsToNdis();
            EcUpdatePerfCounter();
            EcWaitForWork();

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
                auto const pr = NetRingCollectionGetPacketRing(&m_rings);
                auto const fr = NetRingCollectionGetFragmentRing(&m_rings);
                if (pr->BeginIndex == pr->EndIndex && fr->BeginIndex == fr->EndIndex)
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
    NT_FRE_ASSERT(ARRAYSIZE(RxQueueExtensions) == ARRAYSIZE(m_extensions.Extensions));

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(CreateVariousPools(),
                                    "Failed to create pools");

    NET_CLIENT_QUEUE_CONFIG config;
    NET_CLIENT_QUEUE_CONFIG_INIT(
        &config,
        m_rxNumPackets,
        m_rxNumFragments);

    config.Extensions = &RxQueueExtensions[0];
    config.NumberOfExtensions = ARRAYSIZE(RxQueueExtensions);
    config.NumberOfDataBuffers = m_rxNumDataBuffers;
    config.DataBufferPool = m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE 
                            ? m_bufferPool : nullptr;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_adapterDispatch->CreateRxQueue(
            m_adapter,
            this,
            &QueueDispatch,
            &config,
            &m_queue,
            &m_queueDispatch),
        "Failed to create Rx queue. NxRxXlat=%p", this);

    // query extensions for what was enabled
    for (size_t i = 0; i < ARRAYSIZE(RxQueueExtensions); i++)
    {
        m_queueDispatch->GetExtension(
            m_queue,
            &RxQueueExtensions[i],
            &m_extensions.Extensions[i]);
    }

    RtlCopyMemory(&m_rings, m_queueDispatch->GetNetDatapathDescriptor(m_queue), sizeof(m_rings));

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_packetContext.Initialize(sizeof(PacketContext)),
        "Failed to initialize private packet context.");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_fragmentContext.Initialize(sizeof(FragmentContext)),
        "Failed to initialize private fragment context.");

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

void 
NxRxXlat::EnumerateBufferPoolCallback(
    _In_ PVOID Context, 
    _In_ SIZE_T Index, 
    _In_ UINT64 LogicalAddress, 
    _In_ PVOID VirtualAddress)
{
    NxRxXlat* nxRxXlat = reinterpret_cast<NxRxXlat*>(Context);

    MDL_EX* mdlEx = nxRxXlat->m_MdlPool[Index];

    mdlEx->MdlContext.BufferIndex = Index;
    mdlEx->LogicalAddress = LogicalAddress;
    MmInitializeMdl(&mdlEx->Mdl, VirtualAddress, nxRxXlat->m_rxDataBufferSize);
    MmBuildMdlForNonPagedPool(&mdlEx->Mdl);
}

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

    m_backfillSize = 0; //TODO: set 8 bytes will fail the test
    m_rxDataBufferSize = datapathCapabilities.MaximumRxFragmentSize + m_backfillSize;
    m_rxBufferAllocationMode = datapathCapabilities.RxMemoryManagementMode;

    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties;
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_MdlPool.Initialize(m_rxDataBufferSize, perfParameters.NumberOfBuffers));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_NblPool.Initialize(m_adapterProperties.NdisAdapterHandle,
                             perfParameters.NumberOfNbls,
                             adapterProperties.MediaType));

    if (m_rxBufferAllocationMode != NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER)
    {
        // create buffer pool if the client driver wants NetCx to allocate Rx buffer
        NET_CLIENT_BUFFER_POOL_CONFIG bufferPoolConfig = {
            &datapathCapabilities.RxMemoryConstraints,
            perfParameters.NumberOfBuffers,
            m_rxDataBufferSize,
            m_backfillSize,
            0,
            MM_ANY_NODE_OK,                     //use the numa node supplied by adapter capabilities
            NET_CLIENT_BUFFER_POOL_FLAGS_NONE   //non-serialized version
        };

        CX_RETURN_IF_NOT_NT_SUCCESS(
            m_dispatch->NetClientCreateBufferPool(&bufferPoolConfig,
                                                  &m_bufferPool,
                                                  &m_bufferPoolDispatch));

        // pre-built MDL from MDL pool to describe every buffers in the buffer pool
        // MDL pool has the same number of entries as buffer pool, i.e. perfParameters.NumberOfBuffers
        m_bufferPoolDispatch->NetClientEnumerateBuffers(m_bufferPool, 
                                                        EnumerateBufferPoolCallback,
                                                        this);
    }

    return STATUS_SUCCESS;
}

void
NxRxXlat::Notify()
{
    m_executionContext.SignalWork();
}

void
NxRxXlat::EcRecoverBuffers()
{
    // Return any packets come back from upper layer after the EC stops
    EcReturnBuffers();

    auto pr = NetRingCollectionGetPacketRing(&m_rings);
    auto fr = NetRingCollectionGetFragmentRing(&m_rings);

    for (; pr->OSReserved0 != pr->BeginIndex;
         pr->OSReserved0 = NetRingIncrementIndex(pr, pr->OSReserved0))
    {

        auto & context = m_packetContext.GetContext<PacketContext>(pr->OSReserved0);
        auto packet = NetRingGetPacketAtIndex(pr, pr->OSReserved0);

        NT_FRE_ASSERT(context.NetBufferList != nullptr);

        m_NblPool.PushToReturn(context.NetBufferList);
        context.NetBufferList = nullptr;

        if (! packet->Ignore &&
            m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER)
        {
            for (UINT32 i = 0; i < packet->FragmentCount; i++)
            {
                auto fragmentIndex = NetRingAdvanceIndex(fr, packet->FragmentIndex, i);

                auto returnContext = NetExtensionGetFragmentReturnContext(
                    &m_extensions.Extension.ReturnContext, fragmentIndex);

                m_adapterDispatch->ReturnRxBuffer(
                    m_adapter,
                    returnContext->Handle);
                returnContext->Handle = nullptr;
            }
        }
    }

    for (; fr->OSReserved0 != fr->BeginIndex;
         fr->OSReserved0 = NetRingIncrementIndex(fr, fr->OSReserved0))
    {
        auto& context = m_fragmentContext.GetContext<FragmentContext>(fr->OSReserved0);

        if (context.Mdl != nullptr)
        {
            MDL_EX* mdlEx = CONTAINING_RECORD(context.Mdl, MDL_EX, Mdl);

            m_MdlPool.PushToReturn(mdlEx);
            context.Mdl = nullptr;
        }
        else
        {
            // the only case that fragment MDL context can be null if the client driver uses Cx buffer pool
            // and does fragment attachment itself
            NT_FRE_ASSERT(m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE);
        }
    }

    NT_FRE_ASSERT(pr->BeginIndex == pr->EndIndex);
    NT_FRE_ASSERT(fr->BeginIndex == fr->EndIndex);
    NT_FRE_ASSERT(m_NblPool.IsFull());
    NT_FRE_ASSERT(m_MdlPool.IsFull());
}

NxRxXlat::~NxRxXlat()
{
    // stop the EC and wait for wind down.
    m_executionContext.Terminate();

    while (! m_NblPool.IsEmpty())
    {
        auto nbl = m_NblPool.PopToUse();

        if (nbl->NetBufferListInfo[MediaSpecificInformation] != nullptr)
        {
            ExFreePool(nbl->NetBufferListInfo[MediaSpecificInformation]);
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

PMDL
NxRxXlat::GetCorrespondingMdl(UINT32 fragmentIndex)
{
    PMDL mdl = nullptr;

    if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE)
    {
        //client driver uses OS provided buffer pool, get the corresponding MDL based on
        //buffer index
        auto const fragmentBuffer = NetExtensionGetFragmentDataBuffer(
            &m_extensions.Extension.DataBuffer, fragmentIndex);

        size_t index = reinterpret_cast<size_t>(fragmentBuffer->Handle);

        mdl = &m_MdlPool[index]->Mdl;
    }
    else if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH ||
             m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER)
    {
        //
        auto& context = m_fragmentContext.GetContext<FragmentContext>(fragmentIndex);
        mdl = context.Mdl;

        context.Mdl = nullptr;
    }

    NT_FRE_ASSERT(mdl->Next == nullptr);

    return mdl;
}


bool
NxRxXlat::TransferDataBufferFromNetPacketToNbl(
    _In_ NET_PACKET * Packet,
    _In_ PNET_BUFFER_LIST Nbl,
    _In_ UINT32 PacketIndex)
{
    PNET_BUFFER nb = NET_BUFFER_LIST_FIRST_NB(Nbl);
    bool shouldIndicate = false;
    // ensure the NBL chain is broken
    Nbl->Next = nullptr;

    auto fr = NetRingCollectionGetFragmentRing(&m_rings);
    auto const firstFragment = NetRingGetFragmentAtIndex(fr, Packet->FragmentIndex);
    auto const firstVirtualAddress = NetExtensionGetFragmentVirtualAddress(
        &m_extensions.Extension.VirtualAddress, Packet->FragmentIndex);
    auto const firstReturnContext = NetExtensionGetFragmentReturnContext(
        &m_extensions.Extension.ReturnContext, Packet->FragmentIndex);

    PrefetchPacketPayloadForReceiveIndication(firstVirtualAddress->VirtualAddress, firstFragment->Offset);

    Packet->Layout = NxGetPacketLayout(
        m_adapterProperties.MediaType,
        &m_rings,
        m_extensions.Extension.VirtualAddress,
        Packet);

    m_metadataTranslator.TranslatePacketLayout(*Nbl, PacketIndex);
    m_metadataTranslator.TranslatePacketChecksum(*Nbl, PacketIndex);
    m_metadataTranslator.TranslatePacketRsc(*Nbl, PacketIndex);
    m_metadataTranslator.TranslatePacketRscTimestamp(*Nbl, PacketIndex);

    // store which queue this NB comes from
    GetRxContextFromNbl(Nbl)->Queue = this;

    //
    //2. packet's first fragment
    //
    PMDL currMdl = NET_BUFFER_FIRST_MDL(nb)
                 = NET_BUFFER_CURRENT_MDL(nb)
                 = GetCorrespondingMdl(Packet->FragmentIndex);

    fr->OSReserved0 = NetRingIncrementIndex(fr, fr->OSReserved0);

    NET_BUFFER_DATA_LENGTH(nb) = firstFragment->ValidLength;
    NET_BUFFER_DATA_OFFSET(nb) = firstFragment->Offset;

    NET_BUFFER_CURRENT_MDL_OFFSET(nb) = firstFragment->Offset;

    shouldIndicate = ReInitializeMdlForDataBuffer(
        firstFragment,
        firstVirtualAddress,
        firstReturnContext,
        currMdl,
        true);

    //
    //3. packet's additonal fragments
    //

    // Used for populating statistics
    auto frameSize = firstFragment->ValidLength;
    for (UINT32 i = 1; i < Packet->FragmentCount; ++i)
    {
        auto const index = NetRingAdvanceIndex(fr, Packet->FragmentIndex, i);
        auto const fragment = NetRingGetFragmentAtIndex(fr, index);
        auto const virtualAddress = NetExtensionGetFragmentVirtualAddress(
            &m_extensions.Extension.VirtualAddress, index);
        auto const returnContext = NetExtensionGetFragmentReturnContext(
            &m_extensions.Extension.ReturnContext, index);

        NET_BUFFER_DATA_LENGTH(nb) += (ULONG)fragment->ValidLength;

        NDIS_MDL_LINKAGE(currMdl) = GetCorrespondingMdl(index);

        currMdl = NDIS_MDL_LINKAGE(currMdl);

        fr->OSReserved0 = NetRingIncrementIndex(fr, fr->OSReserved0);

        shouldIndicate &= ReInitializeMdlForDataBuffer(
            fragment,
            virtualAddress,
            returnContext,
            currMdl,
            false);

        frameSize += fragment->ValidLength;
    }

    m_statistics.IncrementBy(NxStatisticsCounters::BytesOfData, frameSize);
    return shouldIndicate;
}

bool
NxRxXlat::ReInitializeMdlForDataBuffer(
    _In_ NET_FRAGMENT const * fragment,
    _In_ NET_FRAGMENT_VIRTUAL_ADDRESS const * VirtualAddress,
    _In_ NET_FRAGMENT_RETURN_CONTEXT const * ReturnContext,
    _In_ PMDL fragmentMdl,
    _In_ bool isFirstFragment)
{
    bool mdlBuilt = false;

    if (!isFirstFragment && fragment->Offset != 0)
    {
        //only the first fragment can have an offset, drop this packet
        mdlBuilt = false;
    }
    else
    {
        if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE ||
            m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH)
        {
            //
            //data buffer allocated by the translator, so all MDLs are pre-built
            //i.e. MDLs have already been welded together with the data buffer during initalization
            //no-op here
            //
            mdlBuilt = true;
        }
        else
        {
            MDL_EX* mdlEx = CONTAINING_RECORD(fragmentMdl, MDL_EX, Mdl);

            //the data buffer size must confront to the rx capability declared by the NIC
            if (fragment->Capacity <= m_rxDataBufferSize)
            {
                MmInitializeMdl(fragmentMdl, VirtualAddress->VirtualAddress, fragment->Capacity);
                MmBuildMdlForNonPagedPool(fragmentMdl);

                NT_FRE_ASSERT(fragmentMdl->Next == nullptr);

                //save return context to MDL_EX's context field
                mdlEx->MdlContext.RxBufferReturnContext = ReturnContext->Handle;
                mdlBuilt = true;
            }
            else
            {
                //if the packet received is larger than the driver claimed to be able to support,
                //mark this packet to be dropped
                mdlBuilt = false;

                //we still need to return the rx data buffer of the dropped packet back to the NIC
                mdlEx->MdlContext.RxBufferReturnContext = ReturnContext->Handle;
            }
        }
    }

    return mdlBuilt;
}


PNET_BUFFER_LIST
NxRxXlat::FreeReceivedDataBuffer(PNET_BUFFER_LIST nbl)
{
    PMDL currMdl = NET_BUFFER_CURRENT_MDL(NET_BUFFER_LIST_FIRST_NB(nbl));

    while (currMdl)
    {
        MDL_EX* mdlEx = CONTAINING_RECORD(currMdl, MDL_EX, Mdl);

        if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE)
        {
            //
            // we just need to return the buffer pointed by the MDL to the buffer pool
            //
            m_bufferPoolDispatch->NetClientFreeBuffers(m_bufferPool,
                                                       (SIZE_T*) &mdlEx->MdlContext.BufferIndex,
                                                       1);
        }
        else
        {
            if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER)
            {
                //
                // we need to invoke the client driver supplied buffer-return callback
                //

                m_adapterDispatch->ReturnRxBuffer(
                    m_adapter,
                    mdlEx->MdlContext.RxBufferReturnContext);

                mdlEx->MdlContext.RxBufferReturnContext = nullptr;
            }

            // return the MDL to the MDL pool
            m_MdlPool.PushToReturn(mdlEx);
        }

        currMdl = NDIS_MDL_LINKAGE(currMdl);
    }

    PNET_BUFFER_LIST next = NET_BUFFER_LIST_NEXT_NBL(nbl);
    NET_BUFFER_LIST_NEXT_NBL(nbl) = nullptr;
    NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(nbl)) = nullptr;

    m_NblPool.PushToReturn(nbl);

    return next;
}
