// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the
    receive path.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxNblDatapath.hpp"

#include "NxRxXlat.tmh"
#include "NxRxXlat.hpp"

#include <ndis/nblqueue.h>
#include <ndis/nblrsc.h>
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
#include <net/ieee8021qtypes_p.h>
#include <net/packethash_p.h>

#include "NxPerfTuner.hpp"
#include "NxNblSequence.h"
#include "NxPacketLayout.hpp"
#include "NxTranslationApp.hpp"
#include "NxRxNblContext.hpp"

#include <pktmonclnt.h>

static
EVT_EXECUTION_CONTEXT_POLL
    EvtRxPollQueueStarted;

static
EVT_EXECUTION_CONTEXT_POLL
    EvtRxPollQueueStopping;

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
                    _In_ size_t MdlCount,
                    _In_ bool IsPreBuilt)
{
    size_t slabSize;

    m_IsPreBuilt = IsPreBuilt;

    m_MdlSize = RTL_NUM_ALIGN_UP(RTL_SIZEOF_THROUGH_FIELD(MDL_EX, LogicalAddress) +
                         MmSizeOfMdl(DUMMY_VA, MemorySize),
                         sizeof(PVOID));

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
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_PACKET_EXTENSION_IEEE8021Q_NAME,
        NET_PACKET_EXTENSION_IEEE8021Q_VERSION_1,
        0,
        NET_PACKET_EXTENSION_IEEE8021Q_VERSION_1_SIZE,
        NetExtensionTypePacket,
    },
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_PACKET_EXTENSION_HASH_NAME,
        NET_PACKET_EXTENSION_HASH_VERSION_1,
        0,
        NET_PACKET_EXTENSION_HASH_VERSION_1_SIZE,
        NetExtensionTypePacket,
    },
};

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

