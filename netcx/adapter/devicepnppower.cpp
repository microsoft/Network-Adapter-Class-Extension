// Copyright (C) Microsoft Corporation. All rights reserved.
#include "Nx.hpp"
#include "DevicePnpPower.hpp"
#include "NxDevice.hpp"
#include "NxAdapter.hpp"
#include "NxPrivateGlobals.hpp"

#include "DevicePnpPower.tmh"

#define SMENGINE_TAG 'tSeD'

struct NEW_ADAPTER
{
    NxAdapter *
        Adapter;

    KWaitEvent
        Event;

    LIST_ENTRY
        Linkage;
};

_Use_decl_annotations_
DevicePnpPower::DevicePnpPower(
    NxAdapterCollection & AdapterCollection
)
    : m_adapterCollection(AdapterCollection)
{
    InitializeListHead(&m_newAdaptersList);
    InitializeListHead(&m_newBufferQueueList);
}

_Use_decl_annotations_
DevicePnpPower::~DevicePnpPower(
    void
)
{
    if (IsInitialized())
    {
        EnqueueEvent(Event::Destroy);
    }
}

_Use_decl_annotations_
NTSTATUS
DevicePnpPower::Initialize(
    NxDevice const * Device
)
{
#ifdef _KERNEL_MODE
    StateMachineEngineConfig smConfig(WdfDeviceWdmGetDeviceObject(Device->GetFxObject()), SMENGINE_TAG);
#else
    UNREFERENCED_PARAMETER(Device);
    StateMachineEngineConfig smConfig;
#endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        DevicePnpPowerStateMachine::Initialize(smConfig),
        "Failed to initialize DevicePnpPower state machine");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
DevicePnpPower::AdapterCreated(
    NxAdapter * Adapter
)
{
    NEW_ADAPTER newAdapter{ Adapter };

    KAcquireSpinLock lock{ m_lock };
    InsertTailList(&m_newAdaptersList, &newAdapter.Linkage);
    lock.Release();

    EnqueueEvent(Event::AdapterCreated);

    newAdapter.Event.Wait();
}

_Use_decl_annotations_
void
DevicePnpPower::AdapterDeleted(
    NxAdapter * Adapter
)
{
    m_adapterCollection.Remove(Adapter);
}

_Use_decl_annotations_
void
DevicePnpPower::BufferQueueCreated(
    NxBufferQueue * BufferQueue
)
{
    WdfObjectReferenceWithTag(BufferQueue->GetHandle(), DevicePnpPower::Tag);

    KAcquireSpinLock lock{ m_lock };
    InsertTailList(&m_newBufferQueueList, &BufferQueue->NewEntryLink);
    lock.Release();

    EnqueueEvent(Event::AdapterCreated);
}

_Use_decl_annotations_
void
DevicePnpPower::BufferQueueDestroyed(
    NxBufferQueue * BufferQueue
)
{
    m_bufferQueueCollection.Remove(BufferQueue);
}

_Use_decl_annotations_
void
DevicePnpPower::ProcessNewAdapters(
    AdapterOperation Operation
)
{
    LIST_ENTRY *link;
    KAcquireSpinLock lock{ m_lock };

    while ((link = RemoveHeadList(&m_newAdaptersList)) != &m_newAdaptersList)
    {
        lock.Release();

        auto newAdapter = CONTAINING_RECORD(link, NEW_ADAPTER, Linkage);

        if (Operation != nullptr)
        {
            (newAdapter->Adapter->PnpPower.*Operation)();
        }

        m_adapterCollection.Add(newAdapter->Adapter);
        newAdapter->Event.Set();

        lock.Acquire();
    }
}

_Use_decl_annotations_
void
DevicePnpPower::ProcessNewBufferQueues(
    BufferQueueOperation Operation
)
{
    LIST_ENTRY *link;
    KAcquireSpinLock lock{ m_lock };

    while ((link = RemoveHeadList(&m_newBufferQueueList)) != &m_newBufferQueueList)
    {
        lock.Release();

        auto bufferQueue = CONTAINING_RECORD(link, NxBufferQueue, NewEntryLink);

        if (Operation != nullptr)
        {
            (bufferQueue->*Operation)();
        }

        m_bufferQueueCollection.Add(bufferQueue);

        WdfObjectDereferenceWithTag(bufferQueue->GetHandle(), DevicePnpPower::Tag);

        lock.Acquire();
    }
}

//
// Below are the implementations for state machine entry functions. To understand
// what is their purpose please refer to the state machine diagram.
//

_Use_decl_annotations_
void
DevicePnpPower::Start(
    void
)
{
    EnqueueEvent(Event::Start);
    m_wdfEventProcessed.Wait();
}

_Use_decl_annotations_
void
DevicePnpPower::Stop(
    void
)
{
    EnqueueEvent(Event::Stop);
    m_wdfEventProcessed.Wait();
}

