// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <ntddndis_p.h>
#include "KSpinLock.h"

class NxIdleRestrictions
{
public:

    NxIdleRestrictions(NETADAPTER adapter, WDFDEVICE device);

    ~NxIdleRestrictions();

    NxIdleRestrictions(const NxIdleRestrictions&) = delete;
    NxIdleRestrictions& operator=(const NxIdleRestrictions&) = delete;

    enum class IdleRestrictionSettings
    {
        None = 0,
        HighLatencyResume,
    };

    _Requires_lock_not_held_(m_lock)
    NTSTATUS SetMinimumIdleCondition(NDIS_IDLE_CONDITION minimumIdleCondition);

    _Requires_lock_not_held_(m_lock)
    NTSTATUS SetCurrentIdleCondition(NDIS_IDLE_CONDITION idleCondition);

    _Requires_lock_not_held_(m_lock)
    void OnWdfStopIdleRefsSupported();

private:
    const NETADAPTER m_adapter;
    const WDFDEVICE m_device;

    KSpinLock m_lock;

    bool m_powerReferenceAcquired = false;

    // Represents the most restrictive NDIS_IDLE_CONDITION whose requirements can be met while
    // allowing idle power transitions. We will block idle power transitions when m_currentIdleCondition
    // is more restrictive.
    NDIS_IDLE_CONDITION m_minimumIdleCondition = NdisIdleConditionAnyLowLatency;

    // Current NDIS_IDLE_CONDITION received from WCM
    NDIS_IDLE_CONDITION m_currentIdleCondition = NdisIdleConditionAnyLowLatency;

    // Acquires or Releases power reference based on current state. Should be called whenever
    // any state relevant to idle restriction is updated.
    _Requires_lock_not_held_(m_lock)
    NTSTATUS RestrictS0IdleByIdleCondition();

    // Acquires power reference, no-op if a reference is already held.
    _Requires_lock_held_(m_lock)
    NTSTATUS AcquirePowerReference();

    // Releases power reference if one is aquired, no-op otherwise.
    _Requires_lock_held_(m_lock)
    void ReleasePowerReference();
};