PNET_BUFFER_LIST
GetLongestSpanWithSameQueue(
    _In_        PNET_BUFFER_LIST inputChain,
    _Out_       NBL_QUEUE *span,
    _Outptr_    NxRxXlat** queue,
    _Out_       PULONG numNbl)
{
    NdisInitializeNblQueue(span);

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

        NdisAppendNblChainToNblQueueFast(span, first, current);
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
    NxTranslationApp & App,
    size_t QueueId,
    NET_CLIENT_DISPATCH const * Dispatch,
    NET_CLIENT_ADAPTER Adapter,
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
) noexcept
    : Queue(
        App,
        QueueId,
        EvtRxPollQueueStarted,
        EvtRxPollQueueStopping)
    , m_dispatch(Dispatch)
    , m_adapter(Adapter)
    , m_adapterDispatch(AdapterDispatch)
    , m_metadataTranslator(m_rings, m_extensions)
    , m_packetContext(m_rings, NetRingTypePacket)
    , m_fragmentContext(m_rings, NetRingTypeFragment)
{
    m_adapterDispatch->GetProperties(m_adapter, &m_adapterProperties);
    m_nblDispatcher = static_cast<INxNblDispatcher *>(m_adapterProperties.NblDispatcher);
    m_pktmonLowerEdgeContext = reinterpret_cast<PKTMON_EDGE_CONTEXT const *>(m_adapterProperties.PacketMonitorLowerEdge);
    NdisInitializeNblQueue(&m_discardedNbl);
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
NxRxXlat::IsAffinitized(
    void
) const
{
    return m_affinitySet;
}

_Use_decl_annotations_
void
NxRxXlat::SetAffinity(
    PROCESSOR_NUMBER const & Affinity
)
{
    m_executionContextDispatch->SetAffinity(m_executionContext, &Affinity);
    m_affinitySet = true;
}

void
NxRxXlat::EcReturnBuffers()
{
    m_returnedNbls = 0;

    NBL_QUEUE nblsToReturn;
    m_returnedNblQueue.DequeueAll(&nblsToReturn);
    NdisAppendNblQueueToNblQueueFast(&nblsToReturn, &m_discardedNbl);

    UncoalesceReturnedNbls(nblsToReturn);

    auto currNbl = NdisPopAllFromNblQueue(&nblsToReturn);

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

_Use_decl_annotations_
void
NxRxXlat::UpdateRscStatisticsPreSoftwareFallback(
    NET_BUFFER_LIST const & NetBufferList,
    UINT64 HeaderLength,
    NblRscStatistics & RscStatistics
)
{
    if (!NdisTestNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_IPV4) &&
        !NdisTestNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_IPV6))
    {
        return;
    }

    if (!NdisTestNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_TCP))
    {
        return;
    }

    // Exit if active RSC is not enabled or hardware doesn't support RSC.
    if (NdisTestNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_IPV4))
    {
        if (!m_app.IsActiveRscIPv4Enabled() || !m_rscHardwareCapabilities.IPv4)
        {
            return;
        }
    }
    else if (NdisTestNblFlag(&NetBufferList, NDIS_NBL_FLAGS_IS_IPV6))
    {
        if (!m_app.IsActiveRscIPv6Enabled() || !m_rscHardwareCapabilities.IPv6)
        {
            return;
        }
    }

    auto const & rscInfo = *reinterpret_cast<NDIS_RSC_NBL_INFO const *>(
        &NetBufferList.NetBufferListInfo[TcpRecvSegCoalesceInfo]);

    if (rscInfo.Info.CoalescedSegCount > 0U)
    {
        // Add statistics for packets coalesced by hardware
        InterlockedIncrementNoFence64(reinterpret_cast<LONG64 *>(&RscStatistics.ScusGenerated));
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&RscStatistics.NblsCoalesced),
            rscInfo.Info.CoalescedSegCount);

        UINT64 bytesCoalesced = 0U;
        auto nb = NetBufferList.FirstNetBuffer;

        while (nb != nullptr)
        {
            NT_ASSERT(nb->DataLength >= HeaderLength);
            bytesCoalesced += nb->DataLength - HeaderLength;
            nb = nb->Next;
        }

        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&RscStatistics.BytesCoalesced),
            bytesCoalesced);
    }
    else
    {
        // Noncoalesced NBL even though active RSC offload is enabled and hardware support RSC.
        InterlockedIncrementNoFence64(reinterpret_cast<LONG64 *>(&RscStatistics.NblsAborted));
    }
}

_Use_decl_annotations_
void
NxRxXlat::UpdateRscStatisticsPostSoftwareFallback(
    ULONG PreRscFallbackNblCount,
    NblRscStatistics & RscStatistics
)
{
    // Add statistics for NBLs coalesced by RSClib
    auto const nblsCoalesced = m_rscContext.GetNblsCoalescedCount();

    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&RscStatistics.NblsCoalesced),
        nblsCoalesced);

    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&RscStatistics.NblsAborted),
        PreRscFallbackNblCount - nblsCoalesced);

    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&RscStatistics.ScusGenerated),
        m_rscContext.GetScusGeneratedCount());

    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&RscStatistics.BytesCoalesced),
        m_rscContext.GetBytesCoalescedCount());
}

_Use_decl_annotations_
void
NxRxXlat::UncoalesceReturnedNbls(
    NBL_QUEUE & NblQueue
)
{
    NET_BUFFER_LIST * nblChain = NdisPopAllFromNblQueue(&NblQueue);

    if (nblChain == nullptr)
    {
        return;
    }

    NET_BUFFER_LIST * outputNblTail = nullptr;
    ULONG numberOfNbls = 0U;
    m_rscContext.PerformReceiveSegmentUncoalescing(
        nblChain,
        outputNblTail,
        numberOfNbls);

    NdisAppendNblChainToNblQueueFast(&NblQueue, nblChain, outputNblTail);
}

