// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the
    transmit path.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxNblDatapath.hpp"

#include "NxTxXlat.tmh"
#include "NxTxXlat.hpp"

#include <ndis/nblchain.h>
#include <net/checksumtypes_p.h>
#include <net/logicaladdresstypes_p.h>
#include <net/gsotypes_p.h>
#include <net/mdltypes_p.h>
#include <net/virtualaddresstypes_p.h>
#include <net/wifi/exemptionaction_p.h>
#include <net/ieee8021qtypes_p.h>

#include "NxTranslationapp.hpp"
#include <netiodef.h>
#include <pktmonclnt.h>
#include <pktmonloc.h>

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
        NET_PACKET_EXTENSION_GSO_NAME,
        NET_PACKET_EXTENSION_GSO_VERSION_1,
        0,
        NET_PACKET_EXTENSION_GSO_VERSION_1_SIZE,
        NetExtensionTypePacket,
    },
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_PACKET_EXTENSION_WIFI_EXEMPTION_ACTION_NAME,
        NET_PACKET_EXTENSION_WIFI_EXEMPTION_ACTION_VERSION_1,
        0,
        NET_PACKET_EXTENSION_WIFI_EXEMPTION_ACTION_VERSION_1_SIZE,
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
    {
        sizeof(NET_CLIENT_EXTENSION),
        NET_PACKET_EXTENSION_IEEE8021Q_NAME,
        NET_PACKET_EXTENSION_IEEE8021Q_VERSION_1,
        0,
        NET_PACKET_EXTENSION_IEEE8021Q_VERSION_1_SIZE,
        NetExtensionTypePacket,
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

static
NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY const *
NetClientQueueGetTxDemuxProperty(
    _In_ NET_CLIENT_QUEUE Queue,
    _In_ NET_CLIENT_QUEUE_TX_DEMUX_TYPE Type
)
{
    return reinterpret_cast<NxTxXlat *>(Queue)->GetTxDemuxProperty(Type);

}

static const NET_CLIENT_QUEUE_NOTIFY_DISPATCH QueueDispatch
{
    { sizeof(NET_CLIENT_QUEUE_NOTIFY_DISPATCH) },
    &NetClientQueueNotify,
    &NetClientQueueGetTxDemuxProperty
};

using PacketContext = NxNblTranslator::PacketContext;

static
EVT_EXECUTION_CONTEXT_POLL
    EvtTxPollQueueStarted;

static
EVT_EXECUTION_CONTEXT_POLL
    EvtTxPollQueueStopping;

_Use_decl_annotations_
NxTxXlat::NxTxXlat(
    NxTranslationApp & App,
    bool OnDemand,
    size_t QueueId,
    QueueControlSynchronize & QueueControl,
    NET_CLIENT_DISPATCH const * Dispatch,
    NET_CLIENT_ADAPTER Adapter,
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
) noexcept
    : Queue(
        App,
        QueueId,
        EvtTxPollQueueStarted,
        EvtTxPollQueueStopping)
    , m_onDemand(OnDemand)
    , m_queueControl(QueueControl)
    , m_dispatch(Dispatch)
    , m_adapter(Adapter)
    , m_adapterDispatch(AdapterDispatch)
    , m_bounceBufferPool(
        m_rings,
        m_extensions.Extension.VirtualAddress,
        m_extensions.Extension.LogicalAddress,
        m_extensions.Extension.Mdl)
    , m_packetContext(m_rings, NetRingTypePacket)
{
    m_adapterDispatch->GetProperties(m_adapter, &m_adapterProperties);
    m_adapterDispatch->GetDatapathCapabilities(m_adapter, &m_datapathCapabilities);
    m_nblDispatcher = static_cast<INxNblDispatcher *>(m_adapterProperties.NblDispatcher);
    m_pktmonComponentContext = reinterpret_cast<PKTMON_COMPONENT_CONTEXT const *>(m_adapterProperties.PacketMonitorComponentContext);
}

NxTxXlat::~NxTxXlat()
{
    if (IsCreated())
    {
        Destroy();
    }
}

_Use_decl_annotations_
void
EvtTxPollQueueStarted(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    auto nxTxXlat = reinterpret_cast<NxTxXlat *>(Context);
    nxTxXlat->TransmitNbls(Parameters);
}

_Use_decl_annotations_
void
EvtTxPollQueueStopping(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    auto nxTxXlat = reinterpret_cast<NxTxXlat *>(Context);
    nxTxXlat->TransmitNblsStopping(Parameters);
}

void
NxTxXlat::TransmitNbls(
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
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

    UpdatePerfCounter();

    Parameters->WorkCounter = m_producedPackets + m_completedPackets;
}

void
NxTxXlat::TransmitNblsStopping(
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    bool isAllPacketsReturnedToOs = false;
    bool isAllPacketsSendCompleted = false;

    if (m_packetRing.AnyNicPackets())
    {
        // if NIC still has pending packets, continue to poll them so the client driver
        // can keep returning any packets that it is done with.
        YieldToNetAdapter();
    }
    else
    {
        isAllPacketsReturnedToOs = true;
    }

    if (m_packetRing.AnyReturnedPackets())
    {
        // continue to drain any packets that the NIC has completed.
        // This means returning the associated NBLs for each completed
        // NET_PACKET
        DrainCompletions();
    }
    else
    {
        isAllPacketsSendCompleted = true;
    }

    UpdatePerfCounter();

    // The termination condition is that the NIC has returned all its
    // packets and netcx has send-completed all packets too
    if (isAllPacketsReturnedToOs && isAllPacketsSendCompleted)
    {
        // we are done, drop any pending NBLs at gate
        DropQueuedNetBufferLists();

        // DropQueuedNetBufferLists had completed as many NBLs as possible, but there's
        // a chance that one parital NBL couldn't be completed up there.  Do it now.
        AbortNbls(m_currentNbl);
        m_currentNbl = nullptr;
        m_currentNetBuffer = nullptr;

        // signalling event to stop polling
        m_readyToStop.Set();
    }

    Parameters->WorkCounter = m_producedPackets + m_completedPackets;
}

void
NxTxXlat::DrainCompletions()
{
    NxNblTranslator translator{
        m_currentNbl,
        m_currentNetBuffer,
        m_datapathCapabilities,
        m_extensions,
        m_statistics,
        m_bounceBufferPool,
        m_packetContext,
        m_dmaAdapter.get(),
        m_adapterProperties.MediaType,
        &m_rings,
        m_checksumContext,
        m_gsoContext,
        m_layoutParseBuffer,
        m_checksumHardwareCapabilities,
        m_txChecksumHardwareCapabilities,
        m_gsoHardwareCapabilities,
        m_adapterProperties.PacketMonitorLowerEdge,
        m_adapterProperties.PacketMonitorComponentContext
    };

    auto const result = translator.CompletePackets();

    m_completedPackets = result.CompletedPackets;

    if (result.CompletedChain)
    {
        m_nblDispatcher->SendNetBufferListsComplete(
            result.CompletedChain, result.NumCompletedNbls, 0);
    }
    m_nblDispatcher->CountTxStatistics(translator.GetCounters());
}

void
NxTxXlat::TranslateNbls()
{
    m_producedPackets = 0;

    if (!m_currentNbl)
        return;

    NxNblTranslator translator{
        m_currentNbl,
        m_currentNetBuffer,
        m_datapathCapabilities,
        m_extensions,
        m_statistics,
        m_bounceBufferPool,
        m_packetContext,
        m_dmaAdapter.get(),
        m_adapterProperties.MediaType,
        &m_rings,
        m_checksumContext,
        m_gsoContext,
        m_layoutParseBuffer,
        m_checksumHardwareCapabilities,
        m_txChecksumHardwareCapabilities,
        m_gsoHardwareCapabilities,
        m_adapterProperties.PacketMonitorLowerEdge,
        m_adapterProperties.PacketMonitorComponentContext
    };

    m_producedPackets = translator.TranslateNbls();
}

void
NxTxXlat::UpdatePerfCounter()
{
    auto pr = NetRingCollectionGetPacketRing(&m_rings);

    InterlockedIncrementNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Perf.IterationCount));
    InterlockedAddNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Perf.NblPending), m_synchronizedNblQueue.GetNblQueueDepth());
    InterlockedAddNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Perf.PacketsCompleted), m_completedPackets);
    InterlockedAddNoFence64(reinterpret_cast<LONG64 *>(&m_statistics.Perf.QueueDepth), NetRingGetRangeCount(pr, pr->BeginIndex, pr->EndIndex));
}

