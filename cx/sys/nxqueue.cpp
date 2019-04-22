// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <net/ring.h>
#include <net/packet.h>

#include "NxQueue.tmh"
#include "NxQueue.hpp"

#include "NxAdapter.hpp"

void
NetClientQueueStart(
    NET_CLIENT_QUEUE Queue
)
{
    reinterpret_cast<NxQueue *>(Queue)->Start();
}

void
NetClientQueueStop(
    NET_CLIENT_QUEUE Queue
)
{
    reinterpret_cast<NxQueue *>(Queue)->Stop();
}

void
NetClientQueueAdvance(
    NET_CLIENT_QUEUE Queue
)
{
    reinterpret_cast<NxQueue *>(Queue)->Advance();
}

static
void
NetClientQueueCancel(
    NET_CLIENT_QUEUE Queue
)
{
    reinterpret_cast<NxQueue *>(Queue)->Cancel();
}

static
void
NetClientQueueSetArmed(
    NET_CLIENT_QUEUE Queue,
    BOOLEAN IsArmed
)
{
    reinterpret_cast<NxQueue *>(Queue)->SetArmed(IsArmed);
}

static
void
NetClientQueueGetExtension(
    NET_CLIENT_QUEUE Queue,
    NET_CLIENT_PACKET_EXTENSION const * ExtensionToQuery,
    NET_EXTENSION * Extension
)
{
    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = ExtensionToQuery->Name;
    extensionPrivate.Version = ExtensionToQuery->Version;

    reinterpret_cast<NxQueue *>(Queue)->GetExtension(&extensionPrivate, Extension);
}

static
NET_RING_COLLECTION const *
NetClientQueueGetRingCollection(
    _In_ NET_CLIENT_QUEUE Queue
)
{
    return reinterpret_cast<NxQueue *>(Queue)->GetRingCollection();
}

static const NET_CLIENT_QUEUE_DISPATCH QueueDispatch
{
    { sizeof(NET_CLIENT_QUEUE_DISPATCH) },
    &NetClientQueueStart,
    &NetClientQueueStop,
    &NetClientQueueAdvance,
    &NetClientQueueCancel,
    &NetClientQueueSetArmed,
    &NetClientQueueGetExtension,
    &NetClientQueueGetRingCollection,
};

_Use_decl_annotations_
NxQueue::NxQueue(
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NET_PACKET_QUEUE_CONFIG const & QueueConfig,
    NxQueue::Type QueueType
) :
    m_queueId(QueueId),
    m_queueType(QueueType),
    m_adapter(InitContext.Adapter),
    m_clientQueue(InitContext.ClientQueue),
    m_clientDispatch(InitContext.ClientDispatch),
    m_packetQueueConfig(QueueConfig)
{
    NOTHING;
}

void
NxQueue::Start(
    void
)
{
    for (auto ring : m_ringCollection.Rings)
    {
        if (ring != nullptr)
        {
            ring->BeginIndex = 0;
            ring->NextIndex = 0;
            ring->EndIndex = 0;
            ring->OSReserved2[0] = static_cast<void *>(0);
        }
    }

    if (m_packetQueueConfig.EvtStart)
    {
        m_packetQueueConfig.EvtStart(m_queue);
    }
}

void
NxQueue::Stop(
    void
)
{
    if (m_packetQueueConfig.EvtStop)
    {
        m_packetQueueConfig.EvtStop(m_queue);
    }
}

void
NxQueue::Advance(
    void
)
{
    m_packetQueueConfig.EvtAdvance(m_queue);
}

void
NxQueue::Cancel(
    void
)
{
    m_packetQueueConfig.EvtCancel(m_queue);
}

void
NxQueue::SetArmed(
    bool IsArmed
)
{
    m_packetQueueConfig.EvtSetNotificationEnabled(m_queue, IsArmed);
}

WDFOBJECT
NxQueue::GetWdfObject(
    void
)
{
    return m_queue;
}

void
NxQueue::NotifyMorePacketsAvailable(
    void
)
{
    m_clientDispatch->Notify(m_clientQueue);
}

NxAdapter *
NxQueue::GetAdapter(
    void
)
{
    return m_adapter;
}

NTSTATUS
NxQueue::Initialize(
    _In_ QUEUE_CREATION_CONTEXT & InitContext
)
{
    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateRing(
            ALIGN_UP_BY(sizeof(NET_FRAGMENT), NET_FRAGMENT_ALIGNMENT_BYTES),
            InitContext.ClientQueueConfig->NumberOfFragments,
            NET_RING_TYPE_FRAGMENT));

    if (m_AddedPacketExtensions.count() != 0)
    {
        Rtl::KArray<NET_PACKET_EXTENSION_PRIVATE*> temp;

        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            !temp.resize(m_AddedPacketExtensions.count()));

        for (size_t i = 0; i < m_AddedPacketExtensions.count(); i++)
        {
            temp[i] = &m_AddedPacketExtensions[i];
        }

        m_privateExtensionOffset =
            static_cast<ULONG>(NetPacketComputeSizeAndUpdateExtensions(
                reinterpret_cast<HNETPACKETEXTENSIONCOLLECTION>(&temp[0]),
                static_cast<ULONG>(temp.count())));
    }
    else
    {
        m_privateExtensionOffset = NetPacketGetSize();
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateRing(
            ALIGN_UP_BY(m_privateExtensionOffset, NET_PACKET_ALIGNMENT_BYTES),
            InitContext.ClientQueueConfig->NumberOfPackets,
            NET_RING_TYPE_PACKET));

    *InitContext.AdapterDispatch = &QueueDispatch;

    return STATUS_SUCCESS;
}

