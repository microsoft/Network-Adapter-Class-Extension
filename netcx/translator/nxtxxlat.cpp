// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the
    transmit path.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxTxXlat.tmh"
#include "NxTxXlat.hpp"

#include <net/checksumtypes_p.h>
#include <net/logicaladdresstypes_p.h>
#include <net/lsotypes_p.h>
#include <net/mdltypes_p.h>
#include <net/virtualaddresstypes_p.h>

#include "NxPacketLayout.hpp"
#include "NxChecksumInfo.hpp"

#ifndef _KERNEL_MODE
#define NDIS_STATUS_PAUSED ((NDIS_STATUS)STATUS_NDIS_PAUSED)
#endif // _KERNEL_MODE

static
NET_CLIENT_EXTENSION TxQueueExtensions[] = {
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
        NET_PACKET_EXTENSION_LSO_NAME,
        NET_PACKET_EXTENSION_LSO_VERSION_1,
        0,
        NET_PACKET_EXTENSION_LSO_VERSION_1_SIZE,
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
        NET_FRAGMENT_EXTENSION_MDL_NAME,
        NET_FRAGMENT_EXTENSION_MDL_VERSION_1,
        0,
        NET_FRAGMENT_EXTENSION_MDL_VERSION_1_SIZE,
        NetExtensionTypeFragment,
    },
};

static
void
NetClientQueueNotify(
    PVOID Queue
)
{
    reinterpret_cast<NxTxXlat *>(Queue)->Notify();
}

static const NET_CLIENT_QUEUE_NOTIFY_DISPATCH QueueDispatch
{
    { sizeof(NET_CLIENT_QUEUE_NOTIFY_DISPATCH) },
    &NetClientQueueNotify
};

using PacketContext = NxNblTranslator::PacketContext;