void
NxTxXlat::DropQueuedNetBufferLists()
{
    // This routine completes both the currently dequeued chain
    // of NBLs and the queued chain of NBLs with NDIS_STATUS_PAUSED.
    // This should only be run during the run down of the Tx path.
    // When the Tx path is being run down, no more NBLs are delivered
    // to the NBL queue.

    if (m_currentNbl)
    {
        PKTMON_DROP_NBL(
            const_cast<PKTMON_COMPONENT_CONTEXT *>(m_pktmonComponentContext),
            m_currentNbl,
            PktMonDir_Out,
            PktMonDrop_NetCx_NicQueueStop,
            PMLOC_NETCX_NIC_QUEUE_STOP);

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
    _In_opt_ NET_BUFFER_LIST * nblChain
)
{
    if (! nblChain)
    {
        return;
    }

    NdisSetStatusInNblChain(nblChain, NDIS_STATUS_PAUSED);

    m_nblDispatcher->SendNetBufferListsComplete(
        nblChain,
        NdisNumNblsInNblChain(nblChain),
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

_Use_decl_annotations_
void
NxTxXlat::QueueNetBufferList(
    NET_BUFFER_LIST * NetBufferList
)
{
    // Prepare MiniportReserved fields to be used by NetCx by explicitly clearing it,
    // because some filter drivers (e.g. pacer.sys) use MiniportReserved as their scratch field.
    NetBufferList->MiniportReserved[0] = nullptr;
    NetBufferList->MiniportReserved[1] = nullptr;

    m_synchronizedNblQueue.Enqueue(NetBufferList);

    if (InterlockedExchange(&m_nblNotificationEnabled, FALSE) == TRUE)
    {
        m_executionContextDispatch->Notify(m_executionContext);
    }

    if (! (IsCreated() && IsStarted()))
    {
        m_queueControl.SignalQueueStateChange();
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
    NT_FRE_ASSERT(! m_created);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_checksumContext.Initialize());

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTxXlat::Create(
    void
)
{
    NT_FRE_ASSERT(! m_created);
    NT_FRE_ASSERT(ARRAYSIZE(TxQueueExtensions) == ARRAYSIZE(m_extensions.Extensions));

    m_adapterDispatch->GetDatapathCapabilities(m_adapter, &m_datapathCapabilities);

    NT_FRE_ASSERT(m_datapathCapabilities.NominalMtu != 0u);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_layoutParseBuffer.resize(m_datapathCapabilities.NominalMtu));

    NX_PERF_TX_NIC_CHARACTERISTICS const perfCharacteristics = {
        {
            static_cast<ULONG>(m_adapterProperties.MediaType), // Nic.MediaType
            !! m_adapterProperties.DriverIsVerifying, // Nic.IsDriverVerifierEnabled
        },
        m_datapathCapabilities.PreferredTxFragmentRingSize, // FragmentRingNumberOfElementsHint
        m_datapathCapabilities.MaximumTxFragmentSize, // MaximumFragmentBufferSize
        0u, // MaxFragmentsPerPacket
        m_datapathCapabilities.MtuWithGso, // MaxPacketSizeWithGso
        m_datapathCapabilities.NominalMaxTxLinkSpeed, // NominalLinkSpeed
    };

    NX_PERF_TX_TUNING_PARAMETERS perfParameters;
    NxPerfTunerCalculateTxParameters(&perfCharacteristics, &perfParameters);

    NET_CLIENT_QUEUE_CONFIG config;
    NET_CLIENT_QUEUE_CONFIG_INIT(
        &config,
        perfParameters.PacketRingElementCount,
        perfParameters.FragmentRingElementCount);

    config.Extensions = &TxQueueExtensions[0];
    config.NumberOfExtensions = ARRAYSIZE(TxQueueExtensions);

    m_properties.reserve(3);

    auto const nbl = m_synchronizedNblQueue.PeekNbl();
    if (nbl)
    {
        m_app.GenerateDemuxProperties(nbl, m_properties);
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_adapterDispatch->CreateTxQueue(
            m_adapter,
            this,
            &QueueDispatch,
            &config,
            &m_queue,
            &m_queueDispatch,
            &m_executionContext,
            &m_executionContextDispatch),
        "Failed to create Tx queue. NxTxXlat=%p", this);

    auto deleteQueueIfNotSuccess = wil::scope_exit([this]()
    {
        m_adapterDispatch->DestroyQueue(m_adapter, m_queue);
    });

    // query extensions for what was enabled
    for (size_t i = 0; i < ARRAYSIZE(TxQueueExtensions); i++)
    {
        m_queueDispatch->GetExtension(
            m_queue,
            &TxQueueExtensions[i],
            &m_extensions.Extensions[i]);
    }

    if (m_extensions.Extension.Checksum.Enabled)
    {
        m_adapterDispatch->OffloadDispatch.GetChecksumHardwareCapabilities(
            m_adapter,
            &m_checksumHardwareCapabilities);

        m_adapterDispatch->OffloadDispatch.GetTxChecksumHardwareCapabilities(
            m_adapter,
            &m_txChecksumHardwareCapabilities);
    }

    if (m_extensions.Extension.Gso.Enabled)
    {
        m_adapterDispatch->OffloadDispatch.GetGsoHardwareCapabilities(
            m_adapter,
            &m_gsoHardwareCapabilities);
    }

    RtlCopyMemory(
        &m_rings,
        m_queueDispatch->GetNetDatapathDescriptor(m_queue),
        sizeof(m_rings));

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_packetRing.Initialize(NetRingCollectionGetPacketRing(&m_rings)),
        "Failed to initialize packet ring buffer.");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_packetContext.Initialize(sizeof(PacketContext)),
        "Failed to initialize private context.");

    auto const requirement = m_datapathCapabilities.TxMemoryConstraints.MappingRequirement;
    if (requirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED)
    {
        m_dmaAdapter = wil::make_unique_nothrow<NxDmaAdapter>(
            m_datapathCapabilities,
            m_rings);

        if (! m_dmaAdapter)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        CX_RETURN_IF_NOT_NT_SUCCESS(
            m_dmaAdapter->Initialize(*m_dispatch));
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

    m_executionContextDispatch->RegisterNotification(
        m_executionContext,
        &m_nblNotification);

    for (auto const & property : m_properties)
    {
        switch (property.Type)
        {
        case TxDemuxType8021p:
        case TxDemuxTypeWmmInfo:
            LogInfo(FLAG_TRANSLATOR,
                "Tx Queue Create %Iu"
                " Properties"
                " Type %!QUEUETXDEMUXTYPE!"
                " Demux %Iu"
                " Value %u",
                GetQueueId(),
                property.Type,
                property.Value,
                property.Property.UserPriority);

            break;

        case TxDemuxTypePeerAddress:
            LogInfo(FLAG_TRANSLATOR,
                "Tx Queue Create %Iu"
                " Properties"
                " Type %!QUEUETXDEMUXTYPE!"
                " Demux %Iu"
                " Value %0.2x:%0.2x:%0.2x:%0.2x:%0.2x:%0.2x",
                GetQueueId(),
                property.Type,
                property.Value,
                property.Property.PeerAddress.Value[0],
                property.Property.PeerAddress.Value[1],
                property.Property.PeerAddress.Value[2],
                property.Property.PeerAddress.Value[3],
                property.Property.PeerAddress.Value[4],
                property.Property.PeerAddress.Value[5]);

            break;

        }
    }

    m_created = true;
    deleteQueueIfNotSuccess.release();
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxTxXlat::Destroy(
    void
)
{
    for (auto const & property : m_properties)
    {
        switch (property.Type)
        {
        case TxDemuxType8021p:
        case TxDemuxTypeWmmInfo:
            LogInfo(FLAG_TRANSLATOR,
                "Tx Queue Destroy %Iu"
                " Properties"
                " Type %!QUEUETXDEMUXTYPE!"
                " Demux %Iu"
                " Value %u",
                GetQueueId(),
                property.Type,
                property.Value,
                property.Property.UserPriority);

            break;

        case TxDemuxTypePeerAddress:
            LogInfo(FLAG_TRANSLATOR,
                "Tx Queue Destroy %Iu"
                " Properties"
                " Type %!QUEUETXDEMUXTYPE!"
                " Demux %Iu"
                " Value %0.2x:%0.2x:%0.2x:%0.2x:%0.2x:%0.2x",
                GetQueueId(),
                property.Type,
                property.Value,
                property.Property.PeerAddress.Value[0],
                property.Property.PeerAddress.Value[1],
                property.Property.PeerAddress.Value[2],
                property.Property.PeerAddress.Value[3],
                property.Property.PeerAddress.Value[4],
                property.Property.PeerAddress.Value[5]);

            break;

        }
    }

    NT_FRE_ASSERT(IsCreated());
    NT_FRE_ASSERT(! IsQueuingNetBufferLists());

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
        if (!IsListEmpty(&m_nblNotification.Link))
        {
            m_executionContextDispatch->UnregisterNotification(m_executionContext, &m_nblNotification);
        }

        m_adapterDispatch->DestroyQueue(m_adapter, m_queue);
    }

    m_created = false;
}

bool
NxTxXlat::IsCreated(
    void
) const
{
    return m_created;
}

bool
NxTxXlat::IsOnDemand(
    void
) const
{
    return m_onDemand;
}

_Use_decl_annotations_
bool
NxTxXlat::IsQueuingNetBufferLists(
    void
) const
{
    return m_synchronizedNblQueue.GetNblQueueDepth() != 0;
}

void
NxTxXlat::Notify()
{
    NT_FRE_ASSERT(false);
}

NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY const *
NxTxXlat::GetTxDemuxProperty(
    NET_CLIENT_QUEUE_TX_DEMUX_TYPE Type
) const
{
    for (auto const & property : m_properties)
    {
        if (property.Type == Type)
        {
            return &property;
        }
    }

    return nullptr;
}

const NxTxCounters &
NxTxXlat::GetStatistics(
    void
) const
{
    return m_statistics;
}