_Use_decl_annotations_
void
NxRxXlat::CoalesceNblsToIndicate(
    NxNblSequence & NblSequence
)
{
    if (NblSequence.GetReceiveFlags() & NDIS_RECEIVE_FLAGS_RESOURCES)
    {
        return;
    }

    // If hardware supports coalescing both IPv4 and IPv6 packets,
    // don't bother to iterate through the NBLs. Non-IPv4 and non-IPv6 packets
    // will be skipped in PerformReceiveSegmentCoalescing().
    if (m_rscHardwareCapabilities.IPv4 && m_rscHardwareCapabilities.IPv6)
    {
        return;
    }

    auto const originalNblCount = NblSequence.GetCount();
    NET_BUFFER_LIST * nblChain = NblSequence.PopAllNblsFromNblQueue();

    NET_BUFFER_LIST * outputNblTail = nullptr;
    ULONG numberOfNbls = 0U;

    m_rscContext.PerformReceiveSegmentCoalescing(
        nblChain,
        m_rscHardwareCapabilities.IPv4,
        m_rscHardwareCapabilities.IPv6,
        outputNblTail,
        numberOfNbls);

    NT_FRE_ASSERT(originalNblCount >= numberOfNbls);

    NblSequence.PushNblsToNblQueue(nblChain, outputNblTail, numberOfNbls);
}

void
NxRxXlat::ReclaimSkippedFragments(
    _In_ UINT32 CurrentIndex
)
{
    auto fr = NetRingCollectionGetFragmentRing(&m_rings);

    if (m_rxBufferAllocationMode != NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE)
    {
        for (; fr->OSReserved0 != CurrentIndex;
            fr->OSReserved0 = NetRingIncrementIndex(fr, fr->OSReserved0))
        {
            PMDL fragmentMdl = GetCorrespondingMdl(fr->OSReserved0);
            MDL_EX* mdlEx = CONTAINING_RECORD(fragmentMdl, MDL_EX, Mdl);
            m_MdlPool.PushToReturn(mdlEx);
        }
    }
    else
    {
        fr->OSReserved0 = CurrentIndex;
    }
}

void
NxRxXlat::EcIndicateNblsToNdis()
{
    auto pr = NetRingCollectionGetPacketRing(&m_rings);
    auto fr = NetRingCollectionGetFragmentRing(&m_rings);

    NxNblSequence nblsToIndicate;
    RxPacketCounters counters = {};

    auto const osReserved0 = pr->OSReserved0;

    for (; ((pr->OSReserved0 != pr->BeginIndex) && (nblsToIndicate.GetCount() < NBL_RECEIVE_BATCH_SIZE));
         pr->OSReserved0 = NetRingIncrementIndex(pr, pr->OSReserved0))
    {
        auto & context = m_packetContext.GetContext<PacketContext>(pr->OSReserved0);
        auto packet = NetRingGetPacketAtIndex(pr, pr->OSReserved0);

        NT_FRE_ASSERT(context.NetBufferList != nullptr);
        NT_FRE_ASSERT(NET_BUFFER_LIST_NEXT_NBL(context.NetBufferList) == nullptr);

        if (! packet->Ignore &&
            TransferDataBufferFromNetPacketToNbl(counters, packet, context.NetBufferList, pr->OSReserved0))
        {
            auto const headerLength = packet->Layout.Layer2HeaderLength +
                packet->Layout.Layer3HeaderLength +
                packet->Layout.Layer4HeaderLength;
            UpdateRscStatisticsPreSoftwareFallback(*context.NetBufferList, headerLength, counters.Rsc);
            nblsToIndicate.AddNbl(context.NetBufferList);
        }
        else
        {
            NdisAppendSingleNblToNblQueue(&m_discardedNbl, context.NetBufferList);
        }

        context.NetBufferList = nullptr;
    }

    if (pr->OSReserved0 == pr->BeginIndex)
    {
        // check if any skipped fragments after the last fragment of last packet
        ReclaimSkippedFragments(fr->BeginIndex);
    }

    if (nblsToIndicate)
    {
        PKTMON_LOG_NBL_NDIS(
            const_cast<PKTMON_EDGE_CONTEXT *>(m_pktmonLowerEdgeContext),
            nblsToIndicate.GetNblQueue().First,
            PktMonDir_In);

        if (m_app.IsActiveRscIPv4Enabled() || m_app.IsActiveRscIPv6Enabled())
        {
            auto const originalNblCount = nblsToIndicate.GetCount();
            CoalesceNblsToIndicate(nblsToIndicate);
            UpdateRscStatisticsPostSoftwareFallback(originalNblCount, counters.Rsc);
        }

        m_outstandingNbls += nblsToIndicate.GetCount();

        if (!m_nblDispatcher->IndicateReceiveNetBufferLists(
                counters,
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
            NdisAppendNblQueueToNblQueueFast(&m_discardedNbl, &nblsToIndicate.GetNblQueue());
        }
    }

    m_completedPackets = NetRingGetRangeCount(pr, osReserved0, pr->OSReserved0);
}

void
NxRxXlat::EcUpdatePerfCounter()
{
    auto pr = NetRingCollectionGetPacketRing(&m_rings);

    InterlockedIncrementNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Perf.IterationCount));
    InterlockedAddNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Perf.NblPending), m_outstandingNbls);
    InterlockedAddNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Perf.PacketsCompleted), m_returnedNbls);
    InterlockedAddNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Perf.QueueDepth),
        NetRingGetRangeCount(pr, pr->BeginIndex, pr->EndIndex));
}

