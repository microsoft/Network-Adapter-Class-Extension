// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NxIdleRestrictions.hpp"

#include "NxIdleRestrictions.tmh"

#define INITGUID
#include <guiddef.h>
#include "NxTrace.hpp"
#ifdef _KERNEL_MODE

#include <ntddk.h>
#include "wdf.h"

#endif

#define NX_IDLE_RESTRICTION_REF_TAG (PVOID) 'RIxN'

NxIdleRestrictions::NxIdleRestrictions(NETADAPTER adapter, WDFDEVICE device) :
    m_adapter(adapter)
    , m_device(device)
{
}

NxIdleRestrictions::~NxIdleRestrictions()
{
    ReleasePowerReference();
}

_Use_decl_annotations_
NTSTATUS NxIdleRestrictions::SetMinimumIdleCondition(NDIS_IDLE_CONDITION minimumIdleCondition)
{
    LogInfo(FLAG_POWER,
        "%!FUNC! Setting Min Idle Condition for S0 Idle on NETADAPTER: %p to %!NdisIdleCondition! (from %!NdisIdleCondition!)",
        m_adapter,
        minimumIdleCondition,
        m_minimumIdleCondition);

    m_minimumIdleCondition = minimumIdleCondition;

    return RestrictS0IdleByIdleCondition();
}

_Use_decl_annotations_
NTSTATUS
NxIdleRestrictions::SetCurrentIdleCondition(NDIS_IDLE_CONDITION idleCondition)
{
    LogInfo(FLAG_POWER,
        "%!FUNC! Setting Idle Condition from %!NdisIdleCondition! to %!NdisIdleCondition!",
        m_currentIdleCondition,
        idleCondition);

    m_currentIdleCondition = idleCondition;

    return RestrictS0IdleByIdleCondition();
}

_Use_decl_annotations_
void
NxIdleRestrictions::OnWdfStopIdleRefsSupported(
    void
)
{
    LogInfo(FLAG_POWER,
        "%!FUNC! Re-evaluating Stop Idle refs on WDFDEVICE: %p for NETADAPTER: %p now that refs are supported.",
        m_device,
        m_adapter);

    // Previous attempts to grab stop idle references may have failed. Now that we are able to, attempt to
    // the references if they are needed.
    RestrictS0IdleByIdleCondition();
}

_Use_decl_annotations_
NTSTATUS
NxIdleRestrictions::RestrictS0IdleByIdleCondition()
{
    KAcquireSpinLock lock { m_lock };

    NTSTATUS status = STATUS_SUCCESS;

    // Larger Idle Conditions define less restrictive resume requirements. m_minimumIdleCondition is the most restrictive
    // idle condition whose requirements we can meet while still idling to Dx.
    // So, if m_currentIdleCondition >= (not more restrictive than) m_minimumIdleCondition, release the power ref to allow
    // Idle power transitions. Otherwise, if m_currentIdleCondition < (more restrictive) than m_minimumIdleCondition,
    // grab a power reference to prevent Idling to Dx to make sure we can meet the requirements of the current condition.
    if (m_currentIdleCondition >= m_minimumIdleCondition)
    {
        if (m_powerReferenceAcquired)
        {
            LogInfo(FLAG_POWER,
                "%!FUNC! Allowing S0Idle on WDFDEVICE: %p for NETADAPTER: %p. Minimum Idle Condition: "
                "%!NdisIdleCondition! (%u), Current Idle Condition: %!NdisIdleCondition! (%u)",
                m_device,
                m_adapter,
                m_minimumIdleCondition,
                m_minimumIdleCondition,
                m_currentIdleCondition,
                m_currentIdleCondition);

            ReleasePowerReference();
        }
    }
    else
    {
        if (!m_powerReferenceAcquired)
        {
            LogInfo(FLAG_POWER,
                "%!FUNC! Preventing S0Idle on WDFDEVICE: %p for NETADAPTER: %p. Minimum Idle Condition: "
                "%!NdisIdleCondition! (%u), Current Idle Condition: %!NdisIdleCondition! (%u)",
                m_device,
                m_adapter,
                m_minimumIdleCondition,
                m_minimumIdleCondition,
                m_currentIdleCondition,
                m_currentIdleCondition);

            status = AcquirePowerReference();
        }
    }
    return status;
}

#ifdef _KERNEL_MODE

_Use_decl_annotations_
NTSTATUS
NxIdleRestrictions::AcquirePowerReference()
{
    if (m_powerReferenceAcquired)
    {
        LogInfo(FLAG_POWER,
            "%!FUNC! Already holding StopIdle reference on WDFDEVICE: %p for NETADAPTER: %p, returning.",
            m_device,
            m_adapter);

        return STATUS_SUCCESS;
    }

    const auto powerReferenceStatus = WdfDeviceStopIdleWithTag(
        m_device,
        false,
        NX_IDLE_RESTRICTION_REF_TAG
    );

    if (NT_SUCCESS(powerReferenceStatus))
    {
        m_powerReferenceAcquired = true;
        LogInfo(FLAG_POWER,
            "%!FUNC! Successfully acquired StopIdle reference on WDFDEVICE: %p for NETADAPTER: %p",
            m_device,
            m_adapter);
    }
    else
    {
        LogError(FLAG_POWER,
            "%!FUNC! Failed to acquire StopIdle reference on WDFDEVICE: %p for NETADAPTER: %p. Status: %!STATUS!",
            m_device,
            m_adapter,
            powerReferenceStatus);
    }
    return powerReferenceStatus;
}

_Use_decl_annotations_
void
NxIdleRestrictions::ReleasePowerReference()
{
    if (!m_powerReferenceAcquired)
    {
        return;
    }

    WdfDeviceResumeIdleWithTag(
        m_device,
        NX_IDLE_RESTRICTION_REF_TAG
    );

    m_powerReferenceAcquired = false;

    LogInfo(FLAG_POWER,
        "%!FUNC! Released StopIdle reference on WDFDEVICE: %p for NETADAPTER: %p",
        m_device,
        m_adapter);
}

#endif

#ifndef _KERNEL_MODE

// -------------- Stubs -------------------
_Use_decl_annotations_
NTSTATUS
NxIdleRestrictions::AcquirePowerReference()
{
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxIdleRestrictions::ReleasePowerReference()
{
    return;
}

#endif
