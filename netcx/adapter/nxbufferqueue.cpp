// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"
#include "NxBufferQueue.hpp"
#include "NxDevice.hpp"
#include "NxExecutionContext.hpp"

#include <net/logicaladdress_p.h>
#include <net/virtualaddress_p.h>

#include "DmaAllocator.hpp"
#include "NonPagedAllocator.hpp"

#include "NxBufferQueue.tmh"

static
EVT_EXECUTION_CONTEXT_POLL
    EvtExecutionContextPostBuffersStopped;

EVT_EXECUTION_CONTEXT_POLL
    EvtExecutionContextPostBuffersStarted;

EVT_EXECUTION_CONTEXT_POLL
    EvtExecutionContextPostBuffersFlushing;

static
EVT_EXECUTION_CONTEXT_POLL
    EvtExecutionContextReclaimBuffersStopped;

EVT_EXECUTION_CONTEXT_POLL
    EvtExecutionContextReclaimBuffersStarted;

EVT_EXECUTION_CONTEXT_POLL
    EvtExecutionContextReclaimBuffersFlushing;

static
EVT_EXECUTION_CONTEXT_TASK
    EvtTaskCancelBuffers;

_Use_decl_annotations_
NxBufferQueue::NxBufferQueue(
    NETEXECUTIONCONTEXT ExecutionContext,
    NET_BUFFER_QUEUE_CONFIG const * Config
)
    : m_handle(static_cast<NETBUFFERQUEUE>(WdfObjectContextGetObject(this)))
    , m_device(GetExecutionContextFromHandle(ExecutionContext)->GetDevice())
    , m_bufferLayout(sizeof(HANDLE), alignof(HANDLE))
    , m_executionContext(GetExecutionContextFromHandle(ExecutionContext))
    , m_cancelTask(this, EvtTaskCancelBuffers)
{
    RtlCopyMemory(
        &m_config,
        Config,
        Config->Size);

    if (m_config.DmaCapabilities != nullptr)
    {
        RtlCopyMemory(
            &m_dmaCapabilities,
            Config->DmaCapabilities,
            Config->DmaCapabilities->Size);

        m_config.DmaCapabilities = &m_dmaCapabilities;
    }

    INITIALIZE_EXECUTION_CONTEXT_POLL(
        &m_postBuffersPoll,
        ExecutionContextPollTypePreAdvance,
        this,
        EvtExecutionContextPostBuffersStopped);

    m_executionContext->RegisterPoll(
        &m_postBuffersPoll);

    INITIALIZE_EXECUTION_CONTEXT_POLL(
        &m_reclaimBuffersPoll,
        ExecutionContextPollTypePostAdvance,
        this,
        EvtExecutionContextReclaimBuffersStopped);

    m_executionContext->RegisterPoll(
        &m_reclaimBuffersPoll);
}

_Use_decl_annotations_
NxBufferQueue::~NxBufferQueue(
    void
)
{
    m_executionContext->UnregisterPoll(&m_reclaimBuffersPoll);
    m_executionContext->UnregisterPoll(&m_postBuffersPoll);

    auto device = GetNxDeviceFromHandle(m_device);
    device->PnpPower.BufferQueueDestroyed(this);
}

