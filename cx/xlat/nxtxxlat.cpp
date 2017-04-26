/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxTxXlat.cpp

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the 
    transmit path.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxTxXlat.tmh"

#include "NxTxXlat.hpp"
#include "NxPacketLayout.hpp"
#include "NxChecksumInfo.hpp"

#define TX_NUM_FRAGMENTS 20

NxTxXlat::~NxTxXlat()
{
    m_nblDispatcher->SetTxHandler(nullptr);
    m_executionContext.Stop();

    if (m_ringBuffer.Get())
    {
        for (auto i = 0ul; i < m_ringBuffer.Count(); i++)
        {
            auto & packet = *m_ringBuffer.GetPacketAtIndex(i);
            auto & context = GetPacketContext(packet);

            context.~PacketContext();
        }
    }
}

void
NxTxXlat::ArmNetBufferListArrivalNotification()
{
    m_queueNotification.Set();
}

void
NxTxXlat::ArmAdapterTxNotification()
{
    m_adapterQueue->SetArmed(true);
}

NxTxXlat::PacketContext &
NxTxXlat::GetPacketContext(NET_PACKET &netPacket)
{
    auto & context = *reinterpret_cast<NxTxXlat::PacketContext*>(m_ringBuffer.GetAppContext(&netPacket));
    context.ASSERT_VALID();
    return context;
}

NxTxXlat::ArmedNotifications
NxTxXlat::GetNotificationsToArm()
{
    ArmedNotifications notifications;

    // Shouldn't arm any notifications if we don't want to halt
    if (m_producedPackets == 0 && m_completedPackets == 0)
    {
        // If 0 packets were produced and there are no NBLs available from the
        // serialization queue, then arm the serialization queue for notification.
        notifications.Flags.ShouldArmNblArrival = m_netBufferQueueEmpty;

        // If 0 packets were completed by the Adapter in the last iteration, then arm
        // the adapter to issue Tx completion notifications.
        notifications.Flags.ShouldArmTxCompletion = m_outstandingPackets != 0;

        // At least one notification should be set whenever going through this path.
        //
        // (m_producedPackets == 0, m_completedPackets == 0, m_netBufferQueueEmpty, m_outstandingPackets != 0)
        // is a 4-tuple that combines to a 3 tuple of (ShouldHalt,ShouldArmNblArrival,ShouldArmTxCompletion).
        // 
        // Whenever ShouldHalt is true, ShouldArmNblArrival or ShouldArmTxCompletion
        // must be set. There is a one case of the above tuple (1,1,0,0) that would cause this to
        // be false. This implies no work was done, but there were packets in the producer window
        // (m_oustandingPackets == 0 implies all of them are), and there are packets available in the NBL
        // serialization queue. In this case that means there is a malfunction in the producer routine if
        // the code gets stuck in this way.
        NT_ASSERT(notifications.Value);
    }

    return notifications;
}

void
NxTxXlat::ArmNotifications(ArmedNotifications notifications)
{
    if (notifications.Flags.ShouldArmNblArrival)
    {
        ArmNetBufferListArrivalNotification();
    }

    if (notifications.Flags.ShouldArmTxCompletion)
    {
        ArmAdapterTxNotification();
    }
}

NxTxXlat::TranslationResult
NxTxXlat::TranslateNetBufferToNetPacket( //mostly copied from RtlCopyMdltobuffer
    _In_    NET_BUFFER      &netBuffer,
    _Inout_ NET_PACKET      &netPacket)
{
    NetPacketReuseOne(&netPacket);

    auto pCurrentFragment = &netPacket.Data;

    PMDL mdl = NET_BUFFER_CURRENT_MDL(&netBuffer);
    size_t mdlOffset = NET_BUFFER_CURRENT_MDL_OFFSET(&netBuffer);
    auto bytesToCopy = NET_BUFFER_DATA_LENGTH(&netBuffer);

    if (bytesToCopy == 0)
    {
        netPacket.Data.LastFragmentOfFrame = 1;
        netPacket.Data.LastPacketOfChain = 1;
        return TranslationResult::Success;
    }

    //
    // While there is remaining data to be copied, and we have MDLs to walk...
    //

    for (size_t remain = bytesToCopy; mdl && (remain > 0); mdl = mdl->Next)
    {

        if (!pCurrentFragment)
        {
            return TranslationResult::InsufficientFragments;
        }

        //
        // Skip zero length MDLs.
        //

        size_t const mdlByteCount = MmGetMdlByteCount(mdl);
        if (mdlByteCount == 0)
        {
            continue;
        }

        //
        // Compute the amount to transfer this time.
        //

        ASSERT(mdlOffset < mdlByteCount);
        size_t const copySize = min(remain, mdlByteCount - mdlOffset);

        //
        // Perform the transfer
        //
        pCurrentFragment->Offset = mdlOffset;
        pCurrentFragment->VirtualAddress = MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute);
        pCurrentFragment->DmaLogicalAddress.QuadPart = reinterpret_cast<ULONG_PTR>(mdl);

        pCurrentFragment->Capacity = mdlByteCount;
        pCurrentFragment->ValidLength = copySize;

        mdlOffset = 0;
        remain -= copySize;

        if (remain == 0)
        {
            pCurrentFragment->LastFragmentOfFrame = 1;
        }

        // LastPacketOfChain should be set in every fragment of the last
        // packet. Right now we always have one frame per NET_PACKET, so
        // we can get away with just setting the flag in every fragment up
        // until the first LastFragmentOfFrame.
        pCurrentFragment->LastPacketOfChain = 1;

        pCurrentFragment = NET_PACKET_FRAGMENT_GET_NEXT(pCurrentFragment);
    }

    return TranslationResult::Success;
}

