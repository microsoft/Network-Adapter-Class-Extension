// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <kspinlock.h>
#include <krundown.h>
#include <kwaitevent.h>
#include "PowerReferenceHolder.hpp"

class IdleStateMachine
{
public:

    IdleStateMachine(
        IdleStateMachine const &
    ) = delete;

    IdleStateMachine(
        IdleStateMachine &&
    ) = delete;

    _IRQL_requires_(PASSIVE_LEVEL)
    IdleStateMachine(
        _In_ WDFDEVICE const * Device
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    D0Entry(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    HardwareEnabled(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SelfManagedIoStart(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SelfManagedIoSuspend(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    HardwareDisabled(
        _In_ bool TargetStateIsD3Final
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    D0Exit(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PowerReferenceHolder
    StopIdle(
        _In_ bool WaitForD0,
        _In_ void const * Tag
    );

private:

    WDFDEVICE const * const
        m_device;

    KRundown
        m_dxRundown;

    KSpinLock
        m_lock;

    _Guarded_by_(m_lock)
    KWaitEvent
        m_hardwareEnabled;

    _Guarded_by_(m_lock)
    ULONG_PTR
        m_powerUpThread = 0;
};
