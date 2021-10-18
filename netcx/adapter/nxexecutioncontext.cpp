// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxExecutionContext.hpp"
#include "NxExecutionContext.tmh"

_Use_decl_annotations_
NxExecutionContext::NxExecutionContext(
    WDFDEVICE Device,
    NET_EXECUTION_CONTEXT_CONFIG const * Config
)
    : ExecutionContext(&g_staticEcClientIdentier, &g_staticEcKnobs)
    , m_handle(static_cast<NETEXECUTIONCONTEXT>(WdfObjectContextGetObject(this)))
    , m_device(Device)
{
    RtlCopyMemory(
        &m_config,
        Config,
        Config->Size);

    INITIALIZE_EXECUTION_CONTEXT_POLL(
        &m_preAdvancePoll,
        ExecutionContextPollTypePreAdvance,
        m_handle,
        reinterpret_cast<PFN_EXECUTION_CONTEXT_POLL>(m_config.EvtPreAdvance));

    INITIALIZE_EXECUTION_CONTEXT_POLL(
        &m_postAdvancePoll,
        ExecutionContextPollTypePostAdvance,
        m_handle,
        reinterpret_cast<PFN_EXECUTION_CONTEXT_POLL>(m_config.EvtPostAdvance));

    INITIALIZE_EXECUTION_CONTEXT_NOTIFICATION(
        &m_driverNotification,
        m_handle,
        reinterpret_cast<PFN_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED>(m_config.EvtSetNotificationEnabled));
}

_Use_decl_annotations_
NxExecutionContext::~NxExecutionContext(
    void
)
{
    if (!IsListEmpty(&m_postAdvancePoll.Link))
    {
        this->UnregisterPoll(&m_postAdvancePoll);
    }

    if (!IsListEmpty(&m_preAdvancePoll.Link))
    {
        this->UnregisterPoll(&m_preAdvancePoll);
    }

    if (!IsListEmpty(&m_driverNotification.Link))
    {
        this->UnregisterNotification(&m_driverNotification);
    }
}

_Use_decl_annotations_
NTSTATUS
NxExecutionContext::Initialize(
    void
)
{
    DECLARE_UNICODE_STRING_SIZE(name, ARRAYSIZE(L"NETEXECUTIONCONTEXT xxxxxxxxxxxxxxxx"));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlUnicodeStringPrintf(
            &name,
            L"NETEXECUTIONCONTEXT %p",
            m_handle));

    CX_RETURN_IF_NOT_NT_SUCCESS(ExecutionContext::Initialize(&name));

    if (m_config.EvtPreAdvance != nullptr)
    {
        RegisterPoll(&m_preAdvancePoll);
    }

    if (m_config.EvtPostAdvance != nullptr)
    {
        RegisterPoll(&m_postAdvancePoll);
    }

    RegisterNotification(&m_driverNotification);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
WDFDEVICE
NxExecutionContext::GetDevice(
    void
) const
{
    return m_device;
}
