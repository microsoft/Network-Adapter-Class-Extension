// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements an Execution Context (EC).

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxExecutionContext.tmh"
#include "NxExecutionContext.hpp"

#include <netioapi.h>

#define ThreadNameInformation static_cast<THREADINFOCLASS>(38)

NTSTATUS
NxExecutionContext::Initialize(PVOID context, EC_START_ROUTINE *callback)
{
#if _KERNEL_MODE

    unique_zw_handle threadHandle;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PsCreateSystemThread(
            &threadHandle, THREAD_ALL_ACCESS, nullptr, nullptr, nullptr, callback, context),
        "Failed to start execution context. ExecutionContext=%p", this);

    m_threadHandle = wistd::move(threadHandle);

    unique_pkthread threadObject;

    WIN_VERIFY(NT_SUCCESS(ObReferenceObjectByHandle(
        m_threadHandle.get(),
        THREAD_ALL_ACCESS,
        nullptr,
        KernelMode,
        (PVOID*)&threadObject,
        nullptr)));

    m_ecIdentifier = HandleToULong(PsGetThreadId((PETHREAD)threadObject.get()));
#else

    wil::unique_handle threadObject;
    threadObject.reset(CreateThread(nullptr, 0, callback, context, 0, nullptr));
    CX_RETURN_NTSTATUS_IF(
        NTSTATUS_FROM_WIN32(GetLastError()),
        ! threadObject);

    m_ecIdentifier = GetThreadId(threadObject.get());
#endif

    m_workerThreadObject = wistd::move(threadObject);

    return STATUS_SUCCESS;
}

NxExecutionContext::EcState
NxExecutionContext::SetState(EcState newState)
{
    return static_cast<EcState>(
        InterlockedExchange((LONG*)&m_ecState, (LONG)newState));
}

void
NxExecutionContext::SignalStopped()
{
    auto const priorState = SetState(EcState::Stopped);
    WIN_VERIFY(priorState == EcState::Stopping);

    m_stopped.Set();
}

void
NxExecutionContext::SetStopping()
{
    auto const priorState = SetState(EcState::Stopping);
    WIN_VERIFY(priorState == EcState::Started);

    SignalWork();
}

void
NxExecutionContext::SetStarted()
{
    auto const priorState = SetState(EcState::Started);
    WIN_VERIFY(priorState == EcState::Stopped);

    SignalWork();
    m_changed.Set();
}

void
NxExecutionContext::SetTerminated()
{
    auto const priorState = SetState(EcState::Terminated);
    WIN_VERIFY(priorState == EcState::Stopped);

    m_changed.Set();
}

void
NxExecutionContext::WaitForStopped()
{
    m_stopped.Wait();
}

void
NxExecutionContext::Terminate()
{
    SetTerminated();

    if (m_workerThreadObject)
    {
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
    }
}

void
NxExecutionContext::Start()
{
    SetStarted();
}

void
NxExecutionContext::Cancel()
{
    SetStopping();
}

void
NxExecutionContext::Stop()
{
    WaitForStopped();
}

void
NxExecutionContext::SignalWork()
{
    m_work.Set();
}

void
NxExecutionContext::WaitForWork()
{
    m_work.Wait();
}

bool
NxExecutionContext::IsStopping() const
{
    WIN_ASSERT(m_ecState != EcState::Terminated);
    WIN_ASSERT(m_ecState != EcState::Stopped);

    return m_ecState == EcState::Stopping;
}

bool
NxExecutionContext::IsTerminated()
{
    SignalWork();
    m_changed.Wait();

    return m_ecState == EcState::Terminated;
}

void
NxExecutionContext::SetDebugNameHint(
    _In_ PCWSTR usage,
    _In_ size_t index,
    _In_ NET_LUID networkInterface)
{
    MIB_IF_ROW2 mib;
    mib.InterfaceLuid = networkInterface;

    if (STATUS_SUCCESS != GetIfEntry2Ex(MibIfEntryNormalWithoutStatistics, &mib))
    {
        mib.Description[0] = L'\0';
    }
    else
    {
        mib.Description[RTL_NUMBER_OF(mib.Description) - 1] = L'\0';
    }

    // Build a string like "Receive thread #3 for Contoso 2000 Ethernet Adapter #4"
    auto cch = swprintf_s(mib.Alias, L"%ws thread #%zu for %ws", usage, index, mib.Description);
    if (cch == -1)
        return;

    WIN_ASSERT(cch < RTL_NUMBER_OF(mib.Alias));

#if _KERNEL_MODE
    // This is actually a THREAD_NAME_INFORMATION, but that's not in a kernel header.
    UNICODE_STRING name = {};

    name.Buffer = mib.Alias;
    name.Length = static_cast<USHORT>(cch * sizeof(WCHAR));
    name.MaximumLength = sizeof(mib.Alias);

    (void)ZwSetInformationThread(m_threadHandle.get(), ThreadNameInformation, &name, sizeof(name));
#else
    SetThreadDescription(m_workerThreadObject.get(), mib.Alias);
#endif
}

ULONG
NxExecutionContext::GetExecutionContextIdentifier() const
{
    return m_ecIdentifier;
}