void
NxTxXlat::TransmitThread()
{
    m_executionContext.WaitForStart();

    auto cancelIssued = false;

    // This represents the core execution of the Tx path
    while (true)
    {
        // Check if the NBL serialization has any data
        PollNetBufferLists();

        // Post NBLs to the producer side of the NBL
        TranslateNbls();

        // Allow the NetAdapter to return any packets that it is done with.
        YieldToNetAdapter();

        // Drain any packets that the NIC has completed.
        // This means returning the associated NBLs for each completed
        // NET_PACKET.
        DrainCompletions();

        // Arms notifications if no forward progress was made in
        // this loop.
        WaitForMoreWork();

        // This represents the wind down of Tx
        if (m_executionContext.IsStopRequested())
        {
            if (!cancelIssued)
            {
                // Indicate cancellation to the adapter
                // and drop all outstanding NBLs.
                //
                // One NBL may remain that has been partially programmed into the NIC.
                // So that NBL is kept around until the end

                m_adapterQueue->Cancel();
                DropQueuedNetBufferLists();

                cancelIssued = true;
            }

            // The termination condition is that the NIC has returned all packets.
            if (m_ringBuffer.GetNumPacketsOwnedByNic() == 0)
            {
                break;
            }
        }
    }

    if (m_currentNbl)
    {
        m_currentNbl->Status = NDIS_STATUS_PAUSED;

        m_nblDispatcher->SendNetBufferListsComplete(m_currentNbl, 1, 0);
    }
}

void
NxTxXlat::DrainCompletions()
{
    m_completedPackets = 0;

    ULONG numCompletedNbls = 0;
    PNET_BUFFER_LIST pHead = nullptr;

    while (auto pCompletedPacket = m_ringBuffer.TakeNextPacketFromNic())
    {
        auto & completedPacketExtension = GetPacketContext(*pCompletedPacket);

        if (completedPacketExtension.NetBufferListToComplete)
        {
            completedPacketExtension.NetBufferListToComplete->Next = pHead;
            pHead = completedPacketExtension.NetBufferListToComplete;

            numCompletedNbls += 1;

            completedPacketExtension.NetBufferListToComplete = nullptr;
        }

        --m_outstandingPackets;
        ++m_completedPackets;
    }

    if (pHead)
    {
        m_nblDispatcher->SendNetBufferListsComplete(pHead, numCompletedNbls, 0);
    }
}

PNET_BUFFER
GetCurrentNetBuffer(NET_BUFFER_LIST & netBufferList)
{
    if (netBufferList.Scratch)
    {
        return static_cast<PNET_BUFFER>(netBufferList.Scratch);
    }
    else
    {
        return NET_BUFFER_LIST_FIRST_NB(&netBufferList);
    }
}

void
SetCurrentNetBuffer(NET_BUFFER_LIST & netBufferList, _In_opt_ PNET_BUFFER pNetBuffer)
{
    netBufferList.Scratch = pNetBuffer;
}

void
NxTxXlat::ReinitializePacket(_In_ NET_PACKET & netPacket)
{
    NetPacketReuseOne(&netPacket);

    // In the Tx path the value of LastFragmentOfFrame and LastPacketOfChain depends on the NET_BUFFER
    // being translated, clear them in all the fragments
    for (auto fragment = &netPacket.Data;fragment;fragment = NET_PACKET_FRAGMENT_GET_NEXT(fragment))
    {
        fragment->LastFragmentOfFrame = 0;
        fragment->LastPacketOfChain = 0;
    }
}