_Use_decl_annotations_
void
EvtRxPollQueueStarted(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    auto nxRxXlat = reinterpret_cast<NxRxXlat *>(Context);
    nxRxXlat->ReceiveNbls(Parameters);
}

_Use_decl_annotations_
void
EvtRxPollQueueStopping(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    auto nxRxXlat = reinterpret_cast<NxRxXlat *>(Context);
    nxRxXlat->ReceiveNblsStopping(Parameters);
}

_Use_decl_annotations_
void
NxRxXlat::ReceiveNbls(
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    EcReturnBuffers();

    // provide buffers to NetAdapter only if running
    EcPrepareBuffersForNetAdapter();

    EcYieldToNetAdapter();
    EcIndicateNblsToNdis();
    EcUpdatePerfCounter();

    Parameters->WorkCounter = m_completedPackets + m_returnedNbls;
}

_Use_decl_annotations_
void
NxRxXlat::ReceiveNblsStopping(
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    // The termination condition is that all packets have been returned from the NIC.
    auto const pr = NetRingCollectionGetPacketRing(&m_rings);
    auto const fr = NetRingCollectionGetFragmentRing(&m_rings);
    if (pr->BeginIndex == pr->EndIndex && fr->BeginIndex == fr->EndIndex)
    {
        EcRecoverBuffers();
        m_readyToStop.Set();

        return;
    }

    EcReturnBuffers();

    EcYieldToNetAdapter();
    EcIndicateNblsToNdis();
    EcUpdatePerfCounter();

    Parameters->WorkCounter = m_completedPackets + m_returnedNbls;
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
            &m_queueDispatch,
            &m_executionContext,
            &m_executionContextDispatch),
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

    m_executionContextDispatch->RegisterNotification(
        m_executionContext,
        &m_nblNotification);

    if (m_extensions.Extension.Rsc.Enabled)
    {
        m_adapterDispatch->OffloadDispatch.GetRscHardwareCapabilities(
            m_adapter,
            &m_rscHardwareCapabilities);
    }

    m_rscContext.Initialize();

    return STATUS_SUCCESS;
}