NET_RING_COLLECTION const *
NxQueue::GetRingCollection(
    void
) const
{
    return &m_ringCollection;
}

_Use_decl_annotations_
void
NxQueue::GetExtension(
    const NET_PACKET_EXTENSION_PRIVATE* ExtensionToQuery,
    NET_EXTENSION* Extension
)
{
    Extension->Enabled = false;

    for (auto& extension : m_AddedPacketExtensions)
    {
        if ((0 == wcscmp(extension.Name, ExtensionToQuery->Name)) &&
            (extension.Version >= ExtensionToQuery->Version))
        {
            auto const ring = m_ringCollection.Rings[NET_RING_TYPE_PACKET];
            Extension->Reserved[0] = ring->Buffer + extension.AssignedOffset;
            Extension->Reserved[1] = reinterpret_cast<void *>(ring->ElementStride);
            Extension->Enabled = true;
            break;
        }
    }
}

_Use_decl_annotations_
NxTxQueue::NxTxQueue(
    WDFOBJECT Object,
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NET_PACKET_QUEUE_CONFIG const & QueueConfig
) :
    CFxObject(static_cast<NETPACKETQUEUE>(Object)),
    NxQueue(InitContext, QueueId, QueueConfig, NxQueue::Type::Tx)
{
    m_queue = static_cast<NETPACKETQUEUE>(Object);
}

_Use_decl_annotations_
NTSTATUS
NxTxQueue::Initialize(
    QUEUE_CREATION_CONTEXT & InitContext
)
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PrepareAndStorePacketExtensions(InitContext),
        "Failed to consolidate extensions");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxQueue::Initialize(InitContext),
        "Failed to create Tx queue.");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxRxQueue::NxRxQueue(
    WDFOBJECT Object,
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NET_PACKET_QUEUE_CONFIG const & QueueConfig
) :
    CFxObject(static_cast<NETPACKETQUEUE>(Object)),
    NxQueue(InitContext, QueueId, QueueConfig, NxQueue::Type::Rx)
{
    m_queue = reinterpret_cast<NETPACKETQUEUE>(Object);
}

_Use_decl_annotations_
NTSTATUS
NxRxQueue::Initialize(
    QUEUE_CREATION_CONTEXT & InitContext
)
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PrepareAndStorePacketExtensions(InitContext),
        "Failed to consolidate extensions");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxQueue::Initialize(InitContext),
        "Failed to create Rx queue.");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxQueue::NetQueueInitAddPacketExtension(
    QUEUE_CREATION_CONTEXT *CreationContext,
    const NET_PACKET_EXTENSION_PRIVATE* PacketExtension
)
{
    //
    // Right now, don't allow unregistered extensions to be added to
    // QUEUE_CREATION_CONTEXT
    //
    NET_PACKET_EXTENSION_PRIVATE* storedExtension =
        CreationContext->Adapter->QueryAndGetNetPacketExtensionWithLock(PacketExtension);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        !storedExtension);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !CreationContext->NetAdapterAddedPacketExtensions.append(*storedExtension));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxQueue::PrepareAndStorePacketExtensions(
    QUEUE_CREATION_CONTEXT & InitContext
)
{
    auto const sortByAlignment = [](
        NET_PACKET_EXTENSION_PRIVATE const & Lhs,
        NET_PACKET_EXTENSION_PRIVATE const & Rhs
)
    {
        return Lhs.NonWdfStyleAlignment < Rhs.NonWdfStyleAlignment;
    };

    for (auto& extensionToAdd : InitContext.NetAdapterAddedPacketExtensions)
    {
        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! m_AddedPacketExtensions.insertSorted(
                extensionToAdd,
                sortByAlignment));
    }

    for (auto& extensionToAdd : InitContext.NetClientAddedPacketExtensions)
    {
        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! m_AddedPacketExtensions.insertSorted(
                extensionToAdd,
                sortByAlignment));
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxQueue::CreateRing(
    size_t ElementSize,
    UINT32 ElementCount,
    NET_RING_TYPE RingType
)
{
    auto const size = ElementSize * ElementCount + FIELD_OFFSET(NET_RING, Buffer[0]);
    auto ring = reinterpret_cast<NET_RING *>(
        ExAllocatePoolWithTag(NonPagedPoolNxCacheAligned, size, 'BRxN'));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! ring);

    RtlZeroMemory(ring, size);
    ring->OSReserved2[1] = this;
    ring->ElementStride = static_cast<USHORT>(ElementSize);
    ring->NumberOfElements = static_cast<UINT32>(ElementCount);
    ring->ElementIndexMask = static_cast<ULONG>(ElementCount - 1);

    m_rings[RingType].reset(ring);
    m_ringCollection.Rings[RingType] = m_rings[RingType].get();

    return STATUS_SUCCESS;
}

