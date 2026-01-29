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

#include <NetMemorySharedTypes.h>

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
#include <net/ieee8021qtypes_p.h>
#include <net/packethash_p.h>
#include <net/checksum.h>
#include <net/rsc.h>
#include <net/ieee8021q.h>
#include <net/packethash.h>

#include <net/databuffer_p.h>

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
    NET_FRAGMENT_RETURN_CONTEXT_HANDLE RxBufferReturnContext;
};

struct MDL_EX
{
    EX_MDL_CONTEXT MdlContext;
    NET_MEMORY_MDL_WRAPPER *NetMemoryMdl;
    UINT64 LogicalAddress;
    void* VirtualAddress;
    MDL Mdl;
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

    m_MdlSize = RTL_NUM_ALIGN_UP(RTL_SIZEOF_THROUGH_FIELD(MDL_EX, VirtualAddress) +
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
        NET_PACKET_EXTENSION_RSC_VERSION_2,
        0,
        NET_PACKET_EXTENSION_RSC_VERSION_2_SIZE,
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
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_FRAGMENT_EXTENSION_NET_MEMORY_NAME,
        NET_FRAGMENT_EXTENSION_NET_MEMORY_VERSION_1,
        0,
        NET_FRAGMENT_EXTENSION_NET_MEMORY_VERSION_1_SIZE,
        NetExtensionTypeFragment,
    }
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

        queue->QueueReturnedNetBufferLists(&span, numNbl);
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
}

NET_CLIENT_QUEUE
NxRxXlat::GetQueue(
    void
) const
{
    return m_queue;
}

void
NxRxXlat::EcReturnBuffers()
{
    m_returnedNbls = 0;

    ndis::nbl_queue nblsToReturn;

    auto dequeuedNblsCount = static_cast<ULONG>(m_returnedNblQueue.DequeueAll(&nblsToReturn));
    NT_ASSERT(m_outstandingNbls >= dequeuedNblsCount);
    m_outstandingNbls -= dequeuedNblsCount;

    nblsToReturn.append(&m_nblsToDiscard);

    UncoalesceReturnedNbls(nblsToReturn);

    auto currNbl = nblsToReturn.clear();

    while (currNbl)
    {
        ++m_returnedNbls;
        currNbl = FreeReceivedDataBuffer(currNbl);
    }
}

ULONG
NxRxXlat::EcReturnDriverAllocatedBuffers(
    void
)
{
    if (m_rxBufferAllocationMode != NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER_V2 || m_chainOfMdlsToReturn == nullptr)
    {
        return 0;
    }

    ULONG buffersPostedIntoReturnRing = 0;

    NET_RING* returnBuffersRing = NetRingCollectionGetFragmentReturnContextRing(&m_rings);
    UINT32 const lastIndex = (returnBuffersRing->BeginIndex - 1) & returnBuffersRing->ElementIndexMask;

    while (returnBuffersRing->EndIndex != lastIndex)
    {
        if (m_chainOfMdlsToReturn == nullptr)
        {
            break;
        }

        PMDL mdlToReturn = m_chainOfMdlsToReturn;
        m_chainOfMdlsToReturn = m_chainOfMdlsToReturn->Next;
        mdlToReturn->Next = nullptr;

        MDL_EX* mdlEx = CONTAINING_RECORD(mdlToReturn, MDL_EX, Mdl);

        NET_FRAGMENT_RETURN_CONTEXT* netMemoryReturnContextOut =
            NetRingGetFragmentReturnContextAtIndex(returnBuffersRing, returnBuffersRing->EndIndex);

        netMemoryReturnContextOut->Handle = mdlEx->MdlContext.RxBufferReturnContext;

        mdlEx->NetMemoryMdl->OsOwnedReferenceCount--;
        MmPrepareMdlForReuse(mdlToReturn);
        m_MdlPool.PushToReturn(mdlEx);

        buffersPostedIntoReturnRing++;
        returnBuffersRing->EndIndex = NetRingIncrementIndex(returnBuffersRing, returnBuffersRing->EndIndex);
    }

    return buffersPostedIntoReturnRing;
}

