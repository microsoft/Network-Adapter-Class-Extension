/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxExecutionContext.hpp

Abstract:

    Defines a generic Execution Context (EC) that can safely encapsulates
    single-threaded execution of some task.

Usage:

    To use an EC, implement an EC_START_ROUTINE routine like this:

            void MyEcRoutine()
            {
                ec->WaitForStart();

                while (!ec->IsStopRequested())
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

using unique_pkthread = wil::unique_any<PKTHREAD, decltype(&::DereferenceObject), &::DereferenceObject>;
#endif

#if _KERNEL_MODE
using EC_START_ROUTINE = PKSTART_ROUTINE;
using EC_RETURN = VOID;
#else
using EC_START_ROUTINE = PTHREAD_START_ROUTINE;
using EC_RETURN = DWORD;
#endif

/// Encapsulates single-threaded execution of a task that can be suspended and
/// resumed
class NxExecutionContext
{
public:

    NTSTATUS Initialize(PVOID context, EC_START_ROUTINE callback);

    /// Called from outside the EC once the EC should start running from its 
    /// initial state.
    void Start();

    /// Called from outside the EC to signal the task to clean up.
    /// Must be follwed with a call to WaitForStopComplete().
    void Stop();

    /// Called from outside the EC to prod the EC into taking action.  If the
    /// EC is already executing, then this is a no-op.
    void Signal();

    /// Called only by code running in the EC.  Suspends execution until the
    /// EC is started by a call to Start().
    void WaitForStart();

    /// Called only by code running in the EC.  Suspends execution until the
    /// EC is signalled by a call to Signal().
    void WaitForSignal();

    /// Called only by code running in the EC.  Returns true if the code should
    /// clean up and exit the EC thread.
    bool IsStopRequested();

private:

    enum EcState : LONG
    {
        Stopped,
        Initialized,
        Started,
        Stopping,
    };

    _Interlocked_ volatile 
    EcState m_ecState = EcState::Stopped;

    EcState SetState(EcState newState)
    {
        return (EcState)InterlockedExchange((LONG*)&m_ecState, (LONG)newState);
    }


    KAutoEvent m_signal;

#if _KERNEL_MODE
    unique_pkthread m_workerThreadObject;
#else
    wil::unique_handle m_workerThreadObject;
#endif
};
