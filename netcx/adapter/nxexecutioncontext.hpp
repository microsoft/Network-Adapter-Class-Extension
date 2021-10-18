
// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "Nx.hpp"
#include "ExecutionContext.hpp"

class NxExecutionContext
    : public ExecutionContext
{
public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NxExecutionContext(
        _In_ WDFDEVICE Device,
        _In_ NET_EXECUTION_CONTEXT_CONFIG const * Config
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ~NxExecutionContext(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    WDFDEVICE
    GetDevice(
        void
    ) const;

private:

    NETEXECUTIONCONTEXT const
        m_handle;

    WDFDEVICE const
        m_device;

    NET_EXECUTION_CONTEXT_CONFIG
        m_config = {};

    EXECUTION_CONTEXT_NOTIFICATION
        m_driverNotification;

    EXECUTION_CONTEXT_POLL
        m_preAdvancePoll;

    EXECUTION_CONTEXT_POLL
        m_postAdvancePoll;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxExecutionContext, GetExecutionContextFromHandle);