_Use_decl_annotations_
NTSTATUS
NxBufferQueue::CreateRing(
    void
)
{
    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_bufferLayout.PutExtension(
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
            NetExtensionTypeFragment,
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1_SIZE,
            alignof(NET_FRAGMENT_VIRTUAL_ADDRESS)));

    if (m_dmaCapabilities.Size != 0)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            m_bufferLayout.PutExtension(
                NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
                NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
                NetExtensionTypeFragment,
                NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1_SIZE,
                alignof(NET_FRAGMENT_LOGICAL_ADDRESS)));
    }

    auto const elementSize = m_bufferLayout.Generate();
    auto const elementCount = 256u;
    auto const size = elementSize * elementCount + FIELD_OFFSET(NET_RING, Buffer[0]);

    auto ring = reinterpret_cast<NET_RING *>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED | POOL_FLAG_CACHE_ALIGNED, size, 'BRxN'));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! ring);

    ring->ElementStride = static_cast<USHORT>(elementSize);
    ring->NumberOfElements = elementCount;
    ring->ElementIndexMask = elementCount - 1;

    m_ring.reset(ring);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxBufferQueue::InitializeAllocator(
    void
)
{
    if (m_config.DmaCapabilities != nullptr)
    {
        NET_CLIENT_MEMORY_CONSTRAINTS::DMA dmaConfig = {};

        dmaConfig.CacheEnabled = static_cast<NET_CLIENT_TRI_STATE> (m_config.DmaCapabilities->CacheEnabled);
        dmaConfig.MaximumPhysicalAddress = m_config.DmaCapabilities->MaximumPhysicalAddress;
        dmaConfig.PreferredNode = m_config.DmaCapabilities->PreferredNode;

        dmaConfig.DmaAdapter = WdfDmaEnablerWdmGetDmaAdapter(
            m_config.DmaCapabilities->DmaEnabler,
            WdfDmaDirectionReadFromDevice);

        m_allocator = wil::make_unique_nothrow<DmaAllocator>(dmaConfig);
    }
    else
    {
        m_allocator = wil::make_unique_nothrow<NonPagedAllocator>();
    }

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_allocator);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxBufferQueue::InitializeBufferPool(
    void
)
{
    m_bufferPool = wil::make_unique_nothrow<BufferPool>();

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_bufferPool);

    size_t const bufferCount = m_ring->NumberOfElements * 4;
    size_t const bufferSize = m_config.BufferSize;

    NODE_REQUIREMENT const nodeRequirement = m_config.DmaCapabilities != nullptr
        ? m_config.DmaCapabilities->PreferredNode
        : MM_ANY_NODE_OK;

    auto bufferVector = m_allocator->AllocateVector(
        bufferCount,
        bufferSize,
        0,
        nodeRequirement);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        bufferVector == nullptr);

    CX_RETURN_IF_NOT_NT_SUCCESS(m_bufferPool->Fill(bufferVector));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxBufferQueue::InitializeExecutionContext(
    void
)
{
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxBufferQueue::Initialize(
    void
)
{
    CX_RETURN_IF_NOT_NT_SUCCESS(CreateRing());

    CX_RETURN_IF_NOT_NT_SUCCESS(InitializeAllocator());

    CX_RETURN_IF_NOT_NT_SUCCESS(InitializeBufferPool());

    CX_RETURN_IF_NOT_NT_SUCCESS(InitializeExecutionContext());

    auto device = GetNxDeviceFromHandle(m_device);
    device->PnpPower.BufferQueueCreated(this);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxBufferQueue::IoStart(
    void
)
{
    m_executionContext->ChangePollFunction(
        &m_reclaimBuffersPoll,
        EvtExecutionContextReclaimBuffersStarted);

    m_executionContext->ChangePollFunction(
        &m_postBuffersPoll,
        EvtExecutionContextPostBuffersStarted);
}

_Use_decl_annotations_
void
NxBufferQueue::IoStop(
    void
)
{
    m_executionContext->ChangePollFunction(
        &m_postBuffersPoll,
        EvtExecutionContextPostBuffersStopped);

    m_executionContext->ChangePollFunction(
        &m_reclaimBuffersPoll,
        EvtExecutionContextReclaimBuffersStopped);
}

_Use_decl_annotations_
void
EvtTaskCancelBuffers(
    void * Context
)
{
    auto bufferQueue = static_cast<NxBufferQueue *>(Context);
    bufferQueue->InvokeCancel();

}

_Use_decl_annotations_
void
NxBufferQueue::InvokeCancel(
    void
)
{
    m_config.EvtCancel(m_handle);

    // In case the driver returned all the buffers from the cancel function. There is
    // no need to look at the return from this function in this context
    ReclaimBuffers();
    IsFlushComplete();
}

_Use_decl_annotations_
void
NxBufferQueue::IoFlush(
    void
)
{
    m_executionContext->ChangePollFunction(
        &m_postBuffersPoll,
        EvtExecutionContextPostBuffersFlushing);

    m_executionContext->ChangePollFunction(
        &m_reclaimBuffersPoll,
        EvtExecutionContextReclaimBuffersFlushing);

    m_executionContext->QueueTask(m_cancelTask);
    m_cancelTask.WaitForCompletion();

    m_flushed.Wait();

    m_executionContext->ChangePollFunction(
        &m_postBuffersPoll,
        EvtExecutionContextPostBuffersStopped);

    m_executionContext->ChangePollFunction(
        &m_reclaimBuffersPoll,
        EvtExecutionContextReclaimBuffersStopped);
}

_Use_decl_annotations_
ULONG
NxBufferQueue::PostBuffers(
    void
)
{
    auto br = m_ring.get();
    auto const lastIndex = NetRingAdvanceIndex(br, br->BeginIndex, -1);
    auto const oEndIndex = br->EndIndex;

    while (br->EndIndex != lastIndex)
    {
        SIZE_T index;
        auto const ntStatus = m_bufferPool->Allocate(&index);

        if (ntStatus != STATUS_SUCCESS)
        {
            break;
        }

        m_bufferPool->AddRef(index);
        m_bufferPool->OwnedByOs(index);

        auto handle = static_cast<SIZE_T *>(NetRingGetElementAtIndex(br, br->EndIndex));
        *handle = index;

        br->EndIndex = NetRingIncrementIndex(br, br->EndIndex);
    }

    return NetRingGetRangeCount(br, oEndIndex, br->EndIndex);
}

_Use_decl_annotations_
ULONG
NxBufferQueue::InvokePostAndDrain(
    void
)
{
    if (m_config.EvtPostAndDrain == nullptr)
    {
        return 0;
    }

    auto const oBeginIndex = m_ring->BeginIndex;
    m_config.EvtPostAndDrain(m_handle);

    return NetRingGetRangeCount(m_ring.get(), oBeginIndex, m_ring->BeginIndex);
}

_Use_decl_annotations_
void
EvtExecutionContextPostBuffersStopped(
    void *,
    EXECUTION_CONTEXT_POLL_PARAMETERS *
)
{
    // Nothing to do when the queue is stopped
}

_Use_decl_annotations_
void
EvtExecutionContextPostBuffersStarted(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    // When the queue is started we post new buffers to the ring if there is available space
    // and invoke the client driver's EvtPostAndDrain callback, if one was registered
    auto bufferQueue = static_cast<NxBufferQueue *>(Context);

    auto const newBuffersPosted = bufferQueue->PostBuffers();
    auto const ihvWork = bufferQueue->InvokePostAndDrain();

    Parameters->WorkCounter = newBuffersPosted + ihvWork;
}

_Use_decl_annotations_
void
EvtExecutionContextPostBuffersFlushing(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    // When the queue is stopping we do not post new buffers to the ring, we only invoke the
    // client drivers' EvtPostAndDrain callback, if one was provided
    auto bufferQueue = static_cast<NxBufferQueue *>(Context);
    auto const ihvWork = bufferQueue->InvokePostAndDrain();

    Parameters->WorkCounter = ihvWork;
}

_Use_decl_annotations_
ULONG
NxBufferQueue::InvokeReturnBuffers(
    void
)
{
    if (m_config.EvtReturnBuffers == nullptr)
    {
        return 0;
    }

    auto const oBeginIndex = m_ring->BeginIndex;
    m_config.EvtReturnBuffers(m_handle);

    return NetRingGetRangeCount(m_ring.get(), oBeginIndex, m_ring->BeginIndex);
}

_Use_decl_annotations_
ULONG
NxBufferQueue::ReclaimBuffers(
    void
)
{
    auto br = m_ring.get();
    auto const oOSReserved0 = br->OSReserved0;

    while (br->OSReserved0 != br->BeginIndex)
    {
        auto handle = static_cast<SIZE_T *>(NetRingGetElementAtIndex(br, br->OSReserved0));
        m_bufferPool->DeRef(*handle);

        br->OSReserved0 = NetRingIncrementIndex(br, br->OSReserved0);
    }

    return NetRingGetRangeCount(br, oOSReserved0, br->OSReserved0);
}

_Use_decl_annotations_
bool
NxBufferQueue::IsFlushComplete(
    void
)
{
    if (m_flushed.Test())
    {
        return true;
    }

    if (m_ring->BeginIndex == m_ring->EndIndex)
    {
        m_flushed.Set();
        return true;
    }

    return false;
}

_Use_decl_annotations_
void
EvtExecutionContextReclaimBuffersStopped(
    void *,
    EXECUTION_CONTEXT_POLL_PARAMETERS *
)
{
    // Nothing to do when the queue is stopped
}

_Use_decl_annotations_
void
EvtExecutionContextReclaimBuffersStarted(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    // When the queue is started we invoke the client driver's EvtReturnBuffers callback,
    // if one was provided, and reclaim buffers from the ring.
    auto bufferQueue = static_cast<NxBufferQueue *>(Context);

    auto const ihvWork = bufferQueue->InvokeReturnBuffers();
    auto const buffersReclaimed = bufferQueue->ReclaimBuffers();

    Parameters->WorkCounter = ihvWork + buffersReclaimed;
}

_Use_decl_annotations_
void
EvtExecutionContextReclaimBuffersFlushing(
    void * Context,
    EXECUTION_CONTEXT_POLL_PARAMETERS * Parameters
)
{
    // When the queue is being flushed we bail early if we determine all the resources are
    // already owned by us
    auto bufferQueue = static_cast<NxBufferQueue *>(Context);

    auto const flushed = bufferQueue->IsFlushComplete();

    if (flushed)
    {
        return;
    }

    auto const ihvWork = bufferQueue->InvokeReturnBuffers();
    auto const buffersReclaimed = bufferQueue->ReclaimBuffers();

    Parameters->WorkCounter = ihvWork + buffersReclaimed;
}

_Use_decl_annotations_
void
NxBufferQueue::GetExtension(
    PCWSTR Name,
    UINT32 Version,
    NET_EXTENSION_TYPE Type,
    NET_EXTENSION * Extension
) const
{
    auto extension = m_bufferLayout.GetExtension(
        Name,
        Version,
        Type);

    Extension->Enabled = extension != nullptr;

    if (Extension->Enabled)
    {
        auto const ring = m_ring.get();
        Extension->Reserved[0] = ring->Buffer + extension->AssignedOffset;
        Extension->Reserved[1] = reinterpret_cast<void *>(ring->ElementStride);
    }
}

_Use_decl_annotations_
NET_RING *
NxBufferQueue::GetRing(
    void
)
{
    return m_ring.get();
}

_Use_decl_annotations_
NETBUFFERQUEUE
NxBufferQueue::GetHandle(
    void
) const
{
    return m_handle;
}