void
NxRxXlat::EcPrepareBuffersForNetAdapter()
{
    //
    // NxRxXlat can manage buffers in 3 different modes, which were developed in different times to support different needs.
    // These modes re-use certain structures (such as m_mdlPool and m_fragmentContext) in different ways, which may lead to confusion.
    // Here's a brief description of how each mode works.

    // NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH (KMDF + UMDF)
    //
    // OS prepares the fragment ring for the driver. For each fragment OS allocates internally an MDL from m_mdlPool. 
    // "Internally" means that the driver will not have direct access to the MDL: see m_fragmentContext.
    // All MDLs are initialized and built(mapped) to the buffers allocated previously by the OS. 
    // Also, OS fills VA fragment ring extension with VAs from these buffers.
    // Driver uses provided VA to deliver the payload.

    // NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER (KMDF only)
    //
    // OS prepares the fragment ring for the driver; As before, for each fragment OS allocates an MDL from m_mdlPool,
    // however this time MDLs are only allocated, but not built for any particular address. Driver fills VA fragment
    // ring extension with its own VAs that point to the buffers it pre-allocated, as well as NET_FRAGMENT_RETURN_CONTEXT extension
    // to give us an opaque ID that will be used to "return" the buffer back to the driver. When OS processes the packets received
    // from the driver, it picks up MDL from m_fragmentContext and builds it for the driver-specified VA. When NDIS returns NBL,
    // OS calls the driver using m_adapterDispatch->ReturnRxBuffer callback for each buffer to indicate that the driver can reuse it.

    // NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER_V2 (KMDF + UMDF)
    //
    // The mode NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER has certain drawbacks in KMDF, and also can't be ported to UMDF.
    // As a result, the new mode was introduced to address these shortcomings.
    // OS prepares the fragment ring exactly the same way as in NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER.
    // This time, however, driver doesn't use VA extension, and instead uses NetMemory extension to set opaque (to the driver) pointers to 
    // pre-allocated memory. This memory was registered by the driver using NetMemoryRegister calls.
    // Driver still uses NET_FRAGMENT_RETURN_CONTEXT extension to give us an opaque (to NetAdapter) context pointer that
    // will be used to "return" the buffers. MDLs given to NDIS are still picked up from m_fragmentContext/m_ mdlPool, but now 
    // they aren't build for driver’s VAs, but re-initialized as partial MDLs from the memory
    // that the driver pre-registered using NetMemoryRegister calls. The pre-registered MDLs
    // are stored internally stored in Nx(Rd)Memory. Buffers are now returned to the driver using a new ring, DataBufferRing,
    // and not via callback like in the previous mode.
    // 


    auto pr = NetRingCollectionGetPacketRing(&m_rings);
    auto fr = NetRingCollectionGetFragmentRing(&m_rings);
    auto const prLastIndex = (pr->OSReserved0 - 1) & pr->ElementIndexMask;
    auto const frLastIndex = (fr->OSReserved0 - 1) & fr->ElementIndexMask;

    // 1. prepare the packet ring
    m_postedPackets = 0;

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

        m_postedPackets++;
    }

    // 2. prepare the fragment ring
    for (; fr->EndIndex != frLastIndex;
         fr->EndIndex = NetRingIncrementIndex(fr, fr->EndIndex))
    {
        auto fragment = NetRingGetFragmentAtIndex(fr, fr->EndIndex);

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
            virtualAddress->VirtualAddress = mdlEx->VirtualAddress;

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
    ReceiveSegmentCoalescingBatch& rscBatch,
    NblRscStatistics & RscStatistics
)
{
    // Add statistics for NBLs coalesced by RSClib
    auto const nblsCoalesced = rscBatch.GetNblsCoalescedCount();

    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&RscStatistics.NblsCoalesced),
        nblsCoalesced);

    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&RscStatistics.NblsAborted),
        rscBatch.GetNblsAttemptedCount() - nblsCoalesced);

    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&RscStatistics.ScusGenerated),
        rscBatch.GetScusGeneratedCount());

    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&RscStatistics.BytesCoalesced),
        rscBatch.GetBytesCoalescedCount());
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

