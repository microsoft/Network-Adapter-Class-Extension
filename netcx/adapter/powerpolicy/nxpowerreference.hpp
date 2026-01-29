// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

class NxPowerPolicy;
class NxNdisPnpPower;

#define NX_POWER_REFERENCE_TAG 'RpxN'

void
EvtPowerRequiredWorkItem(
    _In_ WDFWORKITEM WorkItem
);

class NxPowerReference
    : public NxNonpagedAllocation<NX_POWER_REFERENCE_TAG>
{

private:

    NETADAPTER const
        m_adapter;

    WDFDEVICE const
        m_device;

    NxPowerPolicy const &
        m_powerPolicy;

    NxNdisPnpPower &
        m_ndisPnpPower;

    //
    // Tracks how many outstanding calls to PowerReference there are.
    // This is kept for debugging purposes only.
    //
    LONG
        m_powerReferences = 0;

    //
    // Tracks how many calls to PowerReference were successful.
    //
    LONG
        m_successfulPowerReferences = 0;

#if NETCX_KMDF
    //
    // Used to callback into NDIS when a asynchronous stop idle operation is
    // complete. When the work item callback is invoked, a synchronous call to
    // WdfDeviceStopIdle is made.
    //
    WDFWORKITEM
        m_powerRequiredWorkItem;

    //
    // What m_powerRequiredWorkItem really need to run, e.g. NxAdapter::_EvtStopIdleWorkItem
    //
    PFN_WDF_WORKITEM
        m_evtStopIdleWorkItem;

    //
    // Track whether the queued workitem ran.
    //
    bool
        m_powerRequiredWorkItemPending = false;
#endif

public:

    NxPowerReference(
        _In_ NETADAPTER Adapter,
        _In_ WDFDEVICE Device,
        _In_ NxPowerPolicy const & PowerPolicy,
        _In_ NxNdisPnpPower & NdisPnpPower
    );

    ~NxPowerReference(
        void
    ) = default;

    NTSTATUS
    Initialize(
        void
    );

    NTSTATUS
    PowerReference(
        _In_ bool WaitForD0
    );

    void
    PowerDereference(
        void
    );

    void
    ReleaseOutstandingPowerReferences(
        void
    );

#ifdef NETCX_KMDF
    void
    QueuePowerReferenceWorkItem(
        void
    );

    friend
    void
    EvtPowerRequiredWorkItem(
        WDFWORKITEM WorkItem
    );

    bool
    IsPowerRequiredWorkItemPending(
        void
    ) const;
#endif
};
