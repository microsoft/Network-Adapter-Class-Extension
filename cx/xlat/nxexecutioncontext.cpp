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

    m_threadHandle = wistd::move(threadHandle);

    unique_pkthread threadObject;

    WIN_VERIFY(NT_SUCCESS(ObReferenceObjectByHandle(
        m_threadHandle.get(),
        THREAD_ALL_ACCESS,
        nullptr,
        KernelMode,
        (PVOID*)&threadObject,
        nullptr)));

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

NTSTATUS
NxExecutionContext::SetGroupAffinity(
    GROUP_AFFINITY & GroupAffinity
    )
{
#ifdef _KERNEL_MODE
    return ZwSetInformationThread(
        m_threadHandle.get(),
        ThreadGroupInformation,
        &GroupAffinity,
        sizeof(GroupAffinity));
#else
    UNREFERENCED_PARAMETER(GroupAffinity);

    return STATUS_SUCCESS;
#endif
}

void
NxExecutionContext::Start()
{
    auto const priorState = SetState(EcState::Started);
    WIN_VERIFY(priorState == EcState::Initialized);

    m_signal.Set();
}

void
NxExecutionContext::RequestStop()
{
    auto const priorState = SetState(EcState::Stopping);
    WIN_VERIFY(priorState == EcState::Initialized || priorState == EcState::Started || priorState == EcState::Stopped);

    Signal();
}

void
NxExecutionContext::CompleteStop()
{
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

        m_workerThreadObject.reset();
    }

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

void
NxExecutionContext::SetDebugNameHint(
    _In_ PCWSTR usage,
    _In_ ULONG index,
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
    auto cch = swprintf_s(mib.Alias, L"%ws thread #%u for %ws", usage, index, mib.Description);
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
