// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include "NxAdapter.hpp"
#include "version.hpp"
#include "verifier.hpp"

#include "queue/Queue.hpp"

#include "NxQueueVerifier.hpp"

#include "NxQueueVerifier.tmh"

using namespace NetCx::Core;

_Use_decl_annotations_
NTSTATUS
PacketQueueVerifierCreate(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    WDFOBJECT ParentObject,
    NETPACKETQUEUE PacketQueue,
    NxQueueVerifier ** QueueVerifier
)
{
    *QueueVerifier = nullptr;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxQueueVerifier);
    attributes.EvtDestroyCallback = [](WDFOBJECT Object)
    {
        auto queueVerifier = GetNxQueueVerifierFromHandle(Object);
        queueVerifier->~NxQueueVerifier();
    };

    void * memory;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfObjectAllocateContext(
            PacketQueue,
            &attributes,
            &memory));

    *QueueVerifier = new (memory) NxQueueVerifier(
        PrivateGlobals,
        ParentObject,
        PacketQueue);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxQueueVerifier::NxQueueVerifier(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    WDFOBJECT ParentObject,
    NETPACKETQUEUE Queue
)
    : m_privateGlobals(PrivateGlobals)
    , m_adapter(GetNxAdapterFromHandle(ParentObject))
    , m_queue(GetNxQueueFromHandle(Queue))
{
    NT_FRE_ASSERT(m_adapter != nullptr);
}

NTSTATUS
NxQueueVerifier::Initialize(
    NET_RING_COLLECTION const * Rings
)
{
    auto pr = NetRingCollectionGetPacketRing(Rings);
    auto fr = NetRingCollectionGetFragmentRing(Rings);
    auto br = NetRingCollectionGetDataBufferRing(Rings);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CreateRing(
            pr->ElementStride,
            pr->NumberOfElements,
            NetRingTypePacket),
        "Failed to allocate packet ring copy");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CreateRing(
            fr->ElementStride,
            fr->NumberOfElements,
            NetRingTypeFragment),
        "Failed to allocate fragment ring copy");

    if (br)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            CreateRing(
                br->ElementStride,
                br->NumberOfElements,
                NetRingTypeDataBuffer),
            "Failed to allocate data buffer ring copy");
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NxQueueVerifier::CreateRing(
    _In_ size_t ElementSize,
    _In_ UINT32 ElementCount,
    _In_ NET_RING_TYPE RingType
)
{
    NT_FRE_ASSERT(RTL_IS_POWER_OF_TWO(ElementCount));

    auto const size = ElementSize * ElementCount + FIELD_OFFSET(NET_RING, Buffer[0]);

    auto ring = reinterpret_cast<NET_RING *>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED | POOL_FLAG_CACHE_ALIGNED, size, 'BRxN'));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! ring);

    ring->OSReserved2[1] = this;
    ring->ElementStride = static_cast<USHORT>(ElementSize);
    ring->NumberOfElements = ElementCount;
    ring->ElementIndexMask = ElementCount - 1;

    m_verifiedRings[RingType].reset(ring);
    m_verifiedRingCollection.Rings[RingType] = m_verifiedRings[RingType].get();

    return STATUS_SUCCESS;
}

void
NxQueueVerifier::PreStart(
    void
)
{
    m_verifiedPacketIndex = 0;
    m_verifiedFragmentIndex = 0;
    m_verifiedDataBufferIndex = 0;
}

