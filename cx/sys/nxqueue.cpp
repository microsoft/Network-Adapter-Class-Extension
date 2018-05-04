// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include "NxQueue.hpp"
#include "NxQueue.tmh"

#include <NetRingBuffer.h>
#include <NetPacket.h>

ULONG
NetClientQueueGetQueueId(
    NET_CLIENT_QUEUE Queue
    )
{
    return reinterpret_cast<NxQueue *>(Queue)->m_queueId;
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
SIZE_T
NetClientQueueGetPacketExtensionOffset(
    NET_CLIENT_QUEUE Queue,
    NET_CLIENT_PACKET_EXTENSION const * ExtensionToQuery
    )
{
    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = ExtensionToQuery->Name;
    extensionPrivate.Version = ExtensionToQuery->Version;

    return reinterpret_cast<NxQueue *>(Queue)->GetPacketExtensionOffset(&extensionPrivate);
}

static
PCNET_DATAPATH_DESCRIPTOR
NetClientQueueGetDatapathDescriptor(
    _In_ NET_CLIENT_QUEUE Queue
    )
{
    return reinterpret_cast<NxQueue *>(Queue)->GetPacketRingBufferSet();
}

static const NET_CLIENT_QUEUE_DISPATCH QueueDispatch
{
    { sizeof(NET_CLIENT_QUEUE_DISPATCH) },
    &NetClientQueueGetQueueId,
    &NetClientQueueAdvance,
    &NetClientQueueCancel,
    &NetClientQueueSetArmed,
    &NetClientQueueGetPacketExtensionOffset,
    &NetClientQueueGetDatapathDescriptor,
};

NTSTATUS
GetAttributesContextSize(
    _In_ NET_PACKET_CONTEXT_ATTRIBUTES const & Attributes,
    _Out_ ULONG & AttributesContextSize
    )
{
    size_t const contextSize = Attributes.ContextSizeOverride != 0
        ? Attributes.ContextSizeOverride
        : Attributes.ContextTypeInfo->ContextSize;
    size_t const alignedContextSize = WDF_ALIGN_SIZE_UP(contextSize, MEMORY_ALLOCATION_ALIGNMENT);

    if (alignedContextSize < contextSize)
    {
        return STATUS_INTEGER_OVERFLOW;
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        RtlSizeTToULong(alignedContextSize, &AttributesContextSize),
        "Attributes context size invalid, size=%Iu", alignedContextSize);

    return STATUS_SUCCESS;
}

NTSTATUS
GetExtensionSize(
    _In_ QUEUE_CREATION_CONTEXT const & InitContext,
    _Out_ size_t & ExtensionSize
    )
{
    ULONG size = 0;
    for (auto & attributes : InitContext.PacketContextAttributes)
    {
        ULONG attributesContextSize;
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            GetAttributesContextSize(attributes, attributesContextSize),
            "Context attribute size invalid.");

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            RtlULongAdd(size, attributesContextSize, &size),
            "Extension size too large.");
    }
    ExtensionSize = size;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxQueue::NxQueue(
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NxQueue::Type QueueType
    ) :
    m_queueId(QueueId),
    m_queueType(QueueType),
    m_adapter(InitContext.Adapter),
    m_clientQueue(InitContext.QueueContexts[InitContext.QueueContextIndex].ClientQueue),
    m_clientDispatch(InitContext.ClientDispatch)
{
    NOTHING;
}

void
NxQueue::Advance(
    void
)
{
    m_evtAdvance(m_queue);
}

void
NxQueue::Cancel(
    void
)
{
    m_evtCancel(m_queue);
}

void
NxQueue::SetArmed(
    bool IsArmed
)
{
    m_evtArmNotification(m_queue, IsArmed);
}

WDFOBJECT
NxQueue::GetWdfObject(
    void
    )
{
    return m_queue;
}