_Use_decl_annotations_
void
DevicePnpPower::ChangePowerState(
    WDF_POWER_DEVICE_STATE TargetPowerState
)
{
    switch (TargetPowerState)
    {
        case WdfPowerDeviceD0:
            EnqueueEvent(Event::PowerUp);
            break;

        case WdfPowerDeviceD1:
            EnqueueEvent(Event::PowerDownD1);
            break;

        case WdfPowerDeviceD2:
            EnqueueEvent(Event::PowerDownD2);
            break;

        case WdfPowerDeviceD3:
            EnqueueEvent(Event::PowerDownD3);
            break;

        case WdfPowerDeviceD3Final:
            EnqueueEvent(Event::PowerDownD3Final);
            break;
    }

    m_wdfEventProcessed.Wait();
}

_Use_decl_annotations_
void
DevicePnpPower::SurpriseRemoved(
    void
)
{
    EnqueueEvent(Event::SurpriseRemove);
}

_Use_decl_annotations_
void
DevicePnpPower::Cleanup(
    void
)
{
    if (IsInitialized())
    {
        EnqueueEvent(Event::Cleanup);
    }
}

_Use_decl_annotations_
void
DevicePnpPower::AdapterCreatedNoEventsToPlay(
    void
)
{
    ProcessNewAdapters();
    ProcessNewBufferQueues();
}

_Use_decl_annotations_
void
DevicePnpPower::AdapterCreatedPlayPowerUp(
    void
)
{
    ProcessNewBufferQueues(&NxBufferQueue::IoStart);
    ProcessNewAdapters(&AdapterPnpPower::IoStart);
}

_Use_decl_annotations_
void
DevicePnpPower::AdapterCreatedPlaySurpriseRemove(
    void
)
{
    ProcessNewAdapters(&AdapterPnpPower::SurpriseRemoved);
    ProcessNewBufferQueues();
}

_Use_decl_annotations_
void
DevicePnpPower::PoweringUp(
    void
)
{
    m_bufferQueueCollection.ForEach([](NxBufferQueue & bufferQueue)
    {
        bufferQueue.IoStart();
    });

    m_adapterCollection.ForEach([](NxAdapter & adapter)
    {
        adapter.PnpPower.IoStart();
    });
}

_Use_decl_annotations_
void
DevicePnpPower::PoweringDownD1(
    void
)
{
    m_adapterCollection.ForEach([](NxAdapter & adapter)
    {
        adapter.PnpPower.IoStop(WdfPowerDeviceD1);
    });

    m_bufferQueueCollection.ForEach([](NxBufferQueue & bufferQueue)
    {
        bufferQueue.IoStop();
    });
}

_Use_decl_annotations_
void
DevicePnpPower::PoweringDownD2(
    void
)
{
    m_adapterCollection.ForEach([](NxAdapter & adapter)
    {
        adapter.PnpPower.IoStop(WdfPowerDeviceD2);
    });

    m_bufferQueueCollection.ForEach([](NxBufferQueue & bufferQueue)
    {
        bufferQueue.IoStop();
    });
}

_Use_decl_annotations_
void
DevicePnpPower::PoweringDownD3(
    void
)
{
    m_adapterCollection.ForEach([](NxAdapter & adapter)
    {
        adapter.PnpPower.IoStop(WdfPowerDeviceD3);
    });

    m_bufferQueueCollection.ForEach([](NxBufferQueue & bufferQueue)
    {
        bufferQueue.IoStop();
    });
}

_Use_decl_annotations_
void
DevicePnpPower::PoweringDownD3Final(
    void
)
{
    m_adapterCollection.ForEach([](NxAdapter & adapter)
    {
        adapter.PnpPower.IoStop(WdfPowerDeviceD3Final);
    });

    m_bufferQueueCollection.ForEach([](NxBufferQueue & bufferQueue)
    {
        bufferQueue.IoStop();
    });
}

_Use_decl_annotations_
void
DevicePnpPower::Stopping(
    void
)
{
    m_adapterCollection.ForEach([](NxAdapter & adapter)
    {
        adapter.PnpPower.NetAdapterStop(WdfGetDriver());
    });

    m_bufferQueueCollection.ForEach([](NxBufferQueue & bufferQueue)
    {
        bufferQueue.IoFlush();
    });
}

_Use_decl_annotations_
void
DevicePnpPower::WdfEventProcessed(
    void
)
{
    m_wdfEventProcessed.Set();
}

_Use_decl_annotations_
void
DevicePnpPower::Restarting(
    void
)
{
    m_adapterCollection.ForEach([](NxAdapter & adapter)
    {
        // First we need to get the Adapter PnP and Power state machine back to the initial state
        adapter.PnpPower.PnPRebalance();

        // The original contract for rebalance was that upon receiving EvtDevicePrepareHardware the adapter would be in stopped state
        adapter.PnpPower.NetAdapterStop(adapter.GetPrivateGlobals());

        // During device power down we stopped the adapter, we need to remove our stop constraint
        adapter.PnpPower.NetAdapterStart(WdfGetDriver());
        
    });
}

_Use_decl_annotations_
void
DevicePnpPower::SurpriseRemoving(
    void
)
{
    m_adapterCollection.ForEach([](NxAdapter & adapter)
    {
        adapter.PnpPower.SurpriseRemoved();
    });
}

_Use_decl_annotations_
void
DevicePnpPower::Removed(
    void
)
{
}