void
NxRxXlat::EnumerateBufferPoolCallback(
    _In_ PVOID Context,
    _In_ SIZE_T Index,
    _In_ UINT64 LogicalAddress,
    _In_ PVOID VirtualAddress)
{
    NxRxXlat* nxRxXlat = reinterpret_cast<NxRxXlat*>(Context);

    NT_FRE_ASSERT(nxRxXlat->m_MdlPool.IsPreBuilt());

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
    perfCharacteristics.Nic.MediaType = m_adapterProperties.MediaType;
    perfCharacteristics.FragmentRingNumberOfElementsHint = datapathCapabilities.PreferredRxFragmentRingSize;
    perfCharacteristics.MaximumFragmentBufferSize = datapathCapabilities.MaximumRxFragmentSize;
    perfCharacteristics.NominalLinkSpeed = datapathCapabilities.NominalMaxRxLinkSpeed;
    perfCharacteristics.MaxPacketSizeWithRsc = datapathCapabilities.MaximumRxFragmentSize + m_backfillSize;

    NxPerfTunerCalculateRxParameters(&perfCharacteristics, &perfParameters);
    m_rxNumPackets = perfParameters.PacketRingElementCount;
    m_rxNumFragments = perfParameters.FragmentRingElementCount;
    m_rxNumDataBuffers = perfParameters.NumberOfBuffers;

    m_backfillSize = 0;
    m_rxDataBufferSize = datapathCapabilities.MaximumRxFragmentSize + m_backfillSize;
    m_rxBufferAllocationMode = datapathCapabilities.RxMemoryManagementMode;

    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties;
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_MdlPool.Initialize(
            m_rxDataBufferSize,
            perfParameters.NumberOfBuffers,
            m_rxBufferAllocationMode != NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_NblPool.Initialize(m_adapterProperties.NdisAdapterHandle,
                             perfParameters.NumberOfNbls,
                             adapterProperties.MediaType));

    if (m_MdlPool.IsPreBuilt())
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
    NT_FRE_ASSERT(false);
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
        if (!IsListEmpty(&m_nblNotification.Link))
        {
            m_executionContextDispatch->UnregisterNotification(m_executionContext, &m_nblNotification);
        }

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

    if (InterlockedExchange(&m_nblNotificationEnabled, FALSE) == TRUE)
    {
        m_executionContextDispatch->Notify(m_executionContext);
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
        auto& context = m_fragmentContext.GetContext<FragmentContext>(fragmentIndex);
        mdl = context.Mdl;

        context.Mdl = nullptr;
    }

    NT_FRE_ASSERT(mdl->Next == nullptr);

    return mdl;
}

