/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxExecutionContext.cpp

Abstract:

    Implements an Execution Context (EC).

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxExecutionContext.tmh"
#include "NxExecutionContext.hpp"

#if _KERNEL_MODE
using unique_zw_handle = wil::unique_any<HANDLE, decltype(&::ZwClose), &::ZwClose>;
#endif

NTSTATUS
NxExecutionContext::Initialize(PVOID context, EC_START_ROUTINE callback)
{
    auto const priorState = SetState(EcState::Initialized);
    WIN_VERIFY(priorState == EcState::Stopped);

#if _KERNEL_MODE

    unique_zw_handle threadHandle;

    auto status =
        CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            PsCreateSystemThread(
                &threadHandle, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr, callback, context),
            "Failed to start execution context. ExecutionContext=%p", this);

    if (!NT_SUCCESS(status))
    {
        m_ecState = EcState::Stopped;
        return status;
    }

    unique_pkthread threadObject;

    WIN_VERIFY(NT_SUCCESS(ObReferenceObjectByHandle(
        threadHandle.get(),
        THREAD_ALL_ACCESS,
        nullptr,
        KernelMode,
        (PVOID*)&threadObject,
        nullptr)));

    KeSetPriorityThread(threadObject.get(), LOW_PRIORITY);

#else

    wil::unique_handle threadObject;
    threadObject.reset(CreateThread(nullptr, 0, callback, context, 0, nullptr));
    if (!threadObject)
    {
        m_ecState = EcState::Stopped;
        return NTSTATUS_FROM_WIN32(GetLastError());
    }

#endif

    m_workerThreadObject = wistd::move(threadObject);

    return STATUS_SUCCESS;
}

void
NxExecutionContext::Start()
{
    auto const priorState = SetState(EcState::Started);
    WIN_VERIFY(priorState == EcState::Initialized);

    m_signal.Set();
}

void
NxExecutionContext::Stop()
{
    auto const priorState = SetState(EcState::Stopping);
    WIN_VERIFY(priorState == EcState::Initialized || priorState == EcState::Started);

    Signal();

#if _KERNEL_MODE

    KeWaitForSingleObject(
        m_workerThreadObject.get(),
        Executive,
        KernelMode,
        FALSE,
        nullptr);

#else

    WaitForSingleObject(m_workerThreadObject.get(), INFINITE);

#endif

    m_workerThreadObject.reset();

    WIN_VERIFY(SetState(EcState::Stopped) == EcState::Stopping);
}

void
NxExecutionContext::Signal()
{
    WIN_ASSERT(m_ecState == EcState::Started || m_ecState == EcState::Stopping);

    m_signal.Set();
}

void
NxExecutionContext::WaitForStart()
{
    m_signal.Wait();

    WIN_ASSERT(m_ecState == EcState::Started || m_ecState == EcState::Stopping);
}

void
NxExecutionContext::WaitForSignal()
{
    m_signal.Wait();
}

bool
NxExecutionContext::IsStopRequested()
{
    WIN_ASSERT(m_ecState != EcState::Stopped);

    return m_ecState == EcState::Stopping;
}