void
NxTxXlat::TranslateNbls()
{
    m_producedPackets = 0;

    PNET_BUFFER_LIST nextNbl;
    for (NOTHING; m_currentNbl; m_currentNbl = nextNbl)
    {
        nextNbl = m_currentNbl->Next;

        auto const pCurrentNetBuffer = GetCurrentNetBuffer(*m_currentNbl);
        SetCurrentNetBuffer(*m_currentNbl, nullptr);

        for (auto & nb : pCurrentNetBuffer)
        {
            auto pCurrentPacket = m_ringBuffer.GetNextPacketToGiveToNic();
            if (!pCurrentPacket)
            {
                // There are no more packets in the ring buffer that we can give to the NIC.
                //
                // Record the current net buffer in Scratch so that we can resume
                // from that net buffer when ProducePackets is next called.
                SetCurrentNetBuffer(*m_currentNbl, &nb);
                return;
            }

            ReinitializePacket(*pCurrentPacket);

            switch (TranslateNetBufferToNetPacket(nb, *pCurrentPacket))
            {
            case TranslationResult::InsufficientFragments:






                DbgBreakPoint();

                pCurrentPacket->IgnoreThisPacket = true;

                break;
            case TranslationResult::Success:
                // Copy thread information to the corresponding packet extension
                NET_PACKET_INTERNAL_802_15_4_INFO(pCurrentPacket, m_thread802_15_4MSIExExtensionOffset) = NET_BUFFER_LIST_INFO(m_currentNbl, MediaSpecificInformationEx);
                NET_PACKET_INTERNAL_802_15_4_STATUS(pCurrentPacket, m_thread802_15_4StatusOffset) = NET_BUFFER_LIST_STATUS(m_currentNbl);

                pCurrentPacket->Layout = NxGetPacketLayout(m_mediaType, *pCurrentPacket);

                auto const &checksumInfo =
                    *(PNDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO)
                    &m_currentNbl->NetBufferListInfo[TcpIpChecksumNetBufferListInfo];

#if DBG
                if (pCurrentPacket->Layout.Layer4Type == NET_PACKET_LAYER4_TYPE_TCP)
                {
                    NT_ASSERT(checksumInfo.Transmit.TcpHeaderOffset == (ULONG)(pCurrentPacket->Layout.Layer2HeaderLength + pCurrentPacket->Layout.Layer3HeaderLength));
                }
#endif

                pCurrentPacket->Checksum = NxTranslateTxPacketChecksum(*pCurrentPacket, checksumInfo);
                break;
            }

            // If this is the final NB in the NBL, then the NBL needs
            // to be completed when the associated net packet is completed.
            if (!nb.Next)
            {
                auto & currentPacketExtension = GetPacketContext(*pCurrentPacket);

                // When the associated NET_PACKET is completed, the associated NBL
                // must be completed
                currentPacketExtension.NetBufferListToComplete = m_currentNbl;

                // break the NBL chain
                m_currentNbl->Next = nullptr;
            }

            ++m_producedPackets;
            ++m_outstandingPackets;
            m_ringBuffer.GiveNextPacketToNic();
        }
    }
}

void
NxTxXlat::WaitForMoreWork()
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
NxTxXlat::DropQueuedNetBufferLists()
{
    // This routine completes both the currently dequeued chain
    // of NBLs and the queued chain of NBLs with NDIS_STATUS_PAUSED.
    // This should only be run during the run down of the Tx path.
    // When the Tx path is being run down, no more NBLs are delivered
    // to the NBL queue.

    NT_ASSERT(m_executionContext.IsStopRequested());

    if (m_currentNbl)
    {
        auto nblToComplete = m_currentNbl;

        // If the NBL is partially consumed, it
        // cannot be completed immediately
        if (GetCurrentNetBuffer(*m_currentNbl))
        {
            nblToComplete = m_currentNbl->Next;
            m_currentNbl->Next = nullptr;
        }

        if (nblToComplete)
        {
            ULONG numNblsCompleted = 0;

            for (auto & nbl : nblToComplete)
            {
                nbl.Status = NDIS_STATUS_PAUSED;
                numNblsCompleted += 1;
            }

            m_nblDispatcher->SendNetBufferListsComplete(nblToComplete, numNblsCompleted, 0);
        }
    }

    if (auto queuedNbls = DequeueNetBufferListQueue())
    {
        ULONG numNblsCompleted = 0;

        for (auto & queuedNbl : queuedNbls)
        {
            queuedNbl.Status = NDIS_STATUS_PAUSED;
            numNblsCompleted += 1;
        }

        m_nblDispatcher->SendNetBufferListsComplete(queuedNbls, numNblsCompleted, 0);
    }
}

