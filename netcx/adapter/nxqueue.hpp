// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <FxObject.hpp>
#include <KArray.h>
#include <KPtr.h>
#include <NetClientBuffer.h>
#include <NetClientQueue.h>
#include "BufferPool.hpp"

#include "extension/NxExtensionLayout.hpp"
#include "queue/Queue.hpp"

struct NX_PRIVATE_GLOBALS;
class NxAdapter;

#define QUEUE_CREATION_CONTEXT_SIGNATURE 0x7840dd95

struct QUEUE_CREATION_CONTEXT
{
    ULONG
        Signature = QUEUE_CREATION_CONTEXT_SIGNATURE;

    PKTHREAD
        CurrentThread = nullptr;

    NET_CLIENT_QUEUE_CONFIG const *
        ClientQueueConfig = nullptr;

    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const *
        ClientDispatch = nullptr;

    NET_CLIENT_QUEUE_DISPATCH const **
        AdapterDispatch = nullptr;

    NxAdapter *
        Adapter = nullptr;

    Rtl::KArray<NET_EXTENSION_PRIVATE>
        Extensions;

    ULONG
        QueueId = 0;

    void *
        ClientQueue = nullptr;

    wil::unique_wdf_object
        CreatedQueueObject;

};

class NxQueue
    : public NetCx::Core::Queue
{
public:

    enum class Type
    {
        Rx,
        Tx,
    };

    NxQueue(
        _In_ NX_PRIVATE_GLOBALS const & PrivateGlobals,
        _In_ WDFOBJECT ObjectHandle,
        _In_ QUEUE_CREATION_CONTEXT const & InitContext,
        _In_ NET_PACKET_QUEUE_CONFIG const & QueueConfig,
        _In_ NxQueue::Type QueueType
    );

    virtual
    ~NxQueue(
        void
    ) = default;

    NTSTATUS
    Initialize(
        _In_ QUEUE_CREATION_CONTEXT & InitContext
    );

    void
    NotifyMorePacketsAvailable(
        void
    );

    void
    Start(
        void
    );

    void
    Stop(
        void
    );

    void
    Advance(
        void
    );

    void
    AdvanceVerifying(
        void
    );

    void
    CancelVerifying(
        void
    );

    void
    Cancel(
        void
    );

    void
    SetArmed(
        _In_ bool isArmed
    );

    WDFOBJECT
    GetWdfObject(
        void
    );

    ULONG const
        m_queueId;

    NxQueue::Type const
        m_queueType;

private:

    void
    AdvancePreVerifying(
        void
    );

    void
    AdvancePostVerifying(
        void
    ) const;

    NxAdapter *
        m_adapter = nullptr;

    NX_PRIVATE_GLOBALS const &
        m_privateGlobals;

    UINT32
        m_verifiedPacketIndex = 0;

    UINT32
        m_verifiedFragmentIndex = 0;

    KPoolPtr<NET_RING>
        m_verifiedRings[NetRingTypeDataBuffer + 1];

    NET_RING_COLLECTION
        m_verifiedRingCollection;

    void *
        m_clientQueue = nullptr;

    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const *
        m_clientDispatch = nullptr;

    NET_PACKET_QUEUE_CONFIG const
        m_packetQueueConfig;

    UINT8
        m_toggle = 0;

    NETPACKETQUEUE const
        m_queue;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxQueue, GetNxQueueFromHandle);