void
NxRxXlat::ReclaimSkippedFragments(
    _In_ UINT32 CurrentIndex
)
{
    auto fr = NetRingCollectionGetFragmentRing(&m_rings);

    for (; fr->OSReserved0 != CurrentIndex;
        fr->OSReserved0 = NetRingIncrementIndex(fr, fr->OSReserved0))
    {
        PMDL fragmentMdl = GetCorrespondingMdl(fr->OSReserved0);
        MDL_EX* mdlEx = CONTAINING_RECORD(fragmentMdl, MDL_EX, Mdl);
        m_MdlPool.PushToReturn(mdlEx);
    }
}

void
NxRxXlat::EcIndicateNblsToNdis()
{
    //
    // Try to indicate NBLs to NDIS in batches of NBL_BATCH_SIZE:
    //
    // if the client driver completes less packets than NBL_BATCH_SIZE in the current poll iteration,
    // we will try to defer the indication call to a later poll iteration till we have enough packets
    // acculmated
    //
    // if the client driver completes more packets than NBL_BATCH_SIZE in the current poll iteration,
    // we will only indicate a single batch size in the current poll iteration, and defer the rest of
    // packets indication to next poll iterations
    //
    auto pr = NetRingCollectionGetPacketRing(&m_rings);
    auto fr = NetRingCollectionGetFragmentRing(&m_rings);

    auto const osReserved0 = pr->OSReserved0;

    RxPacketCounters counters = {};

    //
    // In a single poll iteration, we will drain at maximum m_maxPackets packets from the
    // client driver and indicate them up. If there are packets yet to be indicated from previous poll
    // iterations, we need count against them as well.
    //
    ULONG maxNumberOfNblsToIndicate =
        m_maxPackets > m_nblsToIndicate.GetCount() ?
        m_maxPackets - m_nblsToIndicate.GetCount() :
        0;
    RtlHomogenousSequence<USHORT> frameTypes;

    //
    // Variants hold during batch RSCing:
    // Given (lastNblInChain, nblCount) = NdisLastNblInNblChainWithCount(nblsToIndicateHead):
    // 1) nblsToIndicateTail == &lastNblInChain->Next
    // 2) numberOfNblsToIndicate == nblCount
    //
    NET_BUFFER_LIST * nblsToIndicateHead = nullptr;
    NET_BUFFER_LIST ** nblsToIndicateTail = &nblsToIndicateHead;
    ULONG numberOfNblsToIndicate = 0;

    //
    // We attempt to coalesce every packet as we scan through the returned packets from client
    // driver
    //
    ReceiveSegmentCoalescingBatch rscBatch(
        m_rscContext,
        m_app.IsActiveRscIPv4Enabled() || m_app.IsActiveRscIPv6Enabled(),
        m_rscHardwareCapabilities.IPv4,
        m_rscHardwareCapabilities.IPv6,
        m_app.IsActiveUdpRscEnabled(),
        m_rscHardwareCapabilities.Udp,
        m_adapterProperties.MediaType);

    for (; (pr->OSReserved0 != pr->BeginIndex) && (numberOfNblsToIndicate < maxNumberOfNblsToIndicate);
            pr->OSReserved0 = NetRingIncrementIndex(pr, pr->OSReserved0))
    {
        auto & context = m_packetContext.GetContext<PacketContext>(pr->OSReserved0);
        auto packet = NetRingGetPacketAtIndex(pr, pr->OSReserved0);

        NT_FRE_ASSERT(context.NetBufferList != nullptr);
        NT_FRE_ASSERT(NET_BUFFER_LIST_NEXT_NBL(context.NetBufferList) == nullptr);

        if (! packet->Ignore &&
            TransferDataBufferFromNetPacketToNbl(counters, packet, context.NetBufferList, pr->OSReserved0))
        {
            frameTypes.AddValue(reinterpret_cast<USHORT>(context.NetBufferList->NetBufferListInfo[NetBufferListFrameType]));

            PKTMON_LOG_NBL_NDIS(
                const_cast<PKTMON_EDGE_CONTEXT *>(m_pktmonLowerEdgeContext),
                context.NetBufferList,
                PktMonDir_In);

            auto const headerLength = packet->Layout.Layer2HeaderLength +
                packet->Layout.Layer3HeaderLength +
                packet->Layout.Layer4HeaderLength;

            //
            // Attemp to coalesce the current packet
            //
            UpdateRscStatisticsPreSoftwareFallback(*context.NetBufferList, headerLength, counters.Rsc);

            rscBatch.TryCoalesceOneReceiveSegment(
                context.NetBufferList,
                nblsToIndicateTail,
                numberOfNblsToIndicate);
        }
        else
        {
            m_nblsToDiscard.append_one_nbl(context.NetBufferList);
        }

        context.NetBufferList = nullptr;
    }

    //
    // Terminate the current batch of coalesced packets
    //
    rscBatch.FinalizeCoalescing(nblsToIndicateTail, numberOfNblsToIndicate);
    UpdateRscStatisticsPostSoftwareFallback(rscBatch, counters.Rsc);

    if (pr->OSReserved0 == pr->BeginIndex)
    {
        //
        // Check if any skipped fragments after the last fragment of last packet
        //
        ReclaimSkippedFragments(fr->BeginIndex);
    }

    m_completedPackets = NetRingGetRangeCount(pr, osReserved0, pr->OSReserved0);

    //
    // If client driver returned any packets in current iteration, append them to
    // the list
    //
    if (nblsToIndicateHead)
    {
        m_nblsToIndicate.AppendNblChain(nblsToIndicateHead, nblsToIndicateTail, numberOfNblsToIndicate, frameTypes);
    }

    //
    // We should indicate to ndis in this iteration if
    //   1) the list has accumulated to m_maxPackets size, or
    //   2) this is last poll iteration before EC decides to wait
    // However, current EC.sys is not able to fulfill our demand today;
    // so we are indicating to NDIS as long as we have packet(s).
    //
    if (m_nblsToIndicate.GetCount() > 0)
    {
        m_outstandingNbls += m_nblsToIndicate.GetCount();

        if (!m_nblDispatcher->IndicateReceiveNetBufferLists(
                counters,
                m_nblsToIndicate.GetNbls(),
                NDIS_DEFAULT_PORT_NUMBER,
                m_nblsToIndicate.GetCount(),
                m_nblsToIndicate.GetReceiveFlags()))
        {
            //
            // While stopping the queue, the NBL packet gate may close while this thread
            // is still trying to indicate a receive.
            //
            // If that happens, we're in the process of tearing down this queue, so just
            // mark the NBLs as returned and bail out.
            //
            m_nblsToDiscard.append(m_nblsToIndicate.GetNblQueue());
            m_outstandingNbls -= m_nblsToIndicate.GetCount();
        }

        m_nblsToIndicate.Reset();
    }
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
    ULONG driverBuffersReturned = 0;
    if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER_V2)
    {
        driverBuffersReturned = EcReturnDriverAllocatedBuffers();
    }

    // provide buffers to NetAdapter only if running
    EcPrepareBuffersForNetAdapter();

