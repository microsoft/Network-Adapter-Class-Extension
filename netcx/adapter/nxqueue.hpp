// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <FxObject.hpp>
#include <KArray.h>
#include <KPtr.h>
#include <NetClientQueue.h>

#include "extension/NxExtensionLayout.hpp"

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
{
public:

    enum class Type
    {
        Rx,
        Tx,
    };

    NxQueue(
        _In_ NX_PRIVATE_GLOBALS const & PrivateGlobals,
        _In_ QUEUE_CREATION_CONTEXT const & InitContext,
        _In_ ULONG QueueId,
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

    NET_RING_COLLECTION const *
    GetRingCollection(
        void
    ) const;

    NxAdapter *
    GetAdapter(
        void
    );

    void
    GetExtension(
        _In_ const NET_EXTENSION_PRIVATE* ExtensionToQuery,
        _Out_ NET_EXTENSION* Extension
    );

    ULONG const
        m_queueId = ~0U;

    NxQueue::Type const
        m_queueType;

protected:

    NxAdapter *
        m_adapter = nullptr;

    NxExtensionLayout
        m_packetLayout;

    NxExtensionLayout
        m_fragmentLayout;

private:

    NTSTATUS
    CreateRing(
        _In_ size_t ElementSize,
        _In_ UINT32 ElementCount,
        _In_ NET_RING_TYPE RingType
    );

    void
    AdvancePreVerifying(
        void
    );

    void
    AdvancePostVerifying(
        void
    ) const;

    NX_PRIVATE_GLOBALS const &
        m_privateGlobals;

    KPoolPtr<NET_RING>
        m_rings[NetRingTypeFragment + 1];

    NET_RING_COLLECTION
        m_ringCollection;

    UINT32
        m_verifiedPacketIndex = 0;

    UINT32
        m_verifiedFragmentIndex = 0;

    KPoolPtr<NET_RING>
        m_verifiedRings[NetRingTypeFragment + 1];

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

protected:

    NETPACKETQUEUE
        m_queue = nullptr;
};

class NxTxQueue;

FORCEINLINE
NxTxQueue *
GetTxQueueFromHandle(
    _In_ NETPACKETQUEUE TxQueue
    );

class NxTxQueue :
    public NxQueue,
    public CFxObject<NETPACKETQUEUE, NxTxQueue, GetTxQueueFromHandle, true>
{
public:

    NxTxQueue(
        _In_ WDFOBJECT Object,
        _In_ NX_PRIVATE_GLOBALS const & PrivateGlobals,
        _In_ QUEUE_CREATION_CONTEXT const & InitContext,
        _In_ ULONG QueueId,
        _In_ NET_PACKET_QUEUE_CONFIG const & QueueConfig
    );

    virtual
    ~NxTxQueue(
        void
    ) = default;

    NTSTATUS
    Initialize(
        _In_ QUEUE_CREATION_CONTEXT & InitContext
    );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxTxQueue, _GetTxQueueFromHandle);

FORCEINLINE
NxTxQueue *
GetTxQueueFromHandle(
    NETPACKETQUEUE TxQueue
    )
{
    return _GetTxQueueFromHandle(TxQueue);
}

class NxRxQueue;

FORCEINLINE
NxRxQueue *
GetRxQueueFromHandle(
    _In_ NETPACKETQUEUE RxQueue
);

class NxRxQueue :
    public NxQueue,
    public CFxObject<NETPACKETQUEUE, NxRxQueue, GetRxQueueFromHandle, true>
{

public:

    NxRxQueue(
        _In_ WDFOBJECT Object,
        _In_ NX_PRIVATE_GLOBALS const & PrivateGlobals,
        _In_ QUEUE_CREATION_CONTEXT const & InitContext,
        _In_ ULONG QueueId,
        _In_ NET_PACKET_QUEUE_CONFIG const & QueueConfig
    );

    virtual
    ~NxRxQueue(
        void
    ) = default;

    NTSTATUS
    Initialize(
        _In_ QUEUE_CREATION_CONTEXT & InitContext
    );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxRxQueue, _GetRxQueueFromHandle);

FORCEINLINE
NxRxQueue *
GetRxQueueFromHandle(
    NETPACKETQUEUE RxQueue
    )
{
    return _GetRxQueueFromHandle(RxQueue);
}

