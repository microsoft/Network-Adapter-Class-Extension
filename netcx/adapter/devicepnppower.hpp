// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <kwaitevent.h>
#include <kspinlock.h>
#include <smfx.h>

#include "DevicePnpPowerStateMachine.h"
#include "AdapterPnpPower.hpp"
#ifdef _KERNEL_MODE
#include "NxBufferQueue.hpp"
#endif
#include "NxCollection.hpp"

class NxDevice;
class NxAdapter;
class NxAdapterCollection;

typedef void (AdapterPnpPower::*AdapterOperation)();
#ifdef _KERNEL_MODE
typedef void (NxBufferQueue::*BufferQueueOperation)();
#endif

class DevicePnpPower : private DevicePnpPowerStateMachine<DevicePnpPower>
{

    inline static void *
        Tag = reinterpret_cast<void *>('PnPD');

public:

    _IRQL_requires_max_(PASSIVE_LEVEL)
    DevicePnpPower(
        _In_ NxAdapterCollection & AdapterCollection
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    ~DevicePnpPower(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        _In_ NxDevice const * Device
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    AdapterCreated(
        _In_ NxAdapter * Adapter
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    AdapterDeleted(
        _In_ NxAdapter * Adapter
    );

#ifdef _KERNEL_MODE
    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    BufferQueueCreated(
        _In_ NxBufferQueue * BufferQueue
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    BufferQueueDestroyed(
        _In_ NxBufferQueue * BufferQueue
    );
#endif

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    Start(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    Stop(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    ChangePowerState(
        _In_ WDF_POWER_DEVICE_STATE TargetPowerState
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    SurpriseRemoved(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    Cleanup(
        void
    );

private:

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    ProcessNewAdapters(
        _In_ AdapterOperation Operation = nullptr
    );

#ifdef _KERNEL_MODE
    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    ProcessNewBufferQueues(
        _In_ BufferQueueOperation Operation = nullptr
    );
#endif

private:

    NxAdapterCollection &
        m_adapterCollection;

#ifdef _KERNEL_MODE
    NxCollection<NxBufferQueue>
        m_bufferQueueCollection;
#endif

    KAutoEvent
        m_wdfEventProcessed;

    //
    // List of adapters newly created on top of this device
    //

    KSpinLock
        m_lock;

    _Guarded_by_(m_lock)
    LIST_ENTRY
        m_newAdaptersList;

    _Guarded_by_(m_lock)
    LIST_ENTRY
        m_newBufferQueueList;

private:

    //
    // Below are the definitions of the state machine entry functions. We declare the base state machine class
    // as friend to avoid making the entry functions visible to users of this class
    //

    friend class
        DevicePnpPowerStateMachine<DevicePnpPower>;

    AsyncOperationDispatch
        AdapterCreatedNoEventsToPlay;

    AsyncOperationDispatch
        AdapterCreatedPlayPowerUp;

    AsyncOperationDispatch
        AdapterCreatedPlaySurpriseRemove;

    AsyncOperationDispatch
        PoweringUp;

    AsyncOperationDispatch
        PoweringDownD1;

    AsyncOperationDispatch
        PoweringDownD2;

    AsyncOperationDispatch
        PoweringDownD3;

    AsyncOperationDispatch
        PoweringDownD3Final;

    AsyncOperationDispatch
        Stopping;

    AsyncOperationDispatch
        WdfEventProcessed;

    AsyncOperationDispatch
        Restarting;

    AsyncOperationDispatch
        SurpriseRemoving;

    AsyncOperationDispatch
        Removed;

};