_Use_decl_annotations_
NTSTATUS
NxQueue::NetQueueInitAddPacketContextAttributes(
    QUEUE_CREATION_CONTEXT * CreationContext,
    PNET_PACKET_CONTEXT_ATTRIBUTES ContextAttributes
    )
{
    if (!CreationContext->PacketContextAttributes.append(*ContextAttributes))
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
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
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        GetExtensionSize(InitContext, m_privateExtensionSize),
        "Extension Size too large.");

    CX_RETURN_IF_NOT_NT_SUCCESS(
        PrepareFragmentRingBuffer(InitContext)
        );

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

    auto const extensionSize = ALIGN_UP_BY(m_privateExtensionSize, MEMORY_ALLOCATION_ALIGNMENT);
    auto const elementSize = ALIGN_UP_BY(m_privateExtensionOffset + extensionSize, NET_PACKET_ALIGNMENT_BYTES);
    auto const ringSize = InitContext.ClientQueueConfig->NumberOfPackets * elementSize;
    auto const allocationSize = ringSize + FIELD_OFFSET(NET_RING_BUFFER, Buffer[0]);

    auto packetRingBuffer = reinterpret_cast<NET_RING_BUFFER*>(
        ExAllocatePoolWithTag(NonPagedPoolNxCacheAligned, allocationSize, 'BRxN'));
    if (! packetRingBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(packetRingBuffer, allocationSize);
    packetRingBuffer->OSReserved2[1] = this;
    packetRingBuffer->ElementStride = static_cast<USHORT>(elementSize);
    packetRingBuffer->NumberOfElements = static_cast<ULONG>(InitContext.ClientQueueConfig->NumberOfPackets);
    packetRingBuffer->ElementIndexMask = static_cast<ULONG>(InitContext.ClientQueueConfig->NumberOfPackets - 1);

    m_packetRingBuffer.reset(packetRingBuffer);
    
    NET_DATAPATH_DESCRIPTOR_GET_PACKET_RING_BUFFER(&m_descriptor) = m_packetRingBuffer.get();

    // If we have contexts on the NET_PACKETs, initialize their
    // information in the queue
    size_t numberOfContexts = InitContext.PacketContextAttributes.count();
    if (numberOfContexts > 0)
    {
        if (!m_clientContextInfo.resize(numberOfContexts))
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        // For each context type initialize a context token
        ULONG i = 0;
        ULONG offset = static_cast<ULONG>(m_privateExtensionOffset);
        for (auto& packetContextAttribs : InitContext.PacketContextAttributes)
        {
            ULONG contextSize;

            // We already made this calculation above, if we're here this should succeed
            WIN_VERIFY(NT_SUCCESS(
                GetAttributesContextSize(
                    packetContextAttribs,
                    contextSize)));

            m_clientContextInfo[i].Offset = offset;
            m_clientContextInfo[i].Size = contextSize;
            m_clientContextInfo[i].ContextTypeInfo = packetContextAttribs.ContextTypeInfo;

            NT_ASSERT(m_clientContextInfo[i].Offset + m_clientContextInfo[i].Size <= m_packetRingBuffer->ElementStride);

            i++;

            // Make sure we don't return a false positive by adding
            // the size of the last packet context type to the offset
            // when there is no next context
            if (i < numberOfContexts)
            {
                CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
                    RtlULongAdd(offset, contextSize, &offset),
                    "Failed to calculate the offset to the packet context");
            }
        }
    }

    *InitContext.AdapterDispatch = &QueueDispatch;

    FuncExit(FLAG_ADAPTER);
    return STATUS_SUCCESS;
}

NET_DATAPATH_DESCRIPTOR*
NxQueue::GetPacketRingBufferSet(
    void
    )
{
    return &m_descriptor;
}

size_t
NxQueue::GetPrivateExtensionOffset(
    void
    ) const
{
    return m_privateExtensionOffset;
}

Rtl::KArray<NET_PACKET_CONTEXT_TOKEN_INTERNAL, NonPagedPoolNx> &
NxQueue::GetClientContextInfo(
    void
    )
{
    return m_clientContextInfo;
}

_Use_decl_annotations_
NET_PACKET_CONTEXT_TOKEN *
NxQueue::GetPacketContextTokenFromTypeInfo(
    PCNET_CONTEXT_TYPE_INFO ContextTypeInfo
    )
{
    for (auto & info : m_clientContextInfo)
    {
        if (info.ContextTypeInfo == ContextTypeInfo)
        {
            return reinterpret_cast<NET_PACKET_CONTEXT_TOKEN *>(&info);
        }
    }

    return nullptr;
}

_Use_decl_annotations_
size_t
NxQueue::GetPacketExtensionOffset(
    const NET_PACKET_EXTENSION_PRIVATE* ExtensionToQuery
    )
{
    size_t assignedOffset = NET_PACKET_EXTENSION_INVALID_OFFSET;

    for (auto& extension : m_AddedPacketExtensions)
    {
        if ((0 == wcscmp(extension.Name, ExtensionToQuery->Name)) &&
            (extension.Version >= ExtensionToQuery->Version))
        {
            assignedOffset = extension.AssignedOffset;
            break;
        }
    }

    return assignedOffset;
}