_Use_decl_annotations_
NxTxXlat::NxTxXlat(
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
    m_bounceBufferPool(
        m_rings,
        m_extensions.Extension.VirtualAddress,
        m_extensions.Extension.LogicalAddress),
    m_packetContext(m_rings, NetRingTypePacket),
    m_statistics(Statistics)
{
    m_adapterDispatch->GetProperties(m_adapter, &m_adapterProperties);
    m_adapterDispatch->GetDatapathCapabilities(m_adapter, &m_datapathCapabilities);
    m_nblDispatcher = static_cast<INxNblDispatcher *>(m_adapterProperties.NblDispatcher);
}

NxTxXlat::~NxTxXlat()
{
    // Waits until the EC completely exits
    m_executionContext.Terminate();

    if (m_packetRing.Get())
    {
        for (auto i = 0ul; i < m_packetRing.Count(); i++)
        {
            auto & context = m_packetContext.GetContext<PacketContext>(i);

            context.~PacketContext();
        }
    }

    if (m_queue)
    {
        m_adapterDispatch->DestroyQueue(m_adapter, m_queue);
    }
}

_Use_decl_annotations_
size_t
NxTxXlat::GetQueueId(
    void
) const
{
    return m_queueId;
}

void
NxTxXlat::ArmNetBufferListArrivalNotification()
{
    m_queueNotification.Set();
}

void
NxTxXlat::ArmAdapterTxNotification()
{
    m_queueDispatch->SetArmed(m_queue, true);
}

NxTxXlat::ArmedNotifications
NxTxXlat::GetNotificationsToArm()
{
    ArmedNotifications notifications;

    // Shouldn't arm any notifications if we don't want to halt
    if ((m_producedPackets == 0) && (m_completedPackets == 0))
    {
        // If the ringbuffer is not full (if there is room for OS to give more packets to NIC),
        // arm the translater serialization queue notification so that when new NBL is sent
        // to the translater later, it will wake up the transmit thread to send packets.
        notifications.Flags.ShouldArmNblArrival = !m_packetRing.AllPacketsOwnedByNic();

        // If 0 packets were completed by the Adapter in the last iteration, then arm
        // the adapter to issue Tx completion notifications.
        notifications.Flags.ShouldArmTxCompletion = m_packetRing.AnyNicPackets();

        // At least one notification should be set whenever going through this path.
        //
        // If the queue is not full, OS side notification will be armed, OS can wake translater
        // if it has more work. If NIC side notification is armed, NIC can wake translater if
        // it completed any packets.
        //
        // By default if this if block is called, the thread is going to halt
        // ShouldArmNblArrival or ShouldArmTxCompletion must be set. If there are
        // more work, this function is a no-op and keeps the main thread alive.
        NT_ASSERT(notifications.Value);
    }

    return notifications;
}

_Use_decl_annotations_
void
NxTxXlat::ArmNotifications(
    ArmedNotifications notifications
)
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

static EC_START_ROUTINE NetAdapterTransmitThread;

static
EC_RETURN
NetAdapterTransmitThread(
    PVOID StartContext
)
{
    reinterpret_cast<NxTxXlat*>(StartContext)->TransmitThread();
    return EC_RETURN();
}

void
NxTxXlat::SetupTxThreadProperties()
{
#if _KERNEL_MODE
    // setup thread prioirty;
    ULONG threadPriority =
        m_dispatch->NetClientQueryDriverConfigurationUlong(TX_THREAD_PRIORITY);

    KeSetBasePriorityThread(KeGetCurrentThread(), threadPriority - (LOW_REALTIME_PRIORITY + LOW_PRIORITY)/2);

    BOOLEAN setThreadAffinity =
        m_dispatch->NetClientQueryDriverConfigurationBoolean(TX_THREAD_AFFINITY_ENABLED);

    ULONG threadAffinity =
        m_dispatch->NetClientQueryDriverConfigurationUlong(TX_THREAD_AFFINITY);

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
NxTxXlat::TransmitThread()
{
    SetupTxThreadProperties();

    while (! m_executionContext.IsTerminated())
    {
        m_queueDispatch->Start(m_queue);

        auto cancelIssued = false;

        // This represents the core execution of the Tx path
        while (true)
        {
            if (!cancelIssued)
            {
                // Check if the NBL serialization has any data
                PollNetBufferLists();

                // Post NBLs to the producer side of the NBL
                TranslateNbls();
            }

            // Allow the NetAdapter to return any packets that it is done with.
            YieldToNetAdapter();

            // Drain any packets that the NIC has completed.
            // This means returning the associated NBLs for each completed
            // NET_PACKET.
            DrainCompletions();

            UpdatePerfCounter();

            // Arms notifications if no forward progress was made in
            // this loop.
            WaitForWork();

            // This represents the wind down of Tx
            if (m_executionContext.IsStopping())
            {
                if (!cancelIssued)
                {
                    // Indicate cancellation to the adapter
                    // and drop all outstanding NBLs.
                    //
                    // One NBL may remain that has been partially programmed into the NIC.
                    // So that NBL is kept around until the end

                    m_queueDispatch->Cancel(m_queue);
                    DropQueuedNetBufferLists();

                    cancelIssued = true;
                }

                // The termination condition is that the NIC has returned all its
                // packets.
                if (!m_packetRing.AnyNicPackets())
                {
                    if (m_packetRing.AnyReturnedPackets())
                    {
                        DrainCompletions();
                        NT_ASSERT(!m_packetRing.AnyReturnedPackets());
                    }

                    // DropQueuedNetBufferLists had completed as many NBLs as possible, but there's
                    // a chance that one parital NBL couldn't be completed up there.  Do it now.
                    AbortNbls(m_currentNbl);
                    m_currentNbl = nullptr;
                    m_currentNetBuffer = nullptr;

                    m_queueDispatch->Stop(m_queue);
                    m_executionContext.SignalStopped();
                    break;
                }
            }
        }
    }
}

void
NxTxXlat::DrainCompletions()
{
    NxNblTranslator translator{
        m_extensions,
        m_nblTranslationStats,
        &m_rings,
        m_datapathCapabilities,
        m_dmaAdapter.get(),
        m_packetContext,
        m_adapterProperties.MediaType,
        m_statistics
    };

    auto const result = translator.CompletePackets(m_bounceBufferPool);

    m_completedPackets = result.CompletedPackets;

    if (result.CompletedChain)
    {
        m_nblDispatcher->SendNetBufferListsComplete(
            result.CompletedChain, result.NumCompletedNbls, 0);
    }
}

void
NxTxXlat::TranslateNbls()
{
    m_producedPackets = 0;

    if (!m_currentNbl)
        return;

    NxNblTranslator translator{
        m_extensions,
        m_nblTranslationStats,
        &m_rings,
        m_datapathCapabilities,
        m_dmaAdapter.get(),
        m_packetContext,
        m_adapterProperties.MediaType,
        m_statistics
    };

    m_producedPackets = translator.TranslateNbls(m_currentNbl, m_currentNetBuffer, m_bounceBufferPool);
}

void
NxTxXlat::UpdatePerfCounter()
{
    auto pr = NetRingCollectionGetPacketRing(&m_rings);

    m_statistics.Increment(NxStatisticsCounters::IterationCount);
    m_statistics.IncrementBy(NxStatisticsCounters::NblPending, m_synchronizedNblQueue.GetNblQueueDepth());
    m_statistics.IncrementBy(NxStatisticsCounters::PacketsCompleted, m_completedPackets);
    m_statistics.IncrementBy(NxStatisticsCounters::QueueDepth, NetRingGetRangeCount(pr, pr->BeginIndex, pr->EndIndex));
}

void
NxTxXlat::WaitForWork()
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

void
NxTxXlat::DropQueuedNetBufferLists()
{
    // This routine completes both the currently dequeued chain
    // of NBLs and the queued chain of NBLs with NDIS_STATUS_PAUSED.
    // This should only be run during the run down of the Tx path.
    // When the Tx path is being run down, no more NBLs are delivered
    // to the NBL queue.

    NT_ASSERT(m_executionContext.IsStopping());

    if (m_currentNbl)
    {
        if (! m_currentNetBuffer)
        {
            AbortNbls(m_currentNbl);
            m_currentNbl = nullptr;
        }
        else
        {
            // At least one NB from the first NBL was already given to the NIC, so we
            // can't immediately complete it.  Complete the rest, at least.
            AbortNbls(m_currentNbl->Next);
            m_currentNbl->Next = nullptr;
        }
    }

    AbortNbls(DequeueNetBufferListQueue());
}

void
NxTxXlat::AbortNbls(
    _In_opt_ NET_BUFFER_LIST *nblChain)
{
    if (!nblChain)
        return;

    ndisSetStatusInNblChain(nblChain, NDIS_STATUS_PAUSED);

    m_nblDispatcher->SendNetBufferListsComplete(
        nblChain,
        ndisNumNblsInNblChain(nblChain),
        0);
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
    if (!m_currentNbl)
    {
        m_currentNbl = DequeueNetBufferListQueue();
    }
}

void
NxTxXlat::SendNetBufferLists(
    _In_ NET_BUFFER_LIST * NblChain,
    _In_ ULONG PortNumber,
    _In_ ULONG NumberOfNbls,
    _In_ ULONG SendFlags
)
{
    UNREFERENCED_PARAMETER((PortNumber, NumberOfNbls, SendFlags));

    m_synchronizedNblQueue.Enqueue(NblChain);

    if (m_queueNotification.TestAndClear())
    {
        m_executionContext.SignalWork();
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
    if (m_packetRing.AnyNicPackets())
    {
        if (m_dmaAdapter)
        {
            m_dmaAdapter->FlushIoBuffers(m_packetRing.NicPackets());
        }

        m_queueDispatch->Advance(m_queue);
    }
}

_Use_decl_annotations_
NTSTATUS
NxTxXlat::Initialize(
    void
)
{
    NT_FRE_ASSERT(ARRAYSIZE(TxQueueExtensions) == ARRAYSIZE(m_extensions.Extensions));

    m_adapterDispatch->GetDatapathCapabilities(m_adapter, &m_datapathCapabilities);

    NX_PERF_TX_NIC_CHARACTERISTICS perfCharacteristics = {};
    NX_PERF_TX_TUNING_PARAMETERS perfParameters;
    perfCharacteristics.Nic.IsDriverVerifierEnabled = !!m_adapterProperties.DriverIsVerifying;
    perfCharacteristics.Nic.MediaType = m_mediaType;
    perfCharacteristics.FragmentRingNumberOfElementsHint = m_datapathCapabilities.PreferredTxFragmentRingSize;
    perfCharacteristics.MaximumFragmentBufferSize = m_datapathCapabilities.MaximumTxFragmentSize;
    perfCharacteristics.NominalLinkSpeed = m_datapathCapabilities.NominalMaxTxLinkSpeed;
    perfCharacteristics.MaxPacketSizeWithLso = m_datapathCapabilities.MtuWithLso;

    NxPerfTunerCalculateTxParameters(&perfCharacteristics, &perfParameters);

    NET_CLIENT_QUEUE_CONFIG config;
    NET_CLIENT_QUEUE_CONFIG_INIT(
        &config,
        perfParameters.PacketRingElementCount,
        perfParameters.FragmentRingElementCount);

    config.Extensions = &TxQueueExtensions[0];
    config.NumberOfExtensions = ARRAYSIZE(TxQueueExtensions);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_adapterDispatch->CreateTxQueue(
            m_adapter,
            this,
            &QueueDispatch,
            &config,
            &m_queue,
            &m_queueDispatch),
        "Failed to create Tx queue. NxTxXlat=%p", this);

    // query extensions for what was enabled
    for (size_t i = 0; i < ARRAYSIZE(TxQueueExtensions); i++)
    {
        m_queueDispatch->GetExtension(
            m_queue,
            &TxQueueExtensions[i],
            &m_extensions.Extensions[i]);
    }

    RtlCopyMemory(&m_rings, m_queueDispatch->GetNetDatapathDescriptor(m_queue), sizeof(m_rings));

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_packetRing.Initialize(NetRingCollectionGetPacketRing(&m_rings)),
        "Failed to initialize packet ring buffer.");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_packetContext.Initialize(sizeof(PacketContext)),
        "Failed to initialize private context.");

    if (m_datapathCapabilities.TxMemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED)
    {
        m_dmaAdapter = wil::make_unique_nothrow<NxDmaAdapter>(m_datapathCapabilities, m_rings);

        if (!m_dmaAdapter)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        CX_RETURN_IF_NOT_NT_SUCCESS(m_dmaAdapter->Initialize(*m_dispatch));
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_bounceBufferPool.Initialize(
            *m_dispatch,
            m_datapathCapabilities,
            perfParameters.NumberOfBounceBuffers));

    for (auto i = 0ul; i < m_packetRing.Count(); i++)
    {
        new (&m_packetContext.GetContext<PacketContext>(i)) PacketContext();
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_executionContext.Initialize(this, NetAdapterTransmitThread),
        "Failed to start Tx execution context. NxTxXlat=%p", this);

    m_executionContext.SetDebugNameHint(L"Transmit", GetQueueId(), m_adapterProperties.NetLuid);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxTxXlat::Start(
    void
)
{
    m_executionContext.Start();
}

_Use_decl_annotations_
void
NxTxXlat::Cancel(
    void
)
{
    m_executionContext.Cancel();
}

_Use_decl_annotations_
void
NxTxXlat::Stop(
    void
)
{
    m_executionContext.Stop();
}

void
NxTxXlat::Notify()
{
    m_executionContext.SignalWork();
}
