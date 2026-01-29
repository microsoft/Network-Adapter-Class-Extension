// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"

#include "NxPowerPolicy.hpp"
#include "NxPowerReference.hpp"
#include "ndisminiport\NxNdisPnpPower.hpp"
#include "NxUtility.hpp"

#include "NxPowerReference.tmh"

_Use_decl_annotations_
NxPowerReference::NxPowerReference(
    NETADAPTER Adapter,
    WDFDEVICE Device,
    NxPowerPolicy const & PowerPolicy,
    NxNdisPnpPower & NdisPnpPower
)
    : m_adapter(Adapter)
    , m_device(Device)
    , m_powerPolicy(PowerPolicy)
    , m_ndisPnpPower(NdisPnpPower)
{
}

#ifdef NETCX_KMDF

bool
NxPowerReference::IsPowerRequiredWorkItemPending(
    void
) const
{
    return m_powerRequiredWorkItemPending;
}

struct PowerRequiredWorkItemContext
{
    NxPowerReference *
        PowerReference;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PowerRequiredWorkItemContext, GetPowerRequiredWorkItemContext);

_Use_decl_annotations_
void
EvtPowerRequiredWorkItem(
    WDFWORKITEM WorkItem
)
{
    PowerRequiredWorkItemContext * context = GetPowerRequiredWorkItemContext(WorkItem);
    NxPowerReference * nxPowerReference = context->PowerReference;

    // We need to notify NDIS with the device in D0, so we acquire a synchronous power reference
    NTSTATUS status = nxPowerReference->PowerReference(true);

    nxPowerReference->m_ndisPnpPower.NotifyAsyncPowerReferenceCompletion(status);

    // Drop the reference we acquired above. The original reference will be released by NDIS
    nxPowerReference->PowerDereference();

    nxPowerReference->m_powerRequiredWorkItemPending = false;
}

NTSTATUS
NxPowerReference::Initialize(
    void
)
{
    //
    // Create a WDFWORKITEM object
    // 1) make NETADAPTER its parent
    // 2) allocate PowerRequiredWorkItemContext on the WDFWORKITEM object and set its PowerReference to *this*
    //
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes, PowerRequiredWorkItemContext);
    objectAttributes.ParentObject = m_adapter;

    WDF_WORKITEM_CONFIG stopIdleWorkItemConfig;
    WDF_WORKITEM_CONFIG_INIT(&stopIdleWorkItemConfig, EvtPowerRequiredWorkItem);
    stopIdleWorkItemConfig.AutomaticSerialization = FALSE;

    auto const status = WdfWorkItemCreate(&stopIdleWorkItemConfig, &objectAttributes, &m_powerRequiredWorkItem);

    if (!NT_SUCCESS(status))
    {
        LogError(FLAG_POWER, "Failed to create m_powerRequiredWorkItem, Status: %!STATUS!", status);

        return status;
    }

    PowerRequiredWorkItemContext * context = GetPowerRequiredWorkItemContext(m_powerRequiredWorkItem);
    context->PowerReference = this;

    return STATUS_SUCCESS;
}

void
NxPowerReference::QueuePowerReferenceWorkItem(
    void
)
{
    m_powerRequiredWorkItemPending = true;
    WdfWorkItemEnqueue(m_powerRequiredWorkItem);
}

#else

NTSTATUS
NxPowerReference::Initialize(
    void
)
{
    return STATUS_SUCCESS;
}

#endif

_Use_decl_annotations_
NTSTATUS
NxPowerReference::PowerReference(
    bool WaitForD0
)
{
    InterlockedIncrement(&m_powerReferences);

    if (!m_powerPolicy.ShouldAcquirePowerReferencesForAdapter())
    {
        LogInfo(FLAG_POWER,
            "%!FUNC! Not acquiring PowerReference because adapter indicated not to acquire references on its behalf.");
        return STATUS_SUCCESS;
    }

    const auto powerReferenceStatus = WdfDeviceStopIdleWithTag(
        m_device,
        WaitForD0,
        reinterpret_cast<void *>(NX_POWER_REFERENCE_TAG));

    if (NT_SUCCESS(powerReferenceStatus))
    {
        InterlockedIncrement(&m_successfulPowerReferences);
    }
    else
    {
        LogWarning(FLAG_ADAPTER,
            "Power reference on parent device failed. WDFDEVICE: %p, NETADAPTER: %p", m_device, m_adapter);
    }

    return powerReferenceStatus;
}

void
NxPowerReference::PowerDereference(
    void
)
{
    InterlockedDecrement(&m_powerReferences);

    if (-1 != NxInterlockedDecrementFloor(&m_successfulPowerReferences, 0))
    {
        NT_FRE_ASSERT(m_powerPolicy.ShouldAcquirePowerReferencesForAdapter());
        WdfDeviceResumeIdleWithTag(m_device, reinterpret_cast<void *>(NX_POWER_REFERENCE_TAG));
    }
    else
    {
        LogWarning(FLAG_ADAPTER, "Dropped reference for failed WdfDeviceStopIdle call. NETADAPTER %p", m_adapter);
    }
}

void
NxPowerReference::ReleaseOutstandingPowerReferences(
    void
)
{
    LONG outstandingReferences;

    while ((outstandingReferences = NxInterlockedDecrementFloor(&m_successfulPowerReferences, 0)) > -1)
    {
        LogWarning(FLAG_ADAPTER, "Releasing outstanding reference for NETADAPTER %p. Remaining references %d",
            m_adapter,
            outstandingReferences);

        WdfDeviceResumeIdleWithTag(m_device, reinterpret_cast<void *>(NX_POWER_REFERENCE_TAG));
    }
}