_Use_decl_annotations_
NxTxQueue::NxTxQueue(
    WDFOBJECT Object,
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NET_TXQUEUE_CONFIG const & QueueConfig
    ) :
    CFxObject(static_cast<NETTXQUEUE>(Object)),
    NxQueue(InitContext, QueueId, NxQueue::Type::Tx)
{
    m_queue = static_cast<NETTXQUEUE>(Object);

    m_evtAdvance = QueueConfig.EvtTxQueueAdvance;
    m_evtCancel = QueueConfig.EvtTxQueueCancel;
    m_evtArmNotification = QueueConfig.EvtTxQueueSetNotificationEnabled;
}

_Use_decl_annotations_
NTSTATUS
NxTxQueue::Initialize(
    QUEUE_CREATION_CONTEXT & InitContext
    )
{
    FuncEntry(FLAG_ADAPTER);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PrepareAndStorePacketExtensions(InitContext),
        "Failed to consolidate extensions");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxQueue::Initialize(InitContext),
        "Failed to create Tx queue.");

    FuncExit(FLAG_ADAPTER);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxRxQueue::NxRxQueue(
    WDFOBJECT Object,
    QUEUE_CREATION_CONTEXT const & InitContext,
    ULONG QueueId,
    NET_RXQUEUE_CONFIG const & QueueConfig
    ) :
    CFxObject(static_cast<NETRXQUEUE>(Object)),
    NxQueue(InitContext, QueueId, NxQueue::Type::Rx)
{
    m_queue = reinterpret_cast<NETTXQUEUE>(Object);

#pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH, "annotation from different declaration")
    m_evtAdvance = reinterpret_cast<PFN_TXQUEUE_ADVANCE>(QueueConfig.EvtRxQueueAdvance);
#pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH, "annotation from different declaration")
    m_evtCancel = reinterpret_cast<PFN_TXQUEUE_CANCEL>(QueueConfig.EvtRxQueueCancel);
#pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH, "annotation from different declaration")
    m_evtArmNotification =
        reinterpret_cast<PFN_TXQUEUE_SET_NOTIFICATION_ENABLED>(QueueConfig.EvtRxQueueSetNotificationEnabled);
}

_Use_decl_annotations_
NTSTATUS
NxRxQueue::Initialize(
    QUEUE_CREATION_CONTEXT & InitContext
    )
{
    FuncEntry(FLAG_ADAPTER);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PrepareAndStorePacketExtensions(InitContext),
        "Failed to consolidate extensions");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxQueue::Initialize(InitContext),
        "Failed to create Rx queue.");

    FuncExit(FLAG_ADAPTER);
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
NxQueue::PrepareFragmentRingBuffer(QUEUE_CREATION_CONTEXT & InitContext)
{
    auto const numberOfFragments = InitContext.ClientQueueConfig->NumberOfFragments;
    auto const elementSize = ALIGN_UP_BY(sizeof(NET_PACKET_FRAGMENT), NET_PACKET_FRAGMENT_ALIGNMENT_BYTES);
    auto const ringSize = numberOfFragments * elementSize ;
    auto const allocationSize = ringSize + FIELD_OFFSET(NET_RING_BUFFER, Buffer[0]);

    auto fragmentRingBuffer = reinterpret_cast<NET_RING_BUFFER*>(
        ExAllocatePoolWithTag(NonPagedPoolNxCacheAligned, allocationSize, 'BRxN'));
    
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !fragmentRingBuffer);

    RtlZeroMemory(fragmentRingBuffer, allocationSize);
    fragmentRingBuffer->OSReserved2[1] = this;
    fragmentRingBuffer->ElementStride = static_cast<USHORT>(elementSize);
    fragmentRingBuffer->NumberOfElements = static_cast<ULONG>(numberOfFragments);
    fragmentRingBuffer->ElementIndexMask = static_cast<ULONG>(numberOfFragments - 1);

    m_fragmentRingBuffer.reset(fragmentRingBuffer);

    NET_DATAPATH_DESCRIPTOR_GET_FRAGMENT_RING_BUFFER(&m_descriptor) = m_fragmentRingBuffer.get();

    return STATUS_SUCCESS;
}

