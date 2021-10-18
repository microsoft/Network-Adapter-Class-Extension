// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <FxObject.hpp>
#include <KArray.h>
#include <KPtr.h>
#include <NetClientBuffer.h>
#include <NetClientQueue.h>

#include "extension/NxExtensionLayout.hpp"
#include "queue/Queue.hpp"

#include "ExecutionContext.hpp"

struct NX_PRIVATE_GLOBALS;
class NxAdapter;
class NxQueueVerifier;

#define QUEUE_CREATION_CONTEXT_SIGNATURE 0x7840dd95

using unique_packet_queue = wil::unique_wdf_any<NETPACKETQUEUE>;

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

    WDFOBJECT
        ParentObject = WDF_NO_HANDLE;

    Rtl::KArray<NET_EXTENSION_PRIVATE>
        Extensions;

    ULONG
        QueueId = 0;

    void *
        ClientQueue = nullptr;

    unique_packet_queue
        CreatedQueueObject;

    ExecutionContext *
        ExecutionContext = nullptr;
};

class NxQueue
    : public NetCx::Core::Queue
{
public:

    NxQueue(
        _In_ QUEUE_CREATION_CONTEXT const & InitContext,
        _In_ NET_PACKET_QUEUE_CONFIG const & QueueConfig,
        _In_ NetCx::Core::Queue::Type QueueType
    );

    void
    Cleanup(
        void
    );

    virtual
    ~NxQueue(
        void
    ) = default;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        _Inout_ QUEUE_CREATION_CONTEXT & InitContext
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    ExecutionContext *
    GetExecutionContext(
        void
    );

    void
    NotifyMorePacketsAvailable(
        void
    );

    NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY const *
    GetTxDemuxProperty(
        NET_CLIENT_QUEUE_TX_DEMUX_TYPE Type
    ) const;

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
    Advance(
        NxQueueVerifier & Verifier
    );

    void
    Cancel(
        void
    );

    void
    Cancel(
        NxQueueVerifier & Verifier
    );

    void
    SetArmed(
        _In_ bool isArmed
    );

    WDFOBJECT
    GetParent(
        void
    );

    WDFOBJECT
    GetWdfObject(
        void
    );

    ULONG const
        m_queueId;

    NetCx::Core::Queue::Type const
        m_queueType;

private:

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CreateFrameworkExecutionContext(
        void
    );

private:

    void *
        m_clientQueue = nullptr;

    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const *
        m_clientDispatch = nullptr;

    NET_PACKET_QUEUE_CONFIG
        m_packetQueueConfig = {};

    NETPACKETQUEUE const
        m_queue;

    WDFOBJECT const
        m_parent;

    // Used when client driver does not provide their own execution context
    wistd::unique_ptr<ExecutionContext>
        m_frameworkExecutionContext;

    ExecutionContext *
        m_executionContext = nullptr;

    EXECUTION_CONTEXT_NOTIFICATION
        m_driverNotification = {};
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxQueue, GetNxQueueFromHandle);

NTSTATUS
PacketQueueCreate(
    _In_ NX_PRIVATE_GLOBALS const & PrivateGlobals,
    _In_ NET_PACKET_QUEUE_CONFIG const & Configuration,
    _In_ QUEUE_CREATION_CONTEXT & InitContext,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * ClientAttributes,
    _In_ NetCx::Core::Queue::Type Type,
    _Out_ NETPACKETQUEUE * PacketQueue
);
