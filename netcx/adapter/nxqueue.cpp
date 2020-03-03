// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <net/ring.h>
#include <net/packet.h>

#include "NxQueue.tmh"
#include "NxQueue.hpp"

#include "NxAdapter.hpp"
#include "version.hpp"
#include "verifier.hpp"

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
NetClientQueueAdvanceVerifying(
    NET_CLIENT_QUEUE Queue
)
{
    reinterpret_cast<NxQueue *>(Queue)->AdvanceVerifying();
}

static
void
NetClientQueueCancelVerifying(
    NET_CLIENT_QUEUE Queue
)
{
    reinterpret_cast<NxQueue *>(Queue)->CancelVerifying();
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
    NET_CLIENT_EXTENSION const * ExtensionToQuery,
    NET_EXTENSION * Extension
)
{
    NET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = ExtensionToQuery->Name;
    extensionPrivate.Version = ExtensionToQuery->Version;
    extensionPrivate.Type = ExtensionToQuery->Type;

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

static NET_CLIENT_QUEUE_DISPATCH const QueueDispatch
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

static NET_CLIENT_QUEUE_DISPATCH const QueueDispatchVerifying
{
    { sizeof(NET_CLIENT_QUEUE_DISPATCH) },
    &NetClientQueueStart,
    &NetClientQueueStop,
    &NetClientQueueAdvanceVerifying,
    &NetClientQueueCancelVerifying,
    &NetClientQueueSetArmed,
    &NetClientQueueGetExtension,
    &NetClientQueueGetRingCollection,
};

_Use_decl_annotations_
NxQueue::NxQueue(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NET_PACKET_QUEUE_CONFIG const & QueueConfig,
    NxQueue::Type QueueType
) :
    m_privateGlobals(PrivateGlobals),
    m_queueId(QueueId),
    m_queueType(QueueType),
    m_adapter(InitContext.Adapter),
    m_packetLayout(sizeof(NET_PACKET), alignof(NET_PACKET)),
    m_fragmentLayout(sizeof(NET_FRAGMENT), alignof(NET_FRAGMENT)),
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

    if (m_privateGlobals.CxVerifierOn)
    {
        m_verifiedPacketIndex = 0;
        m_verifiedFragmentIndex = 0;
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
NxQueue::AdvancePreVerifying(
    void
)
{
    auto const pr = NetRingCollectionGetPacketRing(&m_ringCollection);
    auto const fr = NetRingCollectionGetFragmentRing(&m_ringCollection);

    if (m_queueType == Type::Rx)
    {
        // poison fields we expect may be written by the driver

        for (; m_verifiedPacketIndex != pr->EndIndex;
            m_verifiedPacketIndex = NetRingIncrementIndex(pr, m_verifiedPacketIndex))
        {
            auto packet = NetRingGetPacketAtIndex(pr, m_verifiedPacketIndex);

            packet->FragmentIndex = ~0U;
            packet->FragmentCount = static_cast<UINT16>(~0U);
            packet->Layout = { 0x7f, 0x1ff, 0xff, 0xf, 0xf, 0xf };
        }

        auto const rxCapabilities = m_adapter->GetRxCapabilities();
        for (; m_verifiedFragmentIndex != fr->EndIndex;
            m_verifiedFragmentIndex = NetRingIncrementIndex(fr, m_verifiedFragmentIndex))
        {
            auto fragment = NetRingGetFragmentAtIndex(fr, m_verifiedFragmentIndex);

            // this won't work because these are valid values.

            fragment->ValidLength = ~0U;
            fragment->Offset = ~0U;
            if (rxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver)
            {
                fragment->Capacity = ~0U;
            }
        }
    }

    auto vpr = m_verifiedRings[NetRingTypePacket].get();
    auto vfr = m_verifiedRings[NetRingTypeFragment].get();

    RtlCopyMemory(
        vpr,
        pr,
        pr->ElementStride * pr->NumberOfElements + FIELD_OFFSET(NET_RING, Buffer[0]));

    RtlCopyMemory(
        vfr,
        fr,
        fr->ElementStride * fr->NumberOfElements + FIELD_OFFSET(NET_RING, Buffer[0]));

    if (m_queueType == Type::Rx)
    {
        // poison fields we expect may be written by the driver

        for (auto verifiedIndex = vpr->BeginIndex; verifiedIndex != vpr->EndIndex;
            verifiedIndex = NetRingIncrementIndex(vpr, verifiedIndex))
        {
            auto packet = NetRingGetPacketAtIndex(vpr, verifiedIndex);

            packet->FragmentIndex = ~0U;
            packet->FragmentCount = static_cast<UINT16>(~0U);
            packet->Layout = { 0x7f, 0x1ff, 0xff, 0xf, 0xf, 0xf };
        }

        auto const rxCapabilities = m_adapter->GetRxCapabilities();
        for (auto verifiedIndex = vfr->BeginIndex; verifiedIndex != vfr->EndIndex;
            verifiedIndex = NetRingIncrementIndex(vfr, verifiedIndex))
        {
            auto fragment = NetRingGetFragmentAtIndex(vfr, verifiedIndex);

            // this won't work because these are valid values.

            fragment->ValidLength = ~0U;
            fragment->Offset = ~0U;
            if (rxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver)
            {
                fragment->Capacity = ~0U;
            }
        }
    }
}

void
NxQueue::AdvancePostVerifying(
    void
) const
{
    auto const pr = NetRingCollectionGetPacketRing(&m_ringCollection);
    auto const fr = NetRingCollectionGetFragmentRing(&m_ringCollection);

    Verifier_VerifyRingImmutable(
        m_privateGlobals,
        *m_verifiedRings[NetRingTypePacket].get(),
        *pr);

    Verifier_VerifyRingBeginIndex(
        m_privateGlobals,
        *m_verifiedRings[NetRingTypePacket].get(),
        *pr);

    Verifier_VerifyRingImmutable(
        m_privateGlobals,
        *m_verifiedRings[NetRingTypeFragment].get(),
        *fr);

    Verifier_VerifyRingBeginIndex(
        m_privateGlobals,
        *m_verifiedRings[NetRingTypeFragment].get(),
        *fr);

    Verifier_VerifyRingImmutableFrameworkElements(
        m_privateGlobals,
        *m_verifiedRings[NetRingTypePacket].get(),
        *pr);

    Verifier_VerifyRingImmutableFrameworkElements(
        m_privateGlobals,
        *m_verifiedRings[NetRingTypeFragment].get(),
        *fr);

    switch (m_queueType)
    {

    case Type::Tx:
        Verifier_VerifyRingImmutableDriverTxPackets(
            m_privateGlobals,
            *m_verifiedRings[NetRingTypePacket].get(),
            *pr);

        Verifier_VerifyRingImmutableDriverTxFragments(
            m_privateGlobals,
            *m_verifiedRings[NetRingTypeFragment].get(),
            *fr);

        break;

    case Type::Rx:
        Verifier_VerifyRingImmutableDriverRx(
            m_privateGlobals,
            m_adapter->GetRxCapabilities(),
            *m_verifiedRings[NetRingTypePacket].get(),
            *pr,
            *m_verifiedRings[NetRingTypeFragment].get(),
            *fr);

        break;

    }
}

void
NxQueue::AdvanceVerifying(
    void
)
{
    AdvancePreVerifying();

    if (++m_toggle & 1)
    {
        KIRQL irql;
        KeRaiseIrql(DISPATCH_LEVEL, &irql);
        Advance();
        KeLowerIrql(irql);
    }
    else
    {
        Advance();
    }

    AdvancePostVerifying();
}

void
NxQueue::Cancel(
    void
)
{
    m_packetQueueConfig.EvtCancel(m_queue);
}

void
NxQueue::CancelVerifying(
    void
)
{
    AdvancePreVerifying();

    KIRQL irql;
    KeRaiseIrql(DISPATCH_LEVEL, &irql);
    Cancel();
    KeLowerIrql(irql);

    AdvancePostVerifying();
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
    for (auto const & extension : InitContext.Extensions)
    {
        switch (extension.Type)
        {

        case NetExtensionTypePacket:
            CX_RETURN_IF_NOT_NT_SUCCESS(
                m_packetLayout.PutExtension(
                    extension.Name,
                    extension.Version,
                    extension.Type,
                    extension.Size,
                    extension.NonWdfStyleAlignment));

            break;

        case NetExtensionTypeFragment:
            CX_RETURN_IF_NOT_NT_SUCCESS(
                m_fragmentLayout.PutExtension(
                    extension.Name,
                    extension.Version,
                    extension.Type,
                    extension.Size,
                    extension.NonWdfStyleAlignment));
            break;

        }
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateRing(
            m_fragmentLayout.Generate(),
            InitContext.ClientQueueConfig->NumberOfFragments,
            NetRingTypeFragment));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateRing(
            m_packetLayout.Generate(),
            InitContext.ClientQueueConfig->NumberOfPackets,
            NetRingTypePacket));

    if (m_privateGlobals.CxVerifierOn)
    {
        *InitContext.AdapterDispatch = &QueueDispatchVerifying;
    }
    else
    {
        *InitContext.AdapterDispatch = &QueueDispatch;
    }

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
    const NET_EXTENSION_PRIVATE * ExtensionToQuery,
    NET_EXTENSION * Extension
)
{
    NET_EXTENSION_PRIVATE const * extension = nullptr;
    NET_RING_TYPE type = NetRingTypePacket;
    switch (ExtensionToQuery->Type)
    {

    case NetExtensionTypePacket:
        type = NetRingTypePacket;
        extension = m_packetLayout.GetExtension(
            ExtensionToQuery->Name,
            ExtensionToQuery->Version,
            ExtensionToQuery->Type);

        break;

    case NetExtensionTypeFragment:
        type = NetRingTypeFragment;
        extension = m_fragmentLayout.GetExtension(
            ExtensionToQuery->Name,
            ExtensionToQuery->Version,
            ExtensionToQuery->Type);

        break;

    }

    Extension->Enabled = extension != nullptr;
    if (Extension->Enabled)
    {
        auto const ring = m_ringCollection.Rings[type];
        Extension->Reserved[0] = ring->Buffer + extension->AssignedOffset;
        Extension->Reserved[1] = reinterpret_cast<void *>(ring->ElementStride);
    }
}

_Use_decl_annotations_
NxTxQueue::NxTxQueue(
    WDFOBJECT Object,
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NET_PACKET_QUEUE_CONFIG const & QueueConfig
) :
    CFxObject(static_cast<NETPACKETQUEUE>(Object)),
    NxQueue(PrivateGlobals, InitContext, QueueId, QueueConfig, NxQueue::Type::Tx)
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
        NxQueue::Initialize(InitContext),
        "Failed to create Tx queue.");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxRxQueue::NxRxQueue(
    WDFOBJECT Object,
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NET_PACKET_QUEUE_CONFIG const & QueueConfig
) :
    CFxObject(static_cast<NETPACKETQUEUE>(Object)),
    NxQueue(PrivateGlobals, InitContext, QueueId, QueueConfig, NxQueue::Type::Rx)
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
        NxQueue::Initialize(InitContext),
        "Failed to create Rx queue.");

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

    if (m_privateGlobals.CxVerifierOn)
    {
        ring = reinterpret_cast<NET_RING *>(
            ExAllocatePoolWithTag(NonPagedPoolNxCacheAligned, size, 'BRxN'));

        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! ring);

        RtlZeroMemory(ring, size);
        ring->OSReserved2[1] = this;
        ring->ElementStride = static_cast<USHORT>(ElementSize);
        ring->NumberOfElements = static_cast<UINT32>(ElementCount);
        ring->ElementIndexMask = static_cast<ULONG>(ElementCount - 1);
        m_verifiedRings[RingType].reset(ring);
        m_verifiedRingCollection.Rings[RingType] = m_verifiedRings[RingType].get();
    }

    return STATUS_SUCCESS;
}