#ifndef NETCX_REDIRECTOR
    EcYieldToNetAdapter();
#endif

    UpdateMaxPacketsFromKnobs(Parameters->Irql);
    EcIndicateNblsToNdis();

    Parameters->WorkCounter = m_completedPackets + m_returnedNbls + driverBuffersReturned;
}

_Use_decl_annotations_
void
NxRxXlat::ReceiveNblsStopping(
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    bool allDriverAllocatedBuffersReturned = true;
    ULONG driverBuffersReturned = 0;
    if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER_V2)
    {
        EcReturnBuffers();
        driverBuffersReturned = EcReturnDriverAllocatedBuffers();

        NET_RING* returnBuffersRing = NetRingCollectionGetFragmentReturnContextRing(&m_rings);
        const bool ringIsEmpty = returnBuffersRing->EndIndex == returnBuffersRing->BeginIndex;
        allDriverAllocatedBuffersReturned = ringIsEmpty && m_chainOfMdlsToReturn == nullptr;
    }

    // The termination condition is that all packets have been returned from the NIC.
    auto const pr = NetRingCollectionGetPacketRing(&m_rings);
    auto const fr = NetRingCollectionGetFragmentRing(&m_rings);

    if (pr->BeginIndex == pr->EndIndex &&
        fr->BeginIndex == fr->EndIndex &&
        allDriverAllocatedBuffersReturned)
    {
        EcRecoverBuffers();
        m_readyToStop.Set();
        return;
    }

    EcReturnBuffers();
#ifndef NETCX_REDIRECTOR
    EcYieldToNetAdapter();