_Use_decl_annotations_
void
NxQueueVerifier::PreAdvance(
    NET_RING_COLLECTION const * Rings
)
{
    auto const pr = NetRingCollectionGetPacketRing(Rings);
    auto const fr = NetRingCollectionGetFragmentRing(Rings);
    auto const br = NetRingCollectionGetDataBufferRing(Rings);

    if (m_queue->m_queueType == Queue::Type::Rx)
    {
        // poison fields we expect may be written by the driver

        for (; m_verifiedPacketIndex != pr->EndIndex;
            m_verifiedPacketIndex = NetRingIncrementIndex(pr, m_verifiedPacketIndex))
        {
            auto packet = NetRingGetPacketAtIndex(pr, m_verifiedPacketIndex);

            packet->FragmentIndex = UINT32_MAX;
            packet->FragmentCount = UINT16_MAX;
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

        // data buffer ring has no fields written by the driver

        if (br)
        {
            m_verifiedDataBufferIndex = br->EndIndex;
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

    if (br)
    {
        auto vbr = m_verifiedRings[NetRingTypeDataBuffer].get();
        RtlCopyMemory(
            vbr,
            br,
            br->ElementStride * br->NumberOfElements + FIELD_OFFSET(NET_RING, Buffer[0]));
    }

    if (m_queue->m_queueType == Queue::Type::Rx)
    {
        // poison fields we expect may be written by the driver

        for (auto verifiedIndex = vpr->BeginIndex; verifiedIndex != vpr->EndIndex;
            verifiedIndex = NetRingIncrementIndex(vpr, verifiedIndex))
        {
            auto packet = NetRingGetPacketAtIndex(vpr, verifiedIndex);

            packet->FragmentIndex = UINT32_MAX;
            packet->FragmentCount = UINT16_MAX;
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

        // data buffer ring has no fields written by the driver
    }

    if (++m_toggle & 1)
    {
        KeRaiseIrql(DISPATCH_LEVEL, &m_irql);
    }
}

_Use_decl_annotations_
void
NxQueueVerifier::PostAdvance(
    NET_RING_COLLECTION const * Rings
) const
{
    if (m_toggle & 1)
    {
        KeLowerIrql(m_irql);
    }

    auto const pr = NetRingCollectionGetPacketRing(Rings);
    auto const fr = NetRingCollectionGetFragmentRing(Rings);
    auto const br = NetRingCollectionGetDataBufferRing(Rings);

    Verifier_VerifyRingImmutable(
        m_privateGlobals,
        *m_verifiedRings[NetRingTypePacket].get(),
        *pr);

    Verifier_VerifyRingBeginIndex(
        m_privateGlobals,
        *m_verifiedRings[NetRingTypePacket].get(),
        *pr);

    Verifier_VerifyRingImmutableFrameworkElements(
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
        *m_verifiedRings[NetRingTypeFragment].get(),
        *fr);

    if (br)
    {
        Verifier_VerifyRingImmutable(
            m_privateGlobals,
            *m_verifiedRings[NetRingTypeDataBuffer].get(),
            *br);

        Verifier_VerifyRingBeginIndex(
            m_privateGlobals,
            *m_verifiedRings[NetRingTypeDataBuffer].get(),
            *br);

        Verifier_VerifyRingImmutableFrameworkElements(
            m_privateGlobals,
            *m_verifiedRings[NetRingTypeDataBuffer].get(),
            *br);
    }

    switch (m_queue->m_queueType)
    {

    case Queue::Type::Tx:
        Verifier_VerifyRingImmutableDriverTxPackets(
            m_privateGlobals,
            *m_verifiedRings[NetRingTypePacket].get(),
            *pr);

        Verifier_VerifyRingImmutableDriverTxFragments(
            m_privateGlobals,
            *m_verifiedRings[NetRingTypeFragment].get(),
            *fr);

        break;

    case Queue::Type::Rx:
        Verifier_VerifyRingImmutableDriverRx(
            m_privateGlobals,
            m_adapter->GetRxCapabilities(),
            *m_verifiedRings[NetRingTypePacket].get(),
            *pr,
            *m_verifiedRings[NetRingTypeFragment].get(),
            *fr);

        if (br)
        {
            Verifier_VerifyRingImmutableDriverRx(
                m_privateGlobals,
                *m_verifiedRings[NetRingTypeDataBuffer].get(),
                *br);
        }

        break;

    }
}
