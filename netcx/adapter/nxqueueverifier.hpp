// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

class NxQueueVerifier
{
public:

    NxQueueVerifier(
        _In_ NX_PRIVATE_GLOBALS const & PrivateGlobals,
        _In_ WDFOBJECT ParentObject,
        _In_ NETPACKETQUEUE Queue
    );

    NTSTATUS
    Initialize(
        _In_ NET_RING_COLLECTION const * Rings
    );

    void
    PreStart(
        void
    );

    void
    PreAdvance(
        _In_ NET_RING_COLLECTION const * Rings
    );

    void
    PostAdvance(
        _In_ NET_RING_COLLECTION const * Rings
    ) const;

private:

    NTSTATUS
    CreateRing(
        _In_ size_t ElementSize,
        _In_ UINT32 ElementCount,
        _In_ NET_RING_TYPE RingType
    );

private:

    NxAdapter *
        m_adapter = nullptr;

    NxQueue *
        m_queue = nullptr;

    NX_PRIVATE_GLOBALS const &
        m_privateGlobals;

    UINT32
        m_verifiedPacketIndex = 0;

    UINT32
        m_verifiedFragmentIndex = 0;

    UINT32
        m_verifiedDataBufferIndex = 0;

    KPoolPtr<NET_RING>
        m_verifiedRings[NetRingTypeDataBuffer + 1];

    NET_RING_COLLECTION
        m_verifiedRingCollection;

    UINT8
        m_toggle = 0;

    KIRQL
        m_irql = PASSIVE_LEVEL;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxQueueVerifier, GetNxQueueVerifierFromHandle);

NTSTATUS
PacketQueueVerifierCreate(
    _In_ NX_PRIVATE_GLOBALS const & PrivateGlobals,
    _In_ WDFOBJECT ParentObject,
    _In_ NETPACKETQUEUE PacketQueue,
    _Out_ NxQueueVerifier ** QueueVerifier
);
