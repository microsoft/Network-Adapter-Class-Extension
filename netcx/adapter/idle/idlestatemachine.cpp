// Copyright (C) Microsoft Corporation. All rights reserved.
#ifdef _KERNEL_MODE
#include <ntddk.h>
#define GetCurrentThread (ULONG_PTR)KeGetCurrentThread
#else
#include <windows.h>
#define ASSERT(x)
#define GetCurrentThread (ULONG_PTR)GetCurrentThreadId
#endif
#include <ntassert.h>
#include <wdf.h>
#include <NxTrace.hpp>

#include "IdleStateMachine.hpp"

#include "IdleStateMachine.tmh"

_Use_decl_annotations_
IdleStateMachine::IdleStateMachine(
    WDFDEVICE const * Device
)
    : m_device(Device)
{
    m_dxRundown.CloseAndWait();
}

_Use_decl_annotations_
void
IdleStateMachine::D0Entry(
    void
)
{
    KAcquireSpinLock lock{ m_lock };
    m_powerUpThread = GetCurrentThread();
}

_Use_decl_annotations_
void
IdleStateMachine::HardwareEnabled(
    void
)
{
    KAcquireSpinLock lock{ m_lock };
    m_hardwareEnabled.Set();
}

_IRQL_requires_(PASSIVE_LEVEL)
void
IdleStateMachine::SelfManagedIoStart(
    void
)
{
    KAcquireSpinLock lock{ m_lock };
    m_powerUpThread = 0;
}

_Use_decl_annotations_
void
IdleStateMachine::SelfManagedIoSuspend(
    void
)
{
    KAcquireSpinLock lock{ m_lock };
    m_dxRundown.Reinitialize();
}

_Use_decl_annotations_
void
IdleStateMachine::HardwareDisabled(
    bool TargetStateIsD3Final
)
{
    if (!TargetStateIsD3Final)
    {
        KAcquireSpinLock lock{ m_lock };
        m_hardwareEnabled.Clear();
    }
}

_IRQL_requires_(PASSIVE_LEVEL)
void
IdleStateMachine::D0Exit(
    void
)
{
    m_dxRundown.CloseAndWait();
}

_Use_decl_annotations_
PowerReferenceHolder
IdleStateMachine::StopIdle(
    bool WaitForD0,
    void const * Tag
)
{
    KAcquireSpinLock lock{ m_lock };

    if (m_dxRundown.TryAcquire())
    {
        // The device is powering down, we can't invoke WDF StopIdle, but we will block the power
        // transition until this reference is released
        return { m_dxRundown };
    }

    // Acquire a non-blocking WDF power reference
    auto const ntStatus = WdfDeviceStopIdleWithTag(
        *m_device,
        false,
        const_cast<void *>(Tag));

    PowerReferenceHolder powerReference{ *m_device, Tag, ntStatus };

    // We can return early if:
    //     1. StopIdle suceeded. The device is already in D0 and it is guaranteed to stay in D0
    //     2. StopIdle failed. The device will eventually be surprise removed, nothing else we can do
    //     3. The caller does not want to wait for a power up to complete
    if (ntStatus != STATUS_PENDING || !WaitForD0)
    {
        return wistd::move(powerReference);
    }

    NT_FRE_ASSERT(WaitForD0);
    NT_FRE_ASSERT(ntStatus == STATUS_PENDING);

    // If we're not in the power up thread wait until the device is back in D0
    if (m_powerUpThread != GetCurrentThread())
    {
        lock.Release();

        // We use an internal event because we don't want to wait for the full power up sequence to finish, we just
        // want the hardware to be enabled.
        m_hardwareEnabled.Wait();
    }

    return wistd::move(powerReference);
}
