/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxRxXlat.cpp

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the 
    receive path.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxRxXlat.tmh"
#include "NxRxXlat.hpp"

#include "NxPacketLayout.hpp"
#include "NxChecksumInfo.hpp"

#define RX_NUM_FRAGMENTS 1
ULONG const MAX_DYNAMIC_PAGES = 16;
ULONG const MAX_DYNAMIC_PACKET_SIZE = (MAX_DYNAMIC_PAGES - 1) * PAGE_SIZE + 1;

unique_nbl
NxRxXlat::AllocateOneNbl()
{
    auto nbl = unique_nbl(
        NdisAllocateNetBufferAndNetBufferList(
            m_netBufferListPool.get(), 0, 0, nullptr, 0, 0));
    if (!nbl)
        return nullptr;

    nbl->SourceHandle = m_adapterHandle;

    new (&nbl->MiniportReserved[0]) NblContext();

    return nbl;
}

NxRxXlat::PacketContext &
NxRxXlat::GetPacketContext(NET_PACKET & packet)
{
    auto & packetContext = *reinterpret_cast<NxRxXlat::PacketContext*>(m_ringBuffer.GetAppContext(&packet));
    packetContext.ASSERT_VALID();
    return packetContext;
}

NxRxXlat::NblContext &
NxRxXlat::GetNblContext(NET_BUFFER_LIST & nbl)
{
    auto & nblContext = *reinterpret_cast<NxRxXlat::NblContext*>(&nbl.MiniportReserved[0]);
    nblContext.ASSERT_VALID();
    return nblContext;
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

void
NxRxXlat::ArmNotifications(ArmedNotifications notifications)
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
    m_adapterQueue->SetArmed(true);
}

void
NxRxXlat::ReinitializePacket(NET_PACKET & netPacket)
{
    NetPacketReuseOne(&netPacket);

    // only 1 fragment per packet on the Rx path
    static_assert(RX_NUM_FRAGMENTS == 1, "This code path needs to be updated if RX_NUM_FRAGMENTS is increased");

    auto & fragment = netPacket.Data;

    for (auto f = &fragment; f; f = NET_PACKET_FRAGMENT_GET_NEXT(f))
        f->LastPacketOfChain = TRUE;
}

void
NxRxXlat::PrepareBuffersForNetAdapter()
{
    m_returnedPackets = 0;

    while(auto pCurrentPacket = m_ringBuffer.GetNextPacketToGiveToNic())
    {
        auto & packetContext = GetPacketContext(*pCurrentPacket);
        auto & nblContext = GetNblContext(*packetContext.AssociatedNetBufferList);

        // Give packets to the NetAdapter until we find one that is still busy in NDIS.
        if (!InterlockedExchange(&nblContext.Returned, false))
        {
            break;
        }

        ReinitializePacket(*pCurrentPacket);

        ++m_returnedPackets;
        --m_outstandingPackets;

        m_ringBuffer.GiveNextPacketToNic();
    }
}

void
NxRxXlat::YieldToNetAdapter()
{
    m_adapterQueue->Advance();
}

