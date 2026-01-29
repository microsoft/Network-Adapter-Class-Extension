// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <net/ring.h>
#include <net/packet.h>
#include <NetClientDriverConfigurationImpl.hpp>
#ifdef NETCX_KMDF
#include "NxExecutionContext.hpp"
#endif
#include "NxQueue.hpp"
#include "NxDevice.hpp"
#include "NxQueueVerifier.hpp"

#include "NxAdapter.hpp"
#include "version.hpp"
#include "verifier.hpp"

#include "NxQueue.tmh"


using namespace NetCx::Core;

static
EVT_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED
    EvtNxQueueSetNotificationEnabled;

static
EVT_WDF_OBJECT_CONTEXT_CLEANUP
    EvtQueueCleanup;

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
NetClientQueueStartVerifying(
    NET_CLIENT_QUEUE Queue
)
{
    auto nxQueue = reinterpret_cast<NxQueue *>(Queue);
    auto queueVerifier = GetNxQueueVerifierFromHandle(nxQueue->GetWdfObject());

    queueVerifier->PreStart();
    nxQueue->Start();
}

static
void
NetClientQueueAdvanceVerifying(
    NET_CLIENT_QUEUE Queue
)
{
    auto nxQueue = reinterpret_cast<NxQueue *>(Queue);

    nxQueue->Advance(*GetNxQueueVerifierFromHandle(nxQueue->GetWdfObject()));
}

static
void
NetClientQueueCancelVerifying(
    NET_CLIENT_QUEUE Queue
)
{
    auto nxQueue = reinterpret_cast<NxQueue *>(Queue);

    nxQueue->Cancel(*GetNxQueueVerifierFromHandle(nxQueue->GetWdfObject()));
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

#ifdef NETCX_UMDF
_Use_decl_annotations_
void
EvtAdapterQueuePoll(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Poll
)
{
    auto nxQueue = static_cast<NxQueue *>(Context);
    nxQueue->Advance(Poll);
}
#endif

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
    &NetClientQueueStartVerifying,
    &NetClientQueueStop,
    &NetClientQueueAdvanceVerifying,
    &NetClientQueueCancelVerifying,
    &NetClientQueueSetArmed,
    &NetClientQueueGetExtension,
    &NetClientQueueGetRingCollection,
};

_Use_decl_annotations_
NTSTATUS
PacketQueueCreate(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_PACKET_QUEUE_CONFIG const & Configuration,
    QUEUE_CREATION_CONTEXT & InitContext,
    WDF_OBJECT_ATTRIBUTES * ClientAttributes,
    Queue::Type Type,
    NETPACKETQUEUE * PacketQueue
)
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxQueue);
    attributes.ParentObject = InitContext.ParentObject;
    attributes.EvtCleanupCallback = EvtQueueCleanup;
    attributes.EvtDestroyCallback = [](WDFOBJECT Object)
    {
        auto nxQueue = GetNxQueueFromHandle(Object);
        nxQueue->~NxQueue();
    };

    unique_packet_queue queue;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(
            &attributes,
            reinterpret_cast<WDFOBJECT *>(&queue)),
        "WdfObjectCreate for packet queue failed.");

    auto nxQueue = new (GetNxQueueFromHandle(queue.get()))
        NxQueue(
            InitContext,
            Configuration,
            Type);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        nxQueue->Initialize(InitContext),
        "Packet queue creation failed. NxPrivateGlobals=%p", &PrivateGlobals);

    if (PrivateGlobals.CxVerifierOn)
    {
        NxQueueVerifier * queueVerifier;

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            PacketQueueVerifierCreate(
                PrivateGlobals,
                InitContext.ParentObject,
                queue.get(),
                &queueVerifier),
            "Failed to allocate queue verifier context");

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            queueVerifier->Initialize(nxQueue->GetRingCollection()),
            "Failed to initialize queue verifier context");
    }

    if (ClientAttributes != WDF_NO_OBJECT_ATTRIBUTES)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfObjectAllocateContext(
                queue.get(),
                ClientAttributes,
                nullptr),
            "Failed to allocate client context. NxQueue=%p", nxQueue);
    }

    InitContext.ExecutionContext = nxQueue->GetExecutionContext();

    *InitContext.AdapterDispatch =
        PrivateGlobals.CxVerifierOn
        ? &QueueDispatchVerifying
        : &QueueDispatch;

    *PacketQueue = queue.release();

    CX_RETURN_STATUS_SUCCESS_MSG(
        "Packet queue created. NETPACKETQUEUE %p, Type %!QUEUETYPE!, NETADAPTER %p",
        *PacketQueue,
        static_cast<ULONG>(Type),
        InitContext.ParentObject);
}

