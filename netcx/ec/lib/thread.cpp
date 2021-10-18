// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "Thread.hpp"

#ifdef _KERNEL_MODE
unique_thread::operator bool(
    void
) const
{
    return !!NtHandle;
}

void unique_thread::reset(
)
{
    NtHandle.reset();
    ObHandle.reset();
}
#endif

EC_THREAD
EcGetCurrentThread(
    void
)
{
#ifdef _KERNEL_MODE
    return KeGetCurrentThread();
#else
    return GetCurrentThreadId();
#endif
}

_Use_decl_annotations_
NTSTATUS
EcThreadCreate(
    EC_START_ROUTINE StartRoutine,
    void * Context,
    unique_thread & Thread
)
{
#ifdef _KERNEL_MODE

    unique_zw_handle ntHandle;
    unique_pkthread obHandle;

    auto const ntStatus = PsCreateSystemThread(
        &ntHandle,
        THREAD_ALL_ACCESS,
        nullptr,
        nullptr,
        nullptr,
        StartRoutine,
        Context);

    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    NT_FRE_ASSERT(
        NT_SUCCESS(
            ObReferenceObjectByHandle(
                ntHandle.get(),
                THREAD_ALL_ACCESS,
                nullptr,
                KernelMode,
                reinterpret_cast<void **>(&obHandle),
                nullptr)));

    Thread.NtHandle = wistd::move(ntHandle);
    Thread.ObHandle = wistd::move(obHandle);

#else
    wil::unique_handle thread{ CreateThread(nullptr, 0, StartRoutine, Context, 0, nullptr) };

    if (!thread)
    {
        return NTSTATUS_FROM_WIN32(GetLastError());
    }

    Thread = wistd::move(thread);
#endif

    return STATUS_SUCCESS;
}

void
EcThreadSetDescription(
    unique_thread & Thread,
    UNICODE_STRING const & Description
)
{
#ifdef _KERNEL_MODE
    //
    // Description(UNICODE_STRING) is actually a THREAD_NAME_INFORMATION,
    // but THREAD_NAME_INFORMATION is not in a kernel header.
    //
    (void)ZwSetInformationThread(
        Thread.NtHandle.get(),
        ThreadNameInformation,
        const_cast<UNICODE_STRING *>(&Description),
        sizeof(Description));
#else
    //
    // Ensure we feed null-terminated string into SetThreadDescription.
    //
    auto wideCharCount = Description.Length / sizeof(WCHAR);
    NT_FRE_ASSERT(wcsnlen(Description.Buffer, wideCharCount + 1U) == wideCharCount);

    SetThreadDescription(Thread.get(), Description.Buffer);
#endif
}

void
EcThreadSetPriority(
    unique_thread & Thread,
    EC_THREAD_PRIORITY Priority
)
{
#ifdef _KERNEL_MODE
    // KeSetBasePriorityThread does not take the actual priority, but an increment
    // to be added to the current base priority. Calculate this value.
    auto const increment = Priority - (LOW_REALTIME_PRIORITY + LOW_PRIORITY) / 2;
    KeSetBasePriorityThread(Thread.ObHandle.get(), increment);
#else
    SetThreadPriority(Thread.get(), Priority);
#endif
}

EC_THREAD_PRIORITY
EcThreadGetPriority(
    unique_thread & Thread
)
{
#ifdef _KERNEL_MODE
    return KeQueryPriorityThread(Thread.ObHandle.get());
#else
    return GetThreadPriority(Thread.get());
#endif
}

_Use_decl_annotations_
void
EcThreadSetAffinity(
    PGROUP_AFFINITY GroupAffinity,
    PGROUP_AFFINITY PreviousAffinity
)
{
#ifdef _KERNEL_MODE
    KeSetSystemGroupAffinityThread(GroupAffinity, PreviousAffinity);
#else
    SetThreadGroupAffinity(GetCurrentThread(), GroupAffinity, PreviousAffinity);
#endif
}

_Use_decl_annotations_
void
EcThreadWaitForTermination(
    unique_thread & Thread
)
{
#ifdef _KERNEL_MODE
    KeWaitForSingleObject(
        Thread.ObHandle.get(),
        KWAIT_REASON::Executive,
        KernelMode,
        FALSE,
        nullptr);
#else
    WaitForSingleObject(
        Thread.get(),
        INFINITE);
#endif
}
