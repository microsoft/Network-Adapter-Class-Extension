// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxApp.hpp"

#define NET_RING_BUFFER_FROM_NET_PACKET(_netpacket) (NET_RING_BUFFER *)((PUCHAR)(_netpacket) - (_netpacket)->Reserved2)

#define QUEUE_CREATION_CONTEXT_SIGNATURE 0x7840dd95
struct QUEUE_CREATION_CONTEXT
{
    ULONG Signature = QUEUE_CREATION_CONTEXT_SIGNATURE;

    PKTHREAD CurrentThread = nullptr;
    INxAppQueue *AppQueue = nullptr;

    NxAdapter *Adapter = nullptr;

    ULONG QueueId = 0;

    unique_wdf_object<WDFOBJECT> CreatedQueueObject;
};

class NxQueue
{
public:

    NxQueue(
        _In_ NxAdapter *adapter,
        _In_ INxAppQueue *app) :
        Adapter(adapter),
        m_appQueue(app)
    {
        NOTHING;
    }

    virtual ~NxQueue() = default;

    void Initialize(_In_ NET_TXQUEUE_CONFIG *config)
    {
        EvtAdvance = config->EvtTxQueueAdvance;
        EvtCancel = config->EvtTxQueueCancel;
        EvtArmNotification = config->EvtTxQueueSetNotificationEnabled;
    }

    void Initialize(
        _In_ NET_RXQUEUE_CONFIG *config)
    {
#pragma warning(push)
#pragma warning(disable : 28024) // we're deliberately casting RXQUEUE functions to TXQUEUE functions
        EvtAdvance = reinterpret_cast<PFN_TXQUEUE_ADVANCE>(config->EvtRxQueueAdvance);
        EvtCancel = reinterpret_cast<PFN_TXQUEUE_CANCEL>(config->EvtRxQueueCancel);
        EvtArmNotification = reinterpret_cast<PFN_TXQUEUE_SET_NOTIFICATION_ENABLED>(config->EvtRxQueueSetNotificationEnabled);
#pragma warning(pop)
    }

    void NotifyMorePacketsAvailable()
    {
        m_appQueue->Notify();
    }

    PCNET_CONTEXT_TYPE_INFO m_clientContext = nullptr;
    NXQUEUE_CREATION_RESULT m_info;

    void Advance()
    {
        EvtAdvance((NETTXQUEUE)GetWdfObject());
    }

    void Cancel()
    {
        EvtCancel((NETTXQUEUE)GetWdfObject());
    }

    NTSTATUS SetArmed(_In_ bool isArmed)
    {
        return EvtArmNotification((NETTXQUEUE)GetWdfObject(), isArmed);
    }

    virtual wistd::unique_ptr<INxQueueAllocation> AllocatePacketBuffer(size_t size);

    ULONG GetAlignmentRequirement() const
    {
        return AlignmentRequirement;
    }

    size_t GetAllocationSize() const
    {
        return AllocationSize;
    }
protected:

    virtual WDFOBJECT GetWdfObject() = 0;
    ULONG AlignmentRequirement = MEMORY_ALLOCATION_ALIGNMENT - 1;
    size_t AllocationSize = 0;
    NxAdapter *Adapter = nullptr;

private:

    INxAppQueue *m_appQueue = nullptr;

    PFN_TXQUEUE_ADVANCE EvtAdvance = nullptr;
    PFN_TXQUEUE_CANCEL EvtCancel = nullptr;
    PFN_TXQUEUE_SET_NOTIFICATION_ENABLED EvtArmNotification = nullptr;
};

class NxTxQueue;

FORCEINLINE
NxTxQueue*
GetTxQueueFromHandle(_In_ NETTXQUEUE TxQueue);

class NxTxQueue : public NxQueue,
                  public CFxObject<NETTXQUEUE, NxTxQueue, GetTxQueueFromHandle, true>
{
public:

    NxTxQueue(
        _In_ WDFOBJECT txQueue,
        _In_ NxAdapter *adapter,
        _In_ INxAppQueue *app) : 
        CFxObject((NETTXQUEUE)txQueue),
        NxQueue(adapter, app)
    {
        NOTHING;
    }

    virtual ~NxTxQueue() = default;

    virtual WDFOBJECT GetWdfObject() { return GetFxObject(); }

    static
    NTSTATUS
    _Create(
        _In_     QUEUE_CREATION_CONTEXT  *InitContext,
        _In_opt_ PWDF_OBJECT_ATTRIBUTES   QueueAttributes,
        _In_     PNET_TXQUEUE_CONFIG      Config
        );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxTxQueue, _GetTxQueueFromHandle);

FORCEINLINE
NxTxQueue*
GetTxQueueFromHandle(NETTXQUEUE TxQueue)
{
    return _GetTxQueueFromHandle(TxQueue);
}

class NxRxQueue;

FORCEINLINE
NxRxQueue*
GetRxQueueFromHandle(_In_ NETRXQUEUE TxQueue);

class NxRxQueue : public NxQueue,
                  public CFxObject<NETRXQUEUE, NxRxQueue, GetRxQueueFromHandle, true>
{
    unique_wdf_reference<WDFDMAENABLER> m_dmaEnabler;
public:

    NxRxQueue(
        _In_ WDFOBJECT rxQueue,
        _In_ NxAdapter *adapter,
        _In_ INxAppQueue *app) :
        CFxObject((NETRXQUEUE)rxQueue),
        NxQueue(adapter, app)
    {
        NOTHING;
    }

    virtual ~NxRxQueue() = default;

    virtual WDFOBJECT GetWdfObject() { return GetFxObject(); }

    static
    NTSTATUS
    _Create(
        _In_     QUEUE_CREATION_CONTEXT  *InitContext,
        _In_opt_ PWDF_OBJECT_ATTRIBUTES   QueueAttributes,
        _In_     PNET_RXQUEUE_CONFIG      Config
        );

    NTSTATUS ConfigureDmaAllocator(_In_ WDFDMAENABLER Enabler);
    wistd::unique_ptr<INxQueueAllocation> AllocatePacketBuffer(size_t size) override;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxRxQueue, _GetRxQueueFromHandle);

FORCEINLINE
NxRxQueue*
GetRxQueueFromHandle(NETRXQUEUE TxQueue)
{
    return _GetRxQueueFromHandle(TxQueue);
}

class NxAdapterQueue : public INxAdapterQueue
{
    unique_wdf_object<WDFOBJECT> m_queueObject;
    NxQueue & m_queue;

public:
    NxAdapterQueue(
        unique_wdf_object<WDFOBJECT>&& queueObject,
        NxQueue & queue) :
        m_queueObject(wistd::move(queueObject)),
        m_queue(queue)
    {

    }
    //
    // INxAdapterQueue
    //

    void Advance() override
    {
        m_queue.Advance();
    }

    void Cancel() override
    {
        m_queue.Cancel();
    }

    NTSTATUS SetArmed(_In_ bool isArmed) override
    {
        return m_queue.SetArmed(isArmed);
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    INxQueueAllocation *AllocatePacketBuffer(size_t size) override
    {
        return m_queue.AllocatePacketBuffer(size).release();
    }

    ULONG GetAlignmentRequirement() override
    {
        return m_queue.GetAlignmentRequirement();
    }

    size_t GetAllocationSize() override
    {
        return m_queue.GetAllocationSize();
    }
};