_Use_decl_annotations_
NxQueue::NxQueue(
    QUEUE_CREATION_CONTEXT const & InitContext,
    NET_PACKET_QUEUE_CONFIG const & QueueConfig,
    Queue::Type QueueType
)
    : m_queue{ static_cast<NETPACKETQUEUE>(WdfObjectContextGetObject(this)) }
    , m_parent{ InitContext.ParentObject }
    , m_queueType{ QueueType }
    , m_queueId{ InitContext.QueueId }
    , m_clientQueue{ InitContext.ClientQueue }
    , m_clientDispatch{ InitContext.ClientDispatch }
#ifdef NETCX_UMDF
    , m_startTask{ *this }
    , m_cancelTask{ *this }
    , m_stopTask{ *this }
#endif
{
    m_demuxPeerAddressProperty = InitContext.ClientQueueConfig->DemuxPeerAddressProperty;
    m_demuxWmmProperty = InitContext.ClientQueueConfig->DemuxWmmProperty;
    m_demux8021pProperty = InitContext.ClientQueueConfig->Demux8021pProperty;

    INITIALIZE_EXECUTION_CONTEXT_NOTIFICATION(
        &m_driverNotification,
        this,
        EvtNxQueueSetNotificationEnabled);

    RtlCopyMemory(
        &m_packetQueueConfig,
        &QueueConfig,
        QueueConfig.Size);

#ifdef NETCX_UMDF
    INITIALIZE_EXECUTION_CONTEXT_POLL(
        &m_poll,
        ExecutionContextPollTypeAdvance,
        this,
        EvtAdapterQueuePoll);
#endif
}

_Use_decl_annotations_
void
EvtQueueCleanup(
    WDFOBJECT Object
)
{
    LogInfo(
        FLAG_NET_QUEUE,
        "NETPACKETQUEUE %p deleted",
        Object);

    auto queue = GetNxQueueFromHandle(Object);
    queue->Cleanup();
}

void
NxQueue::Cleanup(
    void
)
{
#ifdef NETCX_KMDF
    if (m_packetQueueConfig.ExecutionContext != WDF_NO_HANDLE)
    {
        WdfObjectDereference(m_packetQueueConfig.ExecutionContext);
    }
#endif
}

_Use_decl_annotations_
NTSTATUS
NxQueue::Initialize(
    QUEUE_CREATION_CONTEXT & InitContext
)
{
#ifdef NETCX_KMDF
    if (m_packetQueueConfig.ExecutionContext != WDF_NO_HANDLE)
    {
        // If the client driver provided their own execution context we need to reference
        // the object, this will protect queue execution against the driver deleting their
        // EC object. Reference is removed in NxQueue::EvtCleanup
        WdfObjectReference(m_packetQueueConfig.ExecutionContext);

        m_executionContext = GetExecutionContextFromHandle(m_packetQueueConfig.ExecutionContext);
    }
    else
#endif
    {
        // If the client driver did not provide their own execution context we create and manage
        // the lifetime of one for them
        CX_RETURN_IF_NOT_NT_SUCCESS(
            CreateFrameworkExecutionContext());

        m_executionContext = m_frameworkExecutionContext.get();
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        Queue::Initialize(
            InitContext.Extensions,
            InitContext.ClientQueueConfig->NumberOfPackets,
            InitContext.ClientQueueConfig->NumberOfFragments,
            InitContext.CreateReturnContextRing));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxQueue::CreateFrameworkExecutionContext(
    void
)
{
    auto nxAdapter = GetNxAdapterFromHandle(GetParent());

    m_frameworkExecutionContext =
        wil::make_unique_nothrow<ExecutionContextWrapper>(
            nxAdapter->GetInterfaceGuid(),
            nxAdapter->m_executionContextKnobs);

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_frameworkExecutionContext,
        "Failed to allocate new ExecutionContextWrapper object");

#ifdef NETCX_KMDF
    // Construct a friendly name for the EC to help when analyzing traces.
    // This is best effort, failure to do so won't fail queue creation.
    auto const & name = nxAdapter->GetInstanceName();
    auto const usage = m_queueType == Type::Tx
        ? L"Send"
        : L"Receive";

    DECLARE_UNICODE_STRING_SIZE(friendlyName, 260);

    auto const ntStatus = RtlUnicodeStringPrintf(
        &friendlyName,
        L"%ws execution context #%lu for %wZ",
        usage,
        m_queueId,
        &name);

    // EC gives a generic name if we pass a nullptr
    auto const pFriendlyName = ntStatus == STATUS_SUCCESS
        ? &friendlyName
        : nullptr;
#else
    UNICODE_STRING *pFriendlyName = nullptr;
#endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_frameworkExecutionContext->Initialize(pFriendlyName),
        "Failed to initialize new ExecutionContextWrapper object");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