void
NxRxXlat::IndicateNblsToNdis()
{
    m_postedPackets = 0;

    NBL_QUEUE nblQueue;
    ndisInitializeNblQueue(&nblQueue);

    ULONG queuedPackets = 0;
    while (auto completed = m_ringBuffer.TakeNextPacketFromNic())
    {
        auto & packetContext = GetPacketContext(*completed);
        auto & nbl = packetContext.AssociatedNetBufferList;
        auto & nblContext = GetNblContext(*nbl);



        completed->Layout = NxGetPacketLayout(m_mediaType, *completed);
        nbl->NetBufferListInfo[TcpIpChecksumNetBufferListInfo] = NxTranslateRxPacketChecksum(*completed).Value;

        if (completed->Layout.Layer2Type == NET_PACKET_LAYER2_TYPE_ETHERNET)
        {
            NT_ASSERT(completed->Layout.Layer2HeaderLength >= sizeof(KEthernetHeader));

            NxPacketHeaderParser parser(*completed);
            auto const ethernetHeader = parser.TryTakeHeader<KEthernetHeader>();

            NT_ASSERT(ethernetHeader);

            nbl->NetBufferListInfo[NetBufferListFrameType] = (PVOID)ethernetHeader->TypeLength.get_NetworkOrder();
        }
        else
        {
            nbl->NetBufferListInfo[NetBufferListFrameType] = nullptr;
        }

        // ensure the NBL chain is broken
        nbl->Next = nullptr;

        // if the dynamic packet is too large, it must be skipped
        // The pre-allocated MDL was not large enough for it.
        if (packetContext.MemoryPreallocated || completed->Data.Capacity <= MAX_DYNAMIC_PACKET_SIZE)
        {
            if (!packetContext.MemoryPreallocated)
            {
                MmInitializeMdl(&packetContext.Mdl, completed->Data.VirtualAddress, completed->Data.Capacity);
                MmBuildMdlForNonPagedPool(&packetContext.Mdl);
            }

            nbl->FirstNetBuffer->DataOffset =
                nbl->FirstNetBuffer->CurrentMdlOffset = completed->Data.Offset;
            nbl->FirstNetBuffer->DataLength = (ULONG)completed->Data.ValidLength;

            // Copy whatever the WDF client driver set in its thread specific packet extension
            // to the corresponding NBL fields
            NET_BUFFER_LIST_INFO(nbl, MediaSpecificInformationEx) = NET_PACKET_INTERNAL_802_15_4_INFO(completed, m_thread802_15_4MSIExExtensionOffset);
            NET_BUFFER_LIST_STATUS(nbl) = NET_PACKET_INTERNAL_802_15_4_STATUS(completed, m_thread802_15_4StatusOffset);

            ndisAppendNblChainToNblQueueFast(&nblQueue, nbl.get(), nbl.get());
            ++queuedPackets;
        }
        else
        {
            nblContext.Returned = true;
        }

        ++m_postedPackets;
        ++m_outstandingPackets;
    }

    if (m_postedPackets == 0)
        return;

    bool indicationAccepted = m_nblDispatcher->IndicateReceiveNetBufferLists(
            nblQueue.First,
            NDIS_DEFAULT_PORT_NUMBER,
            queuedPackets,
            0);

    if (!indicationAccepted)
    {
        // While stopping the queue, the NBL packet gate may close while this thread
        // is still trying to indicate a receive.
        //
        // If that happens, we're in the process of tearing down this queue, so just
        // mark the NBLs as returned and bail out.

        WIN_ASSERT(m_stopRequested);

        for (auto & nbl : nblQueue.First)
        {
            auto & nblContext = GetNblContext(nbl);

            InterlockedExchange(&nblContext.Returned, true);
        }
    }
}

void
NxRxXlat::WaitForMoreWork()
{
    auto notificationsToArm = GetNotificationsToArm();

    // In order to handle race conditions, the notifications that should
    // be armed at halt cannot change between the halt preparation and the
    // actual halt. If they do change, re-arm the necessary notifications
    // and loop again.
    if (notificationsToArm.Value != 0 && notificationsToArm.Value == m_lastArmedNotifications.Value)
    {
        m_executionContext.WaitForSignal();

        // after halting, don't arm any notifications
        notificationsToArm.Value = 0;
    }

    ArmNotifications(notificationsToArm);

    m_lastArmedNotifications = notificationsToArm;
}

void
NxRxXlat::ReceiveThread()
{
    m_executionContext.WaitForStart();

    auto cancelIssued = false;

    while (true)
    {
        if (!m_stopRequested)
            PrepareBuffersForNetAdapter();

        YieldToNetAdapter();

        IndicateNblsToNdis();

        WaitForMoreWork();

        // This represents the wind down of Rx
        if (m_stopRequested)
        {
            if (!cancelIssued)
            {
                // Indicate cancellation to the adapter
                // and drop all outstanding NBLs.
                //
                // One NBL may remain that has been partially programmed into the NIC.
                // So that NBL is kept around until the end.

                m_adapterQueue->Cancel();

                cancelIssued = true;
            }

            // The termination condition is that all packets have been returned from the NIC.
            if (m_executionContext.IsStopRequested() &&
                m_ringBuffer.GetNumPacketsOwnedByNic() == 0)
            {
                break;
            }
        }
    }
}

