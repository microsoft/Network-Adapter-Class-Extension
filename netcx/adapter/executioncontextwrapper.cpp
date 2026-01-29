// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"
#include "version.hpp"

#include "ExecutionContextWrapper.hpp"
#include "ExecutionContextWrapper.tmh"

ExecutionContextWrapper::ExecutionContextWrapper(
    _In_ GUID const * ClientIdentifier,
    _In_ EXECUTION_CONTEXT_RUNTIME_KNOBS const * RuntimeMutableKnobs) :
        m_clientIdentifier{ ClientIdentifier },
        m_runtimeMutableKnobs{ RuntimeMutableKnobs }
{
}

ExecutionContextWrapper::~ExecutionContextWrapper()
{
    if (m_executionContextHandle != nullptr)
    {
        m_dispatchTable.Destroy(m_executionContextHandle);
    }
}

NTSTATUS
ExecutionContextWrapper::InitializeExecutionContextHandle(
    _In_opt_ UNICODE_STRING const* clientFriendlyName
)
{
    EXECUTION_CONTEXT_CONFIGURATION configuration;
    INITIALIZE_EXECUTION_CONTEXT_CONFIGURATION(&configuration);
    configuration.ClientIdentifier = m_clientIdentifier;
    configuration.RuntimeKnobs = m_runtimeMutableKnobs;
    if(clientFriendlyName != nullptr)
    {
        configuration.ClientFriendlyName = *clientFriendlyName;
    }

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_dispatchTable.Create(&configuration, &m_executionContextHandle),
        "Failed to initialize new ExecutionContext object");

    return STATUS_SUCCESS;
}

_No_competing_thread_
NTSTATUS
ExecutionContextWrapper::Initialize(
    _In_opt_ UNICODE_STRING const* clientFriendlyName
)
{
    RtlCopyMemory(
        static_cast<NET_EXECUTION_CONTEXT_DISPATCH *>(&this->m_dispatchTable),
        GetCxDriverContextFromHandle(WdfGetDriver())->GetExecutionContextDispatchTable(),
        sizeof(NET_EXECUTION_CONTEXT_DISPATCH));

    CX_RETURN_IF_NOT_NT_SUCCESS(this->InitializeExecutionContextHandle(clientFriendlyName));

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(HIGH_LEVEL)
void
ExecutionContextWrapper::Notify()
{
    m_dispatchTable.Notify(m_executionContextHandle);
}

void
ExecutionContextWrapper::SetAffinity(_In_ PROCESSOR_NUMBER const *Affinity)
{
    m_dispatchTable.SetAffinity(m_executionContextHandle, Affinity);
}

_Use_decl_annotations_
void
ExecutionContextWrapper::RegisterPoll(
    EXECUTION_CONTEXT_POLL * Poll
)
{
    m_dispatchTable.RegisterPoll(m_executionContextHandle, Poll);
}

_Use_decl_annotations_
void
ExecutionContextWrapper::UnregisterPoll(
    EXECUTION_CONTEXT_POLL * Poll
)
{
    m_dispatchTable.UnregisterPoll(m_executionContextHandle, Poll);
}

_Use_decl_annotations_
void
ExecutionContextWrapper::QueueTask(
    EXECUTION_CONTEXT_TASK * Task
)
{
    m_dispatchTable.QueueTask(m_executionContextHandle, Task);
}

_Use_decl_annotations_
void
ExecutionContextWrapper::ChangePollFunction(
    EXECUTION_CONTEXT_POLL * Poll,
    PFN_EXECUTION_CONTEXT_POLL PollFn
)
{
    m_dispatchTable.ChangePollFunction(m_executionContextHandle, Poll, PollFn);
}

_Use_decl_annotations_
void
ExecutionContextWrapper::RegisterNotification(
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    m_dispatchTable.RegisterNotification(m_executionContextHandle, Notification);
}

_Use_decl_annotations_
void
ExecutionContextWrapper::UnregisterNotification(
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    m_dispatchTable.UnregisterNotification(m_executionContextHandle, Notification);
}

NET_EXECUTION_CONTEXT_DISPATCH *
ExecutionContextWrapper::GetExecutionContextDispatch()
{
    return &m_dispatchTable;
}

NET_EXECUTION_CONTEXT
ExecutionContextWrapper::GetHandle()
{
    return m_executionContextHandle;
}