bool
NxRxXlat::TransferDataBufferFromNetPacketToNbl(
    _Inout_ RxPacketCounters & Counters,
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

    // 1. check if any skipped fragment entries before this packet's fragments and reclaim them back to pools
    ReclaimSkippedFragments(Packet->FragmentIndex);

    PrefetchPacketPayloadForReceiveIndication(firstVirtualAddress->VirtualAddress, firstFragment->Offset);

    NxGetPacketLayout(
        m_adapterProperties.MediaType,
        &m_rings,
        m_extensions.Extension.VirtualAddress,
        Packet);

    m_metadataTranslator.TranslatePacketLayout(*Nbl, PacketIndex);
    m_metadataTranslator.TranslatePacketChecksum(*Nbl, PacketIndex);
    m_metadataTranslator.TranslatePacketRsc(*Nbl, PacketIndex);
    m_metadataTranslator.TranslatePacketRscTimestamp(*Nbl, PacketIndex);
    m_metadataTranslator.TranslatePacketIeee8021q(*Nbl, PacketIndex);
    m_metadataTranslator.TranslatePacketHash(*Nbl, PacketIndex);

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

    GetRxContextFromNb(nb)->HasMultipleFragments = (Packet->FragmentCount > 1);

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

        currMdl->Next = GetCorrespondingMdl(index);

        currMdl = currMdl->Next;

        fr->OSReserved0 = NetRingIncrementIndex(fr, fr->OSReserved0);

        shouldIndicate &= ReInitializeMdlForDataBuffer(
            fragment,
            virtualAddress,
            returnContext,
            currMdl,
            false);

        frameSize += fragment->ValidLength;
    }

    if (shouldIndicate)
    {
        auto const destinationType = NxGetPacketAddressType(
            static_cast<NET_PACKET_LAYER2_TYPE>(Packet->Layout.Layer2Type),
            *nb);

        switch (destinationType)
        {

        case AddressType::Unicast:
            Counters.Unicast.Packets += 1;
            Counters.Unicast.Bytes += frameSize;

            break;

        case AddressType::Multicast:
            Counters.Multicast.Packets += 1;
            Counters.Multicast.Bytes += frameSize;

            break;

        case AddressType::Broadcast:

            Counters.Broadcast.Packets += 1;
            Counters.Broadcast.Bytes += frameSize;

            break;

        }
    }

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
    bool isFragmentValid = true;

    if ((fragment->Capacity > m_rxDataBufferSize) ||
        (fragment->Offset + fragment->ValidLength > fragment->Capacity) ||
        (!isFirstFragment && fragment->Offset != 0))
    {
        //the data buffer size must conform to the rx capability declared by the NIC,
        //and its content must be within capacity
        //and only the first fragment can have an offset.
        //
        //mark this fragment as invalid if these condtions are broken, 
        //and the entire packet will be dropped
        //
        isFragmentValid = false;
    }

    //
    //for data buffer allocated by the translator, all MDLs from the MDL pool have been
    //pre-built, i.e. MDLs have already been welded together with the data buffer during 
    //initalization, no-op.
    //
    //for other cases, we need to intialize MDL on the fly here for every data buffer
    //

    if (!m_MdlPool.IsPreBuilt())
    {
        MDL_EX* mdlEx = CONTAINING_RECORD(fragmentMdl, MDL_EX, Mdl);

        if (isFragmentValid) 
        {
            MmInitializeMdl(fragmentMdl, VirtualAddress->VirtualAddress, fragment->Capacity);
            MmBuildMdlForNonPagedPool(fragmentMdl);

            NT_FRE_ASSERT(fragmentMdl->Next == nullptr);


            mdlEx->MdlContext.RxBufferReturnContext = ReturnContext->Handle;
        }

        //for all fragments, including those marked as invalid,
        //save return context to MDL_EX's context field, so they 
        //can be given back to the NIC
        mdlEx->MdlContext.RxBufferReturnContext = ReturnContext->Handle;
    }

    return isFragmentValid;
}

NET_BUFFER_LIST*
NxRxXlat::FreeReceivedDataBuffer(
    _Inout_ PNET_BUFFER_LIST nbl
)
{
    NET_BUFFER* nb = NET_BUFFER_LIST_FIRST_NB(nbl);
    MDL* currMdl = NET_BUFFER_CURRENT_MDL(nb);

    NT_FRE_ASSERT(nb->Next == nullptr);

    while (currMdl)
    {
        MDL_EX* mdlEx = CONTAINING_RECORD(currMdl, MDL_EX, Mdl);

        currMdl = currMdl->Next;

        if (GetRxContextFromNb(nb)->HasMultipleFragments)
        {
            mdlEx->Mdl.Next = nullptr;
        }
        else
        {
            NT_FRE_ASSERT(mdlEx->Mdl.Next == nullptr);
        }

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
    }

    PNET_BUFFER_LIST next = NET_BUFFER_LIST_NEXT_NBL(nbl);
    NET_BUFFER_LIST_NEXT_NBL(nbl) = nullptr;
    NET_BUFFER_FIRST_MDL(nb) = NET_BUFFER_CURRENT_MDL(nb) = nullptr;

    m_NblPool.PushToReturn(nbl);

    return next;
}

const NxRxCounters &
NxRxXlat::GetStatistics(
    void
) const
{
    return m_statistics;
}
