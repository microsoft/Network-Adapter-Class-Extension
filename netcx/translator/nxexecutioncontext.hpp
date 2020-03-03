// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Defines a generic Execution Context (EC) that can safely encapsulates
    single-threaded execution of some task.

Usage:

    To use an EC, implement an EC_START_ROUTINE routine like this:

            void MyEcRoutine()
            {
                ec->WaitForSignal();

                while (!ec->IsStoppping())
                {
                    if (no work to do)
                        ec->WaitForSignal();
                }
            }

    Then use it like this:

            NxExecutionContext ec;

            ec.Initialize(&MyEcRoutine);
            ec.Start();

            . . . time passes . . .

            ec.Stop();
            ec.WaitForStopComplete();

--*/

#pragma once

#include <KWaitEvent.h>

#if _KERNEL_MODE

inline void DereferenceObject(PVOID object)
{
    ObDereferenceObject(object);
}

using unique_zw_handle = wil::unique_any<HANDLE, decltype(&::ZwClose), &::ZwClose>;
using unique_pkthread = wil::unique_any<PKTHREAD, decltype(&::DereferenceObject), &::DereferenceObject>;

#endif

#if _KERNEL_MODE
using EC_START_ROUTINE = KSTART_ROUTINE;
using EC_RETURN = VOID;
#else
using EC_START_ROUTINE = wistd::remove_pointer<PTHREAD_START_ROUTINE>::type;
using EC_RETURN = DWORD;
#endif

/// Encapsulates single-threaded execution of a task that can be suspended and
/// resumed
class NxExecutionContext
{
public:

    NTSTATUS
    Initialize(
        void * context,
        EC_START_ROUTINE * callback
    );

    void
    Start(
        void
    );

    void
    Cancel(
        void
    );

    void
    Stop(
        void
    );

    void
    Terminate(
        void
    );

    void
    SignalWork(
        void
    );

    void
    WaitForWork(
        void
    );

    void
    SignalStopped(
        void
    );

    void
    WaitForStopped(
        void
    );


    /// Called only by code running in the EC. Returns true if stopping and
    /// false otherwise.
    bool
    IsStopping(
        void
    ) const;

    bool
    IsTerminated(
        void
    );

    void
    SetDebugNameHint(
        _In_ PCWSTR usage,
        _In_ size_t index,
        _In_ NET_LUID networkInterface
    );

    ULONG
    GetExecutionContextIdentifier() const;

private:

    enum EcState : LONG
    {
        Stopped,
        Started,
        Stopping,
        Terminated,
    };

    _Interlocked_ volatile
    EcState
        m_ecState = EcState::Stopped;

    EcState
    SetState(
        EcState newState
    );

    void
    SetStopping(
        void
    );

    void
    SetStarted(
        void
    );

    void
    SetTerminated(
        void
    );

    KAutoEvent m_work;
    KAutoEvent m_stopped;
    KAutoEvent m_changed;

#if _KERNEL_MODE
    unique_zw_handle m_threadHandle;
    unique_pkthread m_workerThreadObject;
#else
    wil::unique_handle m_workerThreadObject;
#endif

    ULONG m_ecIdentifier = 0;
};

