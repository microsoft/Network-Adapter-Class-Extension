// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <wil/resource.h>
#include <KMacros.h>

#ifdef _KERNEL_MODE

using EC_START_ROUTINE = KSTART_ROUTINE;
using EC_RETURN = VOID;
using EC_THREAD = PKTHREAD;
static const EC_THREAD EC_THREAD_INVALID = nullptr;
using EC_THREAD_PRIORITY = LONG;
#define EC_INVALID_THREAD_PRIORITY MAXLONG

using unique_zw_handle = wil::unique_any<HANDLE, decltype(&::ZwClose), &::ZwClose>;
using unique_pkthread = wil::unique_any<PKTHREAD, decltype(&ObfDereferenceObject), &ObfDereferenceObject>;

struct unique_thread
{
    unique_zw_handle
        NtHandle;

    unique_pkthread
        ObHandle;

    operator bool(
        void
    ) const;

    void reset(
    );
};

#else
using EC_START_ROUTINE = wistd::remove_pointer<PTHREAD_START_ROUTINE>::type;
using EC_RETURN = DWORD;
using EC_THREAD = DWORD;
static const EC_THREAD EC_THREAD_INVALID = 0;
using EC_THREAD_PRIORITY = int;
#define EC_INVALID_THREAD_PRIORITY THREAD_PRIORITY_ERROR_RETURN

using unique_thread = wil::unique_handle;
#endif

EC_THREAD
EcGetCurrentThread(
    void
);

NTSTATUS
EcThreadCreate(
    _In_ EC_START_ROUTINE StartRoutine,
    _In_opt_ void * Context,
    _Out_ unique_thread & Thread
);

void
EcThreadSetDescription(
    _In_ unique_thread & Thread,
    _In_ UNICODE_STRING const & Description
);

void
EcThreadSetPriority(
    _In_ unique_thread & Thread,
    _In_ EC_THREAD_PRIORITY Priority
);

EC_THREAD_PRIORITY
EcThreadGetPriority(
    _In_ unique_thread & Thread
);

void
EcThreadSetAffinity(
    _In_ PGROUP_AFFINITY GroupAffinity,
    _Out_opt_ PGROUP_AFFINITY PreviousAffinity
);

void
EcThreadWaitForTermination(
    _In_ unique_thread & Thread
);