void
NxTxXlat::PollNetBufferLists()
{
    // If the NBL chain is currently completely exhausted, go to the NBL
    // serialization queue to see if it has any new NBLs.
    //
    // m_currentNbl can be empty if either the last iteration of the translation
    // routine drained the current set of NBLs or if the last iteration
    // tried to dequeue from the serialized NBL forest and came up empty.
    //
    // Therefore sets m_netBufferQueueEmpty in the latter case to be checked when
    // determining if the translator should halt.
    if (!m_currentNbl)
    {
        m_currentNbl = DequeueNetBufferListQueue();
        if (m_currentNbl)
        {
            m_netBufferQueueEmpty = false;

            // When dequeuing the current NBL chain, reset the scratch field for
            // all NBLs. This field is needed for tracking the current NB when restarting.
            for (auto & nbl : m_currentNbl)
            {
                nbl.Scratch = nullptr;
                nbl.Status = NDIS_STATUS_SUCCESS;
            }
        }
        else
        {
            m_netBufferQueueEmpty = true;
        }
    }
}

void
NxTxXlat::SendNetBufferLists(
    _In_ NET_BUFFER_LIST *nblChain,
    _In_ ULONG portNumber,
    _In_ ULONG numberOfNbls,
    _In_ ULONG sendFlags)
{
    UNREFERENCED_PARAMETER((portNumber, numberOfNbls, sendFlags));

    m_synchronizedNblQueue.Enqueue(nblChain);

    if (m_queueNotification.TestAndClear())
    {
        m_executionContext.Signal();
    }
}

PNET_BUFFER_LIST
NxTxXlat::DequeueNetBufferListQueue()
{
    return m_synchronizedNblQueue.DequeueAll();
}

void
NxTxXlat::YieldToNetAdapter()
{
    m_adapterQueue->Advance();
}

NTSTATUS
NxTxXlat::Initialize(
    _In_ HNETPACKETCLIENT packetClient,
    _In_ NXQUEUE_CREATION_PARAMETERS const *params,
    _Out_ NXQUEUE_CREATION_RESULT *result)
{
    m_thread802_15_4MSIExExtensionOffset = NetPacketGetExtensionOffset(packetClient, THREAD_802_15_4_MSIEx_PACKET_EXTENSION_NAME);
    m_thread802_15_4StatusOffset = NetPacketGetExtensionOffset(packetClient, THREAD_802_15_4_STATUS_PACKET_EXTENSION_NAME);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_ringBuffer.Initialize(
            packetClient,
            params->RingBufferSize,
            TX_NUM_FRAGMENTS,
            sizeof(PacketContext),
            params->ClientContextSize),
        "Failed to initialize ring buffer. NxTxXlat=%p", this);

    m_ringBuffer.Get()->OSReserved2[1] = params->OSReserved2_1;

    for (auto i = 0ul; i < m_ringBuffer.Count(); i++)
    {
        new (m_ringBuffer.GetAppContext(m_ringBuffer.GetPacketAtIndex(i))) PacketContext();
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_executionContext.Initialize(this, [](PVOID context) -> EC_RETURN {
            static_cast<NxTxXlat *>(context)->TransmitThread();
            return EC_RETURN();
        }),
        "Failed to start Rx execution context. NxTxXlat=%p", this);

    result->ContextOffset = m_ringBuffer.GetClientContextOffset();
    result->RingBuffer = m_ringBuffer.Get();
    result->Thread802_15_4MSIExExtensionOffset = m_thread802_15_4MSIExExtensionOffset;
    result->Thread802_15_4StatusOffset = m_thread802_15_4StatusOffset;

    return STATUS_SUCCESS;
}

NTSTATUS 
NxTxXlat::StartQueue(_In_ INxAdapter *adapter)
{
    m_mediaType = adapter->GetMediaType();

    INxAdapterQueue* adapterQueue;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        adapter->OpenQueue(this, NxQueueType::Tx, &adapterQueue),
        "Failed to open a Tx queue. NxTxXlat=%p", this);

    m_adapterQueue.reset(adapterQueue);
    m_nblDispatcher->SetTxHandler(this);

    m_executionContext.Start();

    return STATUS_SUCCESS;
}

void
NxTxXlat::Notify()
{
    m_executionContext.Signal();
}