ExecutionContextWrapper *
NxQueue::GetExecutionContext(
    void
)
{
    return m_executionContext;
}

template <typename PFn>
void
InvokeQueueCallback(
    _In_ ExecutionContextWrapper * ExecutionContext,
    _In_ NETPACKETQUEUE Queue,
    _In_ PFn Callback,
    _In_ const wchar_t *CallbackName
)
{
    auto lambda = [Queue, Callback, CallbackName]()
    {
        LogInfo(
            FLAG_NET_QUEUE,
            "Begin(%ws): NETPACKETQUEUE %p",
            CallbackName,
            Queue);

        Callback(Queue);

        LogInfo(
            FLAG_NET_QUEUE,
            "End(%ws): NETPACKETQUEUE %p",
            CallbackName,
            Queue);
    };

#if NETCX_KMDF
    // In KMDF, invoke the queue callback from the execution context.
    // This function returns only after the task is complete.

    LambdaTask invokeCallbackTask{ lambda };

    // Queue an EC task and wait until it is complete
    ExecutionContext->QueueTask(invokeCallbackTask.GetTask());
    invokeCallbackTask.WaitForCompletion();
#else
    // In UMDF version queue callbacks are already serialized by EC
    UNREFERENCED_PARAMETER(ExecutionContext);
    lambda();
#endif // NETCX_KMDF
}

void
NxQueue::Start(
    void
)
{
    LogInfo(
        FLAG_NET_QUEUE,
        "Entry: NETPACKETQUEUE %p",
        m_queue);

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
        InvokeQueueCallback(
            m_executionContext,
            m_queue,
            m_packetQueueConfig.EvtStart,
            L"EvtStart");
    }

#ifdef NETCX_KMDF
    if (m_packetQueueConfig.EvtSetNotificationEnabled != nullptr)
    {
        m_executionContext->RegisterNotification(&m_driverNotification);
    }
#endif

    LogInfo(
        FLAG_NET_QUEUE,
        "Exit: NETPACKETQUEUE %p",
        m_queue);
}

void
NxQueue::Stop(
    void
)
{
    LogInfo(
        FLAG_NET_QUEUE,
        "Entry: NETPACKETQUEUE %p",
        m_queue);

#ifdef NETCX_KMDF
    if (m_packetQueueConfig.EvtSetNotificationEnabled != nullptr)
    {
        m_executionContext->UnregisterNotification(&m_driverNotification);
    }
#endif

    if (m_packetQueueConfig.EvtStop)
    {
        InvokeQueueCallback(
            m_executionContext,
            m_queue,
            m_packetQueueConfig.EvtStop,
            L"EvtStop");
    }

    LogInfo(
        FLAG_NET_QUEUE,
        "Exit: NETPACKETQUEUE %p",
        m_queue);
}

void
NxQueue::Advance(
    void
)
{
    m_packetQueueConfig.EvtAdvance(m_queue);
}


void
NxQueue::Advance(
    NxQueueVerifier & Verifier
)
{
    auto rings = GetRingCollection();

    Verifier.PreAdvance(rings);
    m_packetQueueConfig.EvtAdvance(m_queue);
    Verifier.PostAdvance(rings);
}

#ifdef NETCX_UMDF
void
NxQueue::Advance(
    EXECUTION_CONTEXT_POLL_PARAMETERS * Poll
)
{
    auto pr = NetRingCollectionGetPacketRing(&m_ringCollection);
    auto fr = NetRingCollectionGetFragmentRing(&m_ringCollection);

    const UINT32 oPrBegin = pr->BeginIndex;
    const UINT32 oFrBegin = fr->BeginIndex;

    this->Advance();

    Poll->WorkCounter += NetRingGetRangeCount(pr, oPrBegin, pr->BeginIndex);
    Poll->WorkCounter += NetRingGetRangeCount(fr, oFrBegin, fr->BeginIndex);
}
#endif

