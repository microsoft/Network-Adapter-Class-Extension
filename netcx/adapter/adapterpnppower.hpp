// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>
#include <kwaitevent.h>
#include <kspinlock.h>
#include <smfx.h>

#include "AdapterPnpPowerStateMachine.h"
#include "NxUtility.hpp"
#include "NxAdapterExtension.hpp"

class NxAdapter;

class AdapterPnpPower : public AdapterPnpPowerStateMachine<AdapterPnpPower>
{
private:

    //
    // This structure is used to keep the desired state for each driver that is allowed to
    // invoke NetAdapterStart and NetAdapterStop. It does not need to be visible outside
    // of the parent class.
    //
    // Changes to this structure are synchronized by the state machine

    struct DriverState : public NONPAGED_OBJECT<'DmSA'>
    {
        WDFDRIVER
            Driver = WDF_NO_HANDLE;

        // True if the state machine is currently processing a NetAdapterStart from this driver
        bool
            StartInProgress = false;

        // True if the driver successfully called NetAdapterStart
        bool
            Started = false;

        // This is kept for diagnostic only
        NTSTATUS
            LastStartResult = -1;
    };

    struct NetAdapterStartContext
    {
        LIST_ENTRY
            Entry{};

        DriverState *
            Requestor{ nullptr };

        NTSTATUS
            Result{ STATUS_UNSUCCESSFUL };

        KAutoEvent
            Processed{};
    };

    struct NetAdapterStopContext
    {
        LIST_ENTRY
            Entry{};

        DriverState *
            Requestor{ nullptr };

        KAutoEvent
            Processed;
    };

    using unique_driver_state_array = wil::unique_any_array_ptr<DriverState, unique_ptr_array_deleter>;

public:

    _IRQL_requires_max_(PASSIVE_LEVEL)
    AdapterPnpPower(
        _In_ NxAdapter * Adapter
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ~AdapterPnpPower(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    bool
    DriverStartedAdapter(
        _In_ NX_PRIVATE_GLOBALS const * RequestorGlobals
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    NetAdapterStart(
        _In_ NX_PRIVATE_GLOBALS const * RequestorGlobals
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    NetAdapterStart(
        _In_ WDFDRIVER RequestorDriver
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    NetAdapterStop(
        _In_ NX_PRIVATE_GLOBALS const * RequestorGlobals
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    NetAdapterStop(
        _In_ WDFDRIVER RequestorDriver
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    IoStart(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    IoStop(
        _In_ WDF_POWER_DEVICE_STATE TargetPowerState
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    IoInterfaceArrived(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    SurpriseRemoved(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    PnPRebalance(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Cleanup(
        void
    );

private:

    _IRQL_requires_max_(PASSIVE_LEVEL)
    DriverState *
    GetDriverState(
        _In_ WDFDRIVER Driver
    );

private:

    NxAdapter &
        m_adapter;

    WDFTIMER
        m_initializePowerManagementTimer = WDF_NO_HANDLE;

    unique_driver_state_array
        m_driverState;

    NetAdapterStartContext *
        m_netAdapterStartContext = nullptr;

    NetAdapterStopContext *
        m_netAdapterStopContext = nullptr;

    KSpinLock
        m_lock;

    _Guarded_by_(m_lock)
    LIST_ENTRY
        m_startRequests;

    _Guarded_by_(m_lock)
    LIST_ENTRY
        m_stopRequests;

    KAutoEvent
        m_ioEventProcessed;

private:

    friend class
        AdapterPnpPowerStateMachine<AdapterPnpPower>;

    SyncOperationPassive
        ShouldStartNetworkInterface;

    AsyncOperationDispatch
        DeferStartNetworkInterface;

    SyncOperationPassive
        StartNetworkInterface;

    AsyncOperationDispatch
        StartNetworkInterfaceInvalidDeviceState;

    AsyncOperationPassive
        StopNetworkInterface;

    AsyncOperationDispatch
        StopNetworkInterfaceNoOp;

    AsyncOperationDispatch
        DequeueNetAdapterStart;

    AsyncOperationDispatch
        CompleteNetAdapterStart;

    AsyncOperationDispatch
        DequeueNetAdapterStop;

    AsyncOperationDispatch
        CompleteNetAdapterStop;

    AsyncOperationPassive
        StartIo;

    AsyncOperationPassive
        StartIoD0;

    AsyncOperationPassive
        StopIoD1;

    AsyncOperationPassive
        StopIoD2;

    AsyncOperationPassive
        StopIoD3;

    AsyncOperationPassive
        StopIoD3Final;

    AsyncOperationDispatch
        IoEventProcessed;

    AsyncOperationPassive
        AllowBindings;

    AsyncOperationPassive
        ReportSurpriseRemoval;

    AsyncOperationDispatch
        DeviceReleased;

    AsyncOperationDispatch
        Deleted;

    EvtLogTransitionFunc
        EvtLogTransition;

};
