// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <kptr.h>
#include <kwaitevent.h>
#include <net/extension.h>
#include <net/ring.h>
#include <NetClientBuffer.h>
#include <NxCollection.hpp>

#include "BufferPool.hpp"
#include "ExecutionContext.hpp"
#include "ExecutionContextTask.hpp"
#include "IBufferVectorAllocator.hpp"
#include "extension/NxExtensionLayout.hpp"

class NxBufferQueue
{
    friend class
        NxCollection<NxBufferQueue>;

public:

    // Only used while the creation event of this object is pending in the DevicePnpPower state machine
    LIST_ENTRY
        NewEntryLink = {};

    _IRQL_requires_(PASSIVE_LEVEL)
    NxBufferQueue(
        _In_ NETEXECUTIONCONTEXT ExecutionContext,
        _In_ NET_BUFFER_QUEUE_CONFIG const * Config
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    ~NxBufferQueue(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    GetExtension(
        _In_ PCWSTR Name,
        _In_ UINT32 Version,
        _In_ NET_EXTENSION_TYPE Type,
        _Out_ NET_EXTENSION * Extension
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_RING *
    GetRing(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NETBUFFERQUEUE
    GetHandle(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    IoStart(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    IoStop(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    IoFlush(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    PostBuffers(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    InvokePostAndDrain(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    InvokeReturnBuffers(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG
    ReclaimBuffers(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    IsFlushComplete(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    InvokeCancel(
        void
    );

private:

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CreateRing(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    InitializeAllocator(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    InitializeBufferPool(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    InitializeExecutionContext(
        void
    );

private:

    NETBUFFERQUEUE const
        m_handle;

    WDFDEVICE const
        m_device;

    LIST_ENTRY
        m_linkage = {};

    NET_BUFFER_QUEUE_CONFIG
        m_config = {};

    NET_ADAPTER_DMA_CAPABILITIES
        m_dmaCapabilities = {};

    NxExtensionLayout
        m_bufferLayout;

    KPoolPtrNP<NET_RING>
        m_ring;

    ExecutionContext * const
        m_executionContext;

    EXECUTION_CONTEXT_POLL
        m_postBuffersPoll;

    EXECUTION_CONTEXT_POLL
        m_reclaimBuffersPoll;

    ExecutionContextTask
        m_cancelTask;

    KAutoEvent
        m_flushed;

    wistd::unique_ptr<IBufferVectorAllocator>
        m_allocator;

    wistd::unique_ptr<BufferPool>
        m_bufferPool;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxBufferQueue, GetNxBufferQueueFromHandle);