NTSTATUS
NxRxXlat::AllocateNetBufferLists()
{
    for (auto i = 0u; i < m_ringBuffer.Count(); i++)
    {
        auto & packet = *m_ringBuffer.GetPacketAtIndex(i);
        auto & packetContext = GetPacketContext(packet);

        packetContext.AssociatedNetBufferList = AllocateOneNbl();
        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !packetContext.AssociatedNetBufferList);

        auto & nbl = packetContext.AssociatedNetBufferList;

        nbl->FirstNetBuffer->MdlChain =
            nbl->FirstNetBuffer->CurrentMdl = &packetContext.Mdl;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NxRxXlat::PreallocateRxBuffers()
{
    auto const perPacketSize = m_adapterQueue->GetAllocationSize();
    if (perPacketSize == 0)
    {
        return STATUS_SUCCESS;
    }

    auto const alignmentRequirement = m_adapterQueue->GetAlignmentRequirement();
    NT_ASSERT(RTL_IS_POWER_OF_TWO(alignmentRequirement + 1));

    auto const numPackets = m_ringBuffer.Count();
    auto const alignedPacketSize = ALIGN_UP_BY(perPacketSize, alignmentRequirement + 1);

    // Try to allocate 1 big allocation. If that fails, continue to split the size of the
    // requested allocations in half until the allocation requests can be satisfied.
    //
    // This won't allocate less memory, but it will split the allocation up so that if room
    // for the initial allocation can't be found, we can get a bunch of smaller allocations
    // instead which are easier to find room for.

    Rtl::KArray<wistd::unique_ptr<INxQueueAllocation>> allocations;
    size_t numAllocations = 1;
    while (numAllocations != allocations.count() && numPackets >= numAllocations)
    {
        // numPackets and numAllocations should both always be powers of 2
        NT_ASSERT(numPackets % numAllocations == 0);

        // The allocations tracking array must be expanded in order for allocation to proceed.
        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            !allocations.reserve(numAllocations));
        
        auto const numPacketsPerAllocation = numPackets / numAllocations;
        size_t allocationSize;
        if (NT_SUCCESS(RtlSizeTMult(alignedPacketSize, numPacketsPerAllocation, &allocationSize)))
        {
            auto allSucceeded = true;
            for (size_t i = 0; i < numAllocations; i++)
            {
                auto allocation = wistd::unique_ptr<INxQueueAllocation>(m_adapterQueue->AllocatePacketBuffer(allocationSize));
                if (!allocation)
                {
                    allSucceeded = false;
                    break;
                }

#if DBG
                auto const va = allocation->GetBuffer();
                NT_ASSERT(ALIGN_DOWN_POINTER_BY(va, alignmentRequirement + 1) == va);
#endif

                // memory has already been pre-allocated - there's
                // no reason this append call should fail.
                NT_VERIFY(allocations.append(wistd::move(allocation)));
            }

            if (allSucceeded) continue;
        }

        // shrinking the KArray invokes no error paths
        NT_VERIFY(allocations.resize(0));
        numAllocations = (numAllocations << 1);
    }

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        numAllocations != allocations.count());

    m_allocations = wistd::move(allocations);

    auto const numPacketsPerAllocation = numPackets / numAllocations;
    for (auto i = 0ul; i < numPackets; i++)
    {
        auto const & allocation = m_allocations[i / numPacketsPerAllocation];

        auto & packet = *m_ringBuffer.GetPacketAtIndex(i);
        auto & packetContext = GetPacketContext(packet);

        auto const buffer = ALIGN_UP_POINTER_BY(allocation->GetBuffer(), alignmentRequirement + 1);
        void * const va = (PVOID)((ULONG_PTR)buffer + i * alignedPacketSize);

        NT_ASSERT(ALIGN_DOWN_POINTER_BY(va, alignmentRequirement + 1) == va);

        packet.Data.VirtualAddress = va;
        packet.Data.Capacity = alignedPacketSize;

        MmInitializeMdl(&packetContext.Mdl, va, alignedPacketSize);
        MmBuildMdlForNonPagedPool(&packetContext.Mdl);

        if (allocation->GetAllocationType() == NxAllocationType::DmaCommonBuffer)
        {
            packet.Data.DmaLogicalAddress = allocation->GetLogicalAddress();
            packet.Data.DmaLogicalAddress.QuadPart += i * alignedPacketSize;
        }

        packetContext.MemoryPreallocated = true;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NxRxXlat::InitializeRingBuffer()
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AllocateNetBufferLists(),
        "Failed to allocate a net buffer list for each packet. NxRxXlat=%p", this);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PreallocateRxBuffers(),
        "Failed to allocate enough memory for all packets in the ring buffer. NxRxXlat=%p", this);

    return STATUS_SUCCESS;
}

unique_nbl_pool
NxRxXlat::CreateNblPool()
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

    return unique_nbl_pool(NdisAllocateNetBufferListPool(m_adapterHandle, &poolParameters));
}