void
NxQueue::Cancel(
    void
)
{
    LogInfo(
        FLAG_NET_QUEUE,
        "Entry: NETPACKETQUEUE %p",
        m_queue);

    if (m_packetQueueConfig.EvtCancel != nullptr)
    {
        LogInfo(
            FLAG_NET_QUEUE,
            "Begin(EvtCancel): NETPACKETQUEUE %p",
            m_queue);

        m_packetQueueConfig.EvtCancel(m_queue);

        LogInfo(
            FLAG_NET_QUEUE,
            "End(EvtCancel): NETPACKETQUEUE %p",
            m_queue);
    }

    LogInfo(
        FLAG_NET_QUEUE,
        "Exit: NETPACKETQUEUE %p",
        m_queue);
}

void
NxQueue::Cancel(
    NxQueueVerifier & Verifier
)
{
    LogInfo(
        FLAG_NET_QUEUE,
        "Entry: NETPACKETQUEUE %p",
        m_queue);

    if (m_packetQueueConfig.EvtCancel != nullptr)
    {
        auto rings = GetRingCollection();

        Verifier.PreAdvance(rings);

        KIRQL irql;
        KeRaiseIrql(DISPATCH_LEVEL, &irql);

        LogInfo(
            FLAG_NET_QUEUE,
            "Begin(EvtCancel): NETPACKETQUEUE %p",
            m_queue);

        m_packetQueueConfig.EvtCancel(m_queue);

        LogInfo(
            FLAG_NET_QUEUE,
            "End(EvtCancel): NETPACKETQUEUE %p",
            m_queue);

        KeLowerIrql(irql);

        Verifier.PostAdvance(rings);
    }

    LogInfo(
        FLAG_NET_QUEUE,
        "Exit: NETPACKETQUEUE %p",
        m_queue);
}

void
EvtNxQueueSetNotificationEnabled(
    void * Context,
    BOOLEAN NotificationEnabled
)
{
    auto nxQueue = static_cast<NxQueue *>(Context);
    nxQueue->SetArmed(NotificationEnabled);
}

void
NxQueue::SetArmed(
    bool IsArmed
)
{
    if (m_queueType == NxQueue::Type::Tx)
    {
        // To keep compatibility with shipped API only invoke a Tx Queue's
        // SetNotificationEnabled if both are true:
        //     - The notifiation is being armed
        //     - There are pending Tx packets
        auto const * pr = NetRingCollectionGetPacketRing(GetRingCollection());
        auto const anyNicPackets = pr->BeginIndex != pr->EndIndex;

        if (IsArmed && anyNicPackets)
        {
            m_packetQueueConfig.EvtSetNotificationEnabled(m_queue, IsArmed);
        }
    }
    else
    {
        // To keep compatibility with shipped API only invoke a Rx Queue's
        // SetNotificationEnabled if the notification is being armed
        if (IsArmed)
        {
            m_packetQueueConfig.EvtSetNotificationEnabled(m_queue, IsArmed);
        }
    }
}

WDFOBJECT
NxQueue::GetParent(
    void
)
{
    return m_parent;
}

WDFOBJECT
NxQueue::GetWdfObject(
    void
)
{
    return m_queue;
}

#ifdef NETCX_UMDF
void
NxQueue::GetCallbacks(
    _Out_ EXECUTION_CONTEXT_POLL * Advance,
    _Out_ EXECUTION_CONTEXT_TASK * Start,
    _Out_ EXECUTION_CONTEXT_TASK * Cancel,
    _Out_ EXECUTION_CONTEXT_TASK * Stop,
    _Out_ EXECUTION_CONTEXT_NOTIFICATION * SetNotification
)
{
    *Advance = m_poll;
    *Start = m_startTask.GetUnderlyingEcTask();
    *Cancel = m_cancelTask.GetUnderlyingEcTask();
    *Stop = m_stopTask.GetUnderlyingEcTask();
    *SetNotification = m_driverNotification;
}
#endif

void
NxQueue::NotifyMorePacketsAvailable(
    void
)
{
    m_executionContext->Notify();
}

NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY const *
NxQueue::GetTxDemuxProperty(
    NET_CLIENT_QUEUE_TX_DEMUX_TYPE Type
) const
{
    switch (Type)
    {
    case NET_CLIENT_QUEUE_TX_DEMUX_TYPE::TxDemuxTypePeerAddress:
        return &m_demuxPeerAddressProperty;
    case NET_CLIENT_QUEUE_TX_DEMUX_TYPE::TxDemuxTypeWmmInfo:
        return &m_demuxWmmProperty;
    case NET_CLIENT_QUEUE_TX_DEMUX_TYPE::TxDemuxType8021p:
        return &m_demux8021pProperty;
    default:
        return nullptr;
    }
}