#endif

    UpdateMaxPacketsFromKnobs(Parameters->Irql);
    EcIndicateNblsToNdis();

    Parameters->WorkCounter = m_completedPackets + m_returnedNbls + driverBuffersReturned;
}

NTSTATUS
NxRxXlat::Initialize(
)
{
    NT_FRE_ASSERT(ARRAYSIZE(RxQueueExtensions) == ARRAYSIZE(m_extensions.Extensions));

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CreateVariousPools(),
        "Failed to create pools");

    NET_CLIENT_QUEUE_CONFIG config;
    NET_CLIENT_QUEUE_CONFIG_INIT(
        &config,
        m_rxNumPackets,
        m_rxNumFragments);

    config.Extensions = &RxQueueExtensions[0];
    config.NumberOfExtensions = ARRAYSIZE(RxQueueExtensions);

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

    return STATUS_SUCCESS;
}

void
NxRxXlat::EnumerateBufferPoolCallback(
    _In_ PVOID Context,
    _In_ NET_DATA_HEADER* const DataHeader)
{
    NxRxXlat* nxRxXlat = reinterpret_cast<NxRxXlat*>(Context);

    NT_FRE_ASSERT(nxRxXlat->m_MdlPool.IsPreBuilt());

    MDL_EX* mdlEx = nxRxXlat->m_MdlPool[DataHeader->Index];
    mdlEx->LogicalAddress = DataHeader->LogicalAddress;

#ifdef NETCX_REDIRECTOR
    mdlEx->VirtualAddress = DataHeader->UmVirtualAddress;
#else
    mdlEx->VirtualAddress = DataHeader->KmVirtualAddress;
#endif
    
    MmInitializeMdl(&mdlEx->Mdl, DataHeader->KmVirtualAddress, nxRxXlat->m_rxDataBufferSize);
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

    m_backfillSize = 0;
    m_rxDataBufferSize = datapathCapabilities.MaximumRxFragmentSize + m_backfillSize;
    m_rxBufferAllocationMode = datapathCapabilities.RxMemoryManagementMode;

    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties;
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_MdlPool.Initialize(
            m_rxDataBufferSize,
            perfParameters.NumberOfBuffers,
            m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_NblPool.Initialize(m_adapterProperties.NdisAdapterHandle,
                             perfParameters.NumberOfNbls,
                             adapterProperties.MediaType));
    
    if (m_MdlPool.IsPreBuilt()) // a.k.a if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH)
    {
        // create buffer pool if the client driver wants NetCx to allocate Rx buffer
        NET_CLIENT_BUFFER_POOL_CONFIG bufferPoolConfig = {
            &datapathCapabilities.RxMemoryConstraints,
            perfParameters.NumberOfBuffers,
            m_rxDataBufferSize,
            m_backfillSize,
            0,
            MM_ANY_NODE_OK,                     // use the numa node supplied by adapter capabilities
            NET_CLIENT_BUFFER_POOL_FLAGS_NONE,  // non-serialized version
            adapterProperties.Process
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

        NT_FRE_ASSERT(context.Mdl != nullptr);

        MDL_EX* mdlEx = CONTAINING_RECORD(context.Mdl, MDL_EX, Mdl);
        m_MdlPool.PushToReturn(mdlEx);
        context.Mdl = nullptr;
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
    _In_ NBL_QUEUE* NblChain,
    _In_ ULONG NblCount
)
{
    NBL_COUNTED_QUEUE countedQueue;
    countedQueue.Queue.First = NblChain->First;
    countedQueue.Queue.Last = NblChain->Last;
    countedQueue.NblCount = NblCount;

    m_returnedNblQueue.Enqueue(&countedQueue);

    if (InterlockedExchange(&m_nblNotificationEnabled, FALSE) == TRUE)
    {
        m_executionContextDispatch->Notify(m_executionContext);
    }
}

PMDL
NxRxXlat::GetCorrespondingMdl(UINT32 fragmentIndex)
{
    auto& context = m_fragmentContext.GetContext<FragmentContext>(fragmentIndex);
    auto mdl = context.Mdl;
    context.Mdl = nullptr;

    NT_FRE_ASSERT(mdl->Next == nullptr);

    return mdl;
}

inline
void
IncrementRxPacketCounters(
    _Inout_ RxPacketCounters & Counters,
    _In_ AddressType DestinationType,
    _In_ ULONG FrameSize
)
{
    switch (DestinationType)
    {
        case AddressType::Unicast:
            Counters.Unicast.Packets += 1;
            Counters.Unicast.Bytes += FrameSize;

            break;

        case AddressType::Multicast:
            Counters.Multicast.Packets += 1;
            Counters.Multicast.Bytes += FrameSize;

            break;

        case AddressType::Broadcast:

            Counters.Broadcast.Packets += 1;
            Counters.Broadcast.Bytes += FrameSize;

            break;
    }
}

static
void
TransferMetadataToNbl(
    _In_ const RxExtensions & Extensions,
    _Inout_ NET_BUFFER_LIST * Nbl,
    _In_ const NET_PACKET_LAYOUT & Layout,
    _In_ UINT32 PacketIndex
)
{
    if (Extensions.Extension.Checksum.Enabled)
    {
        const NET_PACKET_CHECKSUM *checksum = NetExtensionGetPacketChecksum(&Extensions.Extension.Checksum, PacketIndex);
        RxMetadataTranslator::TranslatePacketChecksum(*Nbl, Layout, checksum);
    }

    if (Extensions.Extension.Rsc.Enabled)
    {
        const NET_PACKET_RSC *rsc = NetExtensionGetPacketRsc(&Extensions.Extension.Rsc, PacketIndex);
        RxMetadataTranslator::TranslatePacketRsc(*Nbl, Layout, rsc);
    }

    if (Extensions.Extension.RscTimestamp.Enabled)
    {
        const NET_PACKET_RSC_TIMESTAMP *rscTimestamp = NetExtensionGetPacketRscTimestamp(&Extensions.Extension.RscTimestamp, PacketIndex);
        RxMetadataTranslator::TranslatePacketRscTimestamp(*Nbl, Layout, rscTimestamp);
    }

    if (Extensions.Extension.Ieee8021q.Enabled)
    {
        const NET_PACKET_IEEE8021Q *ieee8021q = NetExtensionGetPacketIeee8021Q(&Extensions.Extension.Ieee8021q, PacketIndex);
        RxMetadataTranslator::TranslatePacketIeee8021q(*Nbl, ieee8021q);
    }

    if (Extensions.Extension.Hash.Enabled)
    {
        const NET_PACKET_HASH *packetHash = NetExtensionGetPacketHash(&Extensions.Extension.Hash, PacketIndex);
        RxMetadataTranslator::TranslatePacketHash(*Nbl, packetHash);
    }
}

bool
NxRxXlat::TransferDataBufferFromNetPacketToNbl(
    _Inout_ RxPacketCounters & Counters,
    _In_ NET_PACKET * Packet,
    _In_ PNET_BUFFER_LIST Nbl,
    _In_ UINT32 PacketIndex)
{
    

    // 1. check if any skipped fragment entries before this packet's fragments and reclaim them back to pools
    ReclaimSkippedFragments(Packet->FragmentIndex);

    NET_RING *fr = NetRingCollectionGetFragmentRing(&m_rings);

    // 2. Initialize first fragment MDL
    const NET_FRAGMENT *firstFragment = NetRingGetFragmentAtIndex(fr, Packet->FragmentIndex);
    MDL *firstMdl = GetCorrespondingMdl(Packet->FragmentIndex);

    const bool isFirstFragmentValid = ValidateFragmentAndInitializeMdl(
        Packet->FragmentIndex,
        firstFragment,
        firstMdl,
        true);

    bool shouldIndicate = isFirstFragmentValid;
    ULONG frameSize = static_cast<ULONG>(firstFragment->ValidLength);
    MDL *currentMdl = firstMdl;

    // 3. Initialize packet's additonal fragment MDLs
    for (UINT32 i = 1; i < Packet->FragmentCount; i++)
    {
        const UINT32 fragmentIndex = NetRingAdvanceIndex(fr, Packet->FragmentIndex, i);
        const NET_FRAGMENT *nthFragment= NetRingGetFragmentAtIndex(fr, fragmentIndex);
        MDL *nthMdl = GetCorrespondingMdl(fragmentIndex);

        const bool isNthFragmentValid = ValidateFragmentAndInitializeMdl(
            fragmentIndex,
            nthFragment,
            nthMdl,
            false);

        shouldIndicate = shouldIndicate && isNthFragmentValid;

        frameSize += static_cast<ULONG>(nthFragment->ValidLength);

        currentMdl->Next = nthMdl;
        currentMdl = currentMdl->Next;
    }

    // 4. If this is a valid packet, initialize NBL metadata and update counters
    if (shouldIndicate)
    {
        const unsigned char *payload = static_cast<unsigned char*>(
            MmGetSystemAddressForMdlSafe(firstMdl, LowPagePriority | MdlMappingNoExecute)) +
            firstFragment->Offset;

        const AddressType destinationType = NxInitializePacketLayout(
            m_adapterProperties.MediaType,
            payload,
            static_cast<size_t>(firstFragment->ValidLength),
            Packet);

        RxMetadataTranslator::TranslatePacketLayout(
            *Nbl,
            Packet->Layout,
            payload,
            static_cast<size_t>(firstFragment->ValidLength));

        TransferMetadataToNbl(
            m_extensions,
            Nbl,
            Packet->Layout,
            PacketIndex);

        IncrementRxPacketCounters(Counters, destinationType, frameSize);
    }

    // ensure the NBL chain is broken
    Nbl->Next = nullptr;
    // store which queue this NB comes from
    GetRxContextFromNbl(Nbl)->Queue = this;

    NET_BUFFER* nb = NET_BUFFER_LIST_FIRST_NB(Nbl);
    GetRxContextFromNb(nb)->HasMultipleFragments = (Packet->FragmentCount > 1);

    // Set up NET_BUFFER metadata
    NET_BUFFER_FIRST_MDL(nb) = firstMdl;
    NET_BUFFER_CURRENT_MDL(nb) = firstMdl;
    NET_BUFFER_DATA_LENGTH(nb) = frameSize;
    NET_BUFFER_DATA_OFFSET(nb) = firstFragment->Offset;
    NET_BUFFER_CURRENT_MDL_OFFSET(nb) = firstFragment->Offset;

    fr->OSReserved0 = NetRingAdvanceIndex(fr, fr->OSReserved0, Packet->FragmentCount);

    return shouldIndicate;
}


static bool IsSubrangeValid(
    PMDL Mdl,
    const void* TargetVa,
    const UINT64 TargetLength)
{
    const char* targetVa = static_cast<const char*>(TargetVa);
    const char* sourceVa = static_cast<const char*>(MmGetMdlVirtualAddress(Mdl));
    const ULONG sourceLength = MmGetMdlByteCount(Mdl);
    const char* endVa = sourceVa + sourceLength;

    const bool wrongOffset =
        (targetVa < sourceVa) ||
        (targetVa >= sourceVa + sourceLength) ||
        (targetVa + TargetLength < targetVa) ||
        (targetVa + TargetLength > endVa);

    return !wrongOffset;
}

bool
NxRxXlat::ValidateFragmentAndInitializeMdl(
    _In_ UINT32 FragmentIndex,
    _In_ const NET_FRAGMENT *Fragment,
    _In_ MDL *Mdl,
    _In_ bool isFirstFragment
)
{
    bool isFragmentValid = true;

    if ((Fragment->Capacity > m_rxDataBufferSize) ||
        (Fragment->Offset + Fragment->ValidLength > Fragment->Capacity) ||
        (!isFirstFragment && Fragment->Offset != 0))
    {
        // the data buffer size must conform to the rx capability declared by the NIC,
        // and its content must be within capacity
        // and only the first fragment can have an offset.
        //
        // mark this fragment as invalid if these condtions are broken,
        // and the entire packet will be dropped
        //
        isFragmentValid = false;
    }

    //
    // for data buffer allocated by the translator, all MDLs from the MDL pool have been
    // pre-built, i.e. MDLs have already been welded together with the data buffer during
    // initalization, no-op.
    //
    // for other cases, we need to intialize MDL on the fly here for every data buffer
    //

    if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER_V2)
    {
        // Convert the memory buffer handle the drive has given to us to MDL
        const NET_FRAGMENT_NET_MEMORY* fragmentDataBuffer =
            NetExtensionGetFragmentNetMemory(
                &m_extensions.Extension.NetMemoryId,
                FragmentIndex);

        NET_MEMORY_MDL_WRAPPER* nxMemoryMdl = m_adapterDispatch->GetMdlForDriverAllocatedMemory(
            m_adapter,
            reinterpret_cast<ULONG64>(fragmentDataBuffer->NetMemoryId));

        // now build a partial MDL out of the pre-mapped MDL from the driver buffer.
        // This is needed so the driver can reuse the original buffer for other fragments.
        MDL_EX* mdlEx = CONTAINING_RECORD(Mdl, MDL_EX, Mdl);

        const NET_FRAGMENT_RETURN_CONTEXT *ReturnContext = NetExtensionGetFragmentReturnContext(
            &m_extensions.Extension.ReturnContext, FragmentIndex);

        const NET_FRAGMENT_VIRTUAL_ADDRESS* VirtualAddress = NetExtensionGetFragmentVirtualAddress(
                &m_extensions.Extension.VirtualAddress, FragmentIndex);

        const bool subrangeIsValid = (nxMemoryMdl != nullptr) &&
            IsSubrangeValid(
                &nxMemoryMdl->Mdl,
                VirtualAddress->VirtualAddress,
                Fragment->Capacity);

        // if the memory is wrong, but we're not verifying the driver, we drop the fragment
        // if the driver is verifying, we still attempt to build the MDL and let it bugcheck        
        isFragmentValid = isFragmentValid && (subrangeIsValid || m_adapterProperties.DriverIsVerifying);

        if (isFragmentValid)
        {
            IoBuildPartialMdl(
                &nxMemoryMdl->Mdl,
                &mdlEx->Mdl,
                VirtualAddress->VirtualAddress,
                static_cast<ULONG>(Fragment->Capacity)
            );

            NT_ASSERT(Mdl->Next == nullptr);
        }

        mdlEx->MdlContext.RxBufferReturnContext = ReturnContext->Handle;
        mdlEx->NetMemoryMdl = nxMemoryMdl;
        nxMemoryMdl->OsOwnedReferenceCount++;
    }
#if !NETCX_REDIRECTOR    
    else if (!m_MdlPool.IsPreBuilt()) // i.e m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER, which only exists in KMDF
    {
        MDL_EX* mdlEx = CONTAINING_RECORD(Mdl, MDL_EX, Mdl);

        const NET_FRAGMENT_VIRTUAL_ADDRESS *VirtualAddress = NetExtensionGetFragmentVirtualAddress(
            &m_extensions.Extension.VirtualAddress, FragmentIndex);
        const NET_FRAGMENT_RETURN_CONTEXT *ReturnContext = NetExtensionGetFragmentReturnContext(
            &m_extensions.Extension.ReturnContext, FragmentIndex);

        if (isFragmentValid)
        {
            MmInitializeMdl(Mdl, VirtualAddress->VirtualAddress, Fragment->Capacity);
            MmBuildMdlForNonPagedPool(Mdl);

            NT_ASSERT(Mdl->Next == nullptr);
        }

        // for all fragments, including those marked as invalid,
        // save return context to MDL_EX's context field, so they
        // can be given back to the NIC
        mdlEx->MdlContext.RxBufferReturnContext = ReturnContext->Handle;
    }
#endif

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

    if (m_rxBufferAllocationMode == NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER_V2)
    {
        while (currMdl)
        {
            if (!GetRxContextFromNb(nb)->HasMultipleFragments)
            {
                NT_ASSERT(currMdl->Next == nullptr);
            }

            PMDL nextMdl = currMdl->Next;
            currMdl->Next = nullptr;

            if (m_chainOfMdlsToReturn == nullptr)
            {
                m_chainOfMdlsToReturn = m_lastMdlToReturn = currMdl;
            }
            else
            {
                m_lastMdlToReturn->Next = currMdl;
                m_lastMdlToReturn = currMdl;
            }

            currMdl = nextMdl;
        }
    }
    else
    {
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

void
NxRxXlat::UpdateMaxPacketsFromKnobs(
    KIRQL Irql
)
{
    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties;
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);
    m_maxPackets =
        Irql == PASSIVE_LEVEL ?
        adapterProperties.ExecutionContextKnobs->MaxPacketsReceive.AtPassive :
        adapterProperties.ExecutionContextKnobs->MaxPacketsReceive.AtDispatch;
}