NTSTATUS
NxRxXlat::Initialize(
    _In_ HNETPACKETCLIENT packetClient,
    _In_ NXQUEUE_CREATION_PARAMETERS const *params,
    _Out_ NXQUEUE_CREATION_RESULT *result)
{
    m_thread802_15_4MSIExExtensionOffset = NetPacketGetExtensionOffset(packetClient, THREAD_802_15_4_MSIEx_PACKET_EXTENSION_NAME);
    m_thread802_15_4StatusOffset = NetPacketGetExtensionOffset(packetClient, THREAD_802_15_4_STATUS_PACKET_EXTENSION_NAME);

    m_netBufferListPool = CreateNblPool();
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_netBufferListPool);

    ULONG const maxPages =
        (params->AllocationSize == 0) ?
            MAX_DYNAMIC_PAGES :
            ADDRESS_AND_SIZE_TO_SPAN_PAGES(PAGE_SIZE - 1, ALIGN_UP_BY(params->AllocationSize, params->AlignmentRequirement + 1));
    ULONG const packetExtensionSize = FIELD_OFFSET(PacketContext, _PfnNumbers) + sizeof(PFN_NUMBER) * maxPages;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_ringBuffer.Initialize(
            packetClient,
            params->RingBufferSize,
            RX_NUM_FRAGMENTS,
            packetExtensionSize,
            params->ClientContextSize),
        "Failed to allocate ring buffer. NxRxXlat=%p", this);

    for (auto i = 0ul; i < m_ringBuffer.Count(); i++)
    {
        new (m_ringBuffer.GetAppContext(m_ringBuffer.GetPacketAtIndex(i))) PacketContext();
    }

    m_ringBuffer.Get()->OSReserved2[1] = params->OSReserved2_1;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_executionContext.Initialize(this, [](PVOID context) -> EC_RETURN {
            static_cast<NxRxXlat *>(context)->ReceiveThread();
            return EC_RETURN();
        }),
        "Failed to start Rx execution context. NxRxXlat=%p", this);

    result->ContextOffset = m_ringBuffer.GetClientContextOffset();
    result->RingBuffer = m_ringBuffer.Get();
    result->Thread802_15_4MSIExExtensionOffset = m_thread802_15_4MSIExExtensionOffset;
    result->Thread802_15_4StatusOffset = m_thread802_15_4StatusOffset;

    return STATUS_SUCCESS;
}

void
NxRxXlat::Notify()
{
    m_executionContext.Signal();
}

NTSTATUS 
NxRxXlat::StartQueue(_In_ INxAdapter *adapter)
{
    m_mediaType = adapter->GetMediaType();

    INxAdapterQueue* adapterQueue;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        adapter->OpenQueue(this, NxQueueType::Rx, &adapterQueue),
        "Failed to open an RxQueue. NxRxXlat=%p", this);

    m_adapterQueue.reset(adapterQueue);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        InitializeRingBuffer(),
        "Failed to allocate memory for the Rx ring buffer.");

    m_nblDispatcher->SetRxHandler(this);

    m_executionContext.Start();

    return STATUS_SUCCESS;
}

NxRxXlat::~NxRxXlat()
{
    // Give a soft hint to the RX thread that it stop its NBL indications to NDIS, and stop
    // posting new buffers to the netadapter.
    WIN_ASSERT(m_stopRequested == false);
    m_stopRequested = true;

    // Close a hard gate that prevent new NBLs from being indicated to NDIS, then wait
    // for all NBLs to be returned.
    m_nblDispatcher->SetRxHandler(nullptr);

    // Signal the RX thread that it can exit, and wait for it to exit.
    m_executionContext.Stop();

    for (auto i = 0ul; i < m_ringBuffer.Count(); i++)
    {
        auto & packet = *m_ringBuffer.GetPacketAtIndex(i);
        auto & packetContext = GetPacketContext(packet);

        {
            auto & nbl = packetContext.AssociatedNetBufferList;
            auto & nblContext = GetNblContext(*nbl);

            nblContext.~NblContext();
        }

        packetContext.~PacketContext();
    }
}

void 
NxRxXlat::ReturnNetBufferLists(
    _In_ NET_BUFFER_LIST *nblChain,
    _In_ ULONG numberOfNbls,
    _In_ ULONG receiveCompleteFlags)
{
    UNREFERENCED_PARAMETER((numberOfNbls, receiveCompleteFlags));

    for (auto & nbl : nblChain)
    {
        auto & nblContext = GetNblContext(nbl);

        InterlockedExchange(&nblContext.Returned, true);
    }

    if (m_returnedNblNotification.TestAndClear())
    {
        m_executionContext.Signal();
    }
}
