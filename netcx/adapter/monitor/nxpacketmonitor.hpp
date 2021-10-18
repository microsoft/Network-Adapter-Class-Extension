// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once 

#include <netiodef.h>
#include <pktmonclnt.h>
#include <kpushlock.h>

class NxAdapter;

class NxPacketMonitor
{

public:

    _IRQL_requires_max_(PASSIVE_LEVEL)
    NxPacketMonitor(
        NxAdapter & Adapter
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    ~NxPacketMonitor(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    static
    void
    EnumerateAndRegisterAdapters(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    static
    void
    EnumerateAndUnRegisterAdapters(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    RegisterAdapter(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    void
    UnregisterAdapter(
        void
    );

    _IRQL_requires_max_(PASSIVE_LEVEL)
    PKTMON_EDGE_CONTEXT const &
    GetLowerEdgeContext(
        void
    ) const;

    _IRQL_requires_max_(PASSIVE_LEVEL)
    PKTMON_COMPONENT_CONTEXT const &
    GetComponentContext(
        void
    ) const;

public:

    KPushLock
        m_registrationLock;

private:

    NxAdapter &
        m_adapter;

    _Guarded_by_(m_registrationLock)
    PKTMON_COMPONENT_CONTEXT
        m_clientComponentContext = {};

    _Guarded_by_(m_registrationLock)
    bool
        m_isRegistered = false;

    PKTMON_EDGE_CONTEXT
        m_clientLowerEdgeContext = {};
};
