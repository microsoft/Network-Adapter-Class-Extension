// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"
#include "NxQueue.tmh"

#define DEFAULT_RING_BUFFER_SIZE 128

template <typename TNxQueue, typename TConfig>
NTSTATUS
CreateNetQueueCommon(
    _In_     QUEUE_CREATION_CONTEXT      *InitContext,
    _Inout_  NXQUEUE_CREATION_PARAMETERS *QueueParameters,
    _In_     WDF_OBJECT_ATTRIBUTES       *PrivateQueueAttributes,
    _In_opt_ WDF_OBJECT_ATTRIBUTES       *ClientQueueAttributes,
    _In_     TConfig                     *Config,
    _Out_    TNxQueue                   **QueueHandle
    )
{
    if (Config->ContextTypeInfo && Config->ContextTypeInfo->ContextSize > MAXULONG)
        return STATUS_INVALID_PARAMETER;

    unique_wdf_object<WDFOBJECT> queueHandle;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(PrivateQueueAttributes, &queueHandle),
        "WdfObjectCreate for NetQueue failed.");

    TNxQueue *nxQueue =
        new (TNxQueue::_FromFxBaseObject(queueHandle.get()))
        TNxQueue (queueHandle.get(), InitContext->Adapter, InitContext->AppQueue);

    if (ClientQueueAttributes != WDF_NO_OBJECT_ATTRIBUTES)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfObjectAllocateContext(nxQueue->GetWdfObject(), ClientQueueAttributes, NULL),
            "Failed to allocate client context. NxQueue=%p", nxQueue);
    }

    nxQueue->Initialize(Config);

    QueueParameters->OSReserved2_1 = nxQueue;
    QueueParameters->RingBufferSize = DEFAULT_RING_BUFFER_SIZE;

    if (Config->ContextTypeInfo)
    {
        nxQueue->m_clientContext = Config->ContextTypeInfo;
        QueueParameters->ClientContextSize = static_cast<ULONG>(Config->ContextTypeInfo->ContextSize);
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        InitContext->AppQueue->Initialize(
            InitContext->Adapter->m_PacketClient.get(),
            QueueParameters,
            &nxQueue->m_info),
        "Failed to initialize app queue. NxQueue=%p", nxQueue);

    InitContext->CreatedQueueObject = wistd::move(queueHandle);
    *QueueHandle = nxQueue;

    FuncExit(FLAG_ADAPTER);
    return STATUS_SUCCESS;
}

wistd::unique_ptr<INxQueueAllocation>
NxQueue::AllocatePacketBuffer(size_t size)
{
    NT_ASSERT(size != 0);
    NT_ASSERT(AllocationSize != 0);

    auto const alignedSize = ALIGN_UP_BY(size, AlignmentRequirement);

    unique_wdf_memory memory;
    auto memoryCreateStatus =
        CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES, NonPagedPoolNx, 'AQxN', alignedSize, &memory, nullptr),
            "Failed to allocate memory");

    if (!NT_SUCCESS(memoryCreateStatus))
    {
        return nullptr;
    }

    return wil::make_unique_nothrow<NxPoolAllocation>(wistd::move(memory), AlignmentRequirement);
}

NTSTATUS
NxTxQueue::_Create(
    _In_     QUEUE_CREATION_CONTEXT  *InitContext,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES   QueueAttributes,
    _In_     PNET_TXQUEUE_CONFIG      Config
)
{
    FuncEntry(FLAG_ADAPTER);

    WDF_OBJECT_ATTRIBUTES queueBlockAttributes;
    CFX_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueBlockAttributes, NxTxQueue);
    queueBlockAttributes.ParentObject = InitContext->Adapter->GetFxObject();

    NXQUEUE_CREATION_PARAMETERS queueParameters;

    NxTxQueue* txQueue;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CreateNetQueueCommon(
            InitContext,
            &queueParameters,
            &queueBlockAttributes,
            QueueAttributes,
            Config,
            &txQueue),
        "Failed to create Tx queue.");

    FuncExit(FLAG_ADAPTER);
    return STATUS_SUCCESS;
}

NTSTATUS
NxRxQueue::_Create(
    _In_     QUEUE_CREATION_CONTEXT  *InitContext,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES   QueueAttributes,
    _In_     PNET_RXQUEUE_CONFIG      Config
)
{
    FuncEntry(FLAG_ADAPTER);

    WDF_OBJECT_ATTRIBUTES queueBlockAttributes;
    CFX_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueBlockAttributes, NxRxQueue);
    queueBlockAttributes.ParentObject = InitContext->Adapter->GetFxObject();

    NXQUEUE_CREATION_PARAMETERS queueParameters;
    queueParameters.AllocationSize = Config->AllocationSize;
    queueParameters.AlignmentRequirement = Config->AlignmentRequirement != NET_RX_QUEUE_DEFAULT_ALIGNMENT ?
        Config->AlignmentRequirement :
        MEMORY_ALLOCATION_ALIGNMENT - 1;

    NxRxQueue* rxQueue;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CreateNetQueueCommon(
            InitContext,
            &queueParameters,
            &queueBlockAttributes,
            QueueAttributes,
            Config,
            &rxQueue),
        "Failed to create Rx queue.");

    rxQueue->AllocationSize = queueParameters.AllocationSize;
    rxQueue->AlignmentRequirement = queueParameters.AlignmentRequirement;

    FuncExit(FLAG_ADAPTER);
    return STATUS_SUCCESS;
}

NTSTATUS
NxRxQueue::ConfigureDmaAllocator(
    _In_ WDFDMAENABLER Enabler
)
{
    FuncEntry(FLAG_ADAPTER);

    CX_RETURN_NTSTATUS_IF(STATUS_ALREADY_REGISTERED, m_dmaEnabler);

    WdfObjectReference(Enabler);
    m_dmaEnabler.reset(Enabler);

    auto const deviceAlignment = WdfDeviceGetAlignmentRequirement(Adapter->GetDevice());

    AlignmentRequirement = max(AlignmentRequirement, deviceAlignment);

    FuncExit(FLAG_ADAPTER);
    return STATUS_SUCCESS;
}

wistd::unique_ptr<INxQueueAllocation>
NxRxQueue::AllocatePacketBuffer(size_t size)
{
    NT_ASSERT(size != 0);
    NT_ASSERT(AllocationSize != 0);

    if (!m_dmaEnabler)
    {
        return NxQueue::AllocatePacketBuffer(size);
    }

    WDF_COMMON_BUFFER_CONFIG commonBufferConfig;
    WDF_COMMON_BUFFER_CONFIG_INIT(&commonBufferConfig, AlignmentRequirement);

    unique_wdf_common_buffer commonBuffer;
    auto memoryCreateStatus =
        CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            WdfCommonBufferCreateWithConfig(
                m_dmaEnabler.get(),
                size,
                &commonBufferConfig,
                WDF_NO_OBJECT_ATTRIBUTES,
                &commonBuffer),
            "Failed to allocate memory. Size=%Iu", size);

    if (!NT_SUCCESS(memoryCreateStatus))
    {
        return nullptr;
    }

    return wil::make_unique_nothrow<NxCommonBufferAllocation>(wistd::move(commonBuffer));
}

