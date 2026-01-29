// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include "UserModeExecutionContext.hpp"

#define NTSTATUS_FROM_HRESULT(_hr) NTSTATUS_FROM_WIN32(HRESULT_CODE((_hr)))

template <typename InputType>
NTSTATUS
SendSynchronousIoctl(
    HANDLE Handle,
    DWORD IoControlCode,
    InputType *InputBuffer
)
{
    OVERLAPPED overlapped{};
    wil::unique_event_nothrow wait;

    HRESULT hr = wait.create();

    if (FAILED(hr))
    {
        return NTSTATUS_FROM_HRESULT(hr);
    }

    overlapped.hEvent = wait.get();

    DeviceIoControl(
        Handle,
        IoControlCode,
        InputBuffer,
        sizeof(InputType),
        nullptr,
        0,
        nullptr,
        &overlapped);

    DWORD bytesTransferred;

    if (!GetOverlappedResult(
            Handle,
            &overlapped,
            &bytesTransferred,
            TRUE))
    {
        return NTSTATUS_FROM_WIN32(GetLastError());
    }

    return STATUS_SUCCESS;
}

UserModeExecutionContext::~UserModeExecutionContext(
    void
)
{
    // Tell the user mode worker thread it should exit
    m_exit.SetEvent();

    // Wait until user mode worker thread exits
    WaitForSingleObject(m_workerThread.get(), INFINITE);
}

HANDLE
UserModeExecutionContext::GetHandle(
    void
) const
{
    return m_handle.get();
}

NTSTATUS
UserModeExecutionContext::Initialize(
    void
)
{
    wil::unique_hfile handle;

    handle.reset(
        CreateFileW(
            L"\\\\.\\kloader\\{9C0B898D-6275-48EC-81B4-E5EDBE44B535}",
            FILE_READ_ACCESS | FILE_WRITE_ACCESS,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
            nullptr));

    if (!handle)
    {
        NTSTATUS_FROM_WIN32(GetLastError());
    }

    struct
    {
        UserModeExecutionContext *self;
        wil::unique_event_nothrow initialized;
        wil::unique_event_nothrow ready;
    } threadParameters{ this };

    wil::unique_event_nothrow poll;
    wil::unique_event_nothrow exit;
    wil::unique_event_nothrow userThreadMonitorEvent;

    HRESULT hr = threadParameters.initialized.create();

    if (FAILED(hr))
    {
        return NTSTATUS_FROM_HRESULT(hr);
    }

    hr = threadParameters.ready.create();

    if (FAILED(hr))
    {
        return NTSTATUS_FROM_HRESULT(hr);
    }

    hr = poll.create();

    if (FAILED(hr))
    {
        return NTSTATUS_FROM_HRESULT(hr);
    }

    hr = exit.create();

    if (FAILED(hr))
    {
        return NTSTATUS_FROM_HRESULT(hr);
    }

    hr = userThreadMonitorEvent.create();

    if (FAILED(hr))
    {
        return NTSTATUS_FROM_HRESULT(hr);
    }

    wil::unique_handle thread
    {
        CreateThread(
            nullptr,
            0,
            [](void * Context) -> DWORD
            {
                auto parameters = static_cast<decltype(&threadParameters)>(Context);

                auto executionContext = parameters->self;

                // We need to wait until the object initialization is done,
                // otherwise the canary IOCTL may fail depending on timing
                parameters->initialized.wait();

                // We need to send the canary IOCTL before we signal the creating thread that
                // EC has completed initialization, otherwise the creating thread may proceed
                // with queue creations, and reach a point when something will block waiting on EC
                // to finish a task. If process crashes at this point, before canay IOCTL is sent,
                // there will be a deadlock
                executionContext->SendCanaryIoctl();

                // Signal to the thread creating the EC everything is ready
                parameters->ready.SetEvent();

                executionContext->WorkerThread();

                return 0;
            },
            &threadParameters,
            0,
            nullptr)
    };

    if (!thread)
    {
        return NTSTATUS_FROM_WIN32(GetLastError());
    }

    EXECUTION_CONTEXT_INITIALIZE_IN in{};
    in.UserModeContext = reinterpret_cast<EXECUTION_CONTEXT_USER_MODE_CONTEXT>(this);
    in.UserModeAction = &m_userModeAction;
    in.KernelModeAction = &m_kernelModeAction;
    in.Thread = thread.get();
    in.PollEvent = poll.get();
    in.ExitEvent = exit.get();

    RETURN_IF_NTSTATUS_FAILED(
        SendSynchronousIoctl(
            handle.get(),
            IOCTL_EXECUTION_CONTEXT_INITIALIZE,
            &in));

    m_handle = wistd::move(handle);
    m_workerThread = wistd::move(thread);
    m_poll = wistd::move(poll);
    m_exit = wistd::move(exit);
    m_userThreadMonitorEvent = wistd::move(userThreadMonitorEvent);

    threadParameters.initialized.SetEvent();
    threadParameters.ready.wait();

    return STATUS_SUCCESS;
}

void
UserModeExecutionContext::SendCanaryIoctl(
    void
)
{
    m_userThreadMonitorOverlapped = {};
    m_userThreadMonitorOverlapped.hEvent = m_userThreadMonitorEvent.get();

    // This IOCTL is pended by the kernel driver. Its only purpose is to
    // be cancelled by the I/O manager when we exit this thread.
    DeviceIoControl(
        m_handle.get(),
        IOCTL_EXECUTION_CONTEXT_REGISTER_USER_THREAD_MONITOR,
        nullptr,
        0,
        nullptr,
        0,
        nullptr,
        &m_userThreadMonitorOverlapped);
}

void
UserModeExecutionContext::Notify(
    void
)
{
    m_poll.SetEvent();
}

_Use_decl_annotations_
void
UserModeExecutionContext::RegisterPoll(
    EXECUTION_CONTEXT_POLL * Poll
)
{
    SendSynchronousIoctl(
        m_handle.get(),
        IOCTL_EXECUTION_CONTEXT_REGISTER_POLL,
        Poll);
}

_Use_decl_annotations_
void
UserModeExecutionContext::UnregisterPoll(
    EXECUTION_CONTEXT_POLL * Poll
)
{
    SendSynchronousIoctl(
        m_handle.get(),
        IOCTL_EXECUTION_CONTEXT_UNREGISTER_POLL,
        Poll);
}

_Use_decl_annotations_
void
UserModeExecutionContext::RegisterNotification(
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    SendSynchronousIoctl(
        m_handle.get(),
        IOCTL_EXECUTION_CONTEXT_REGISTER_NOTIFICATION,
        Notification);
}

_Use_decl_annotations_
void
UserModeExecutionContext::UnregisterNotification(
    EXECUTION_CONTEXT_NOTIFICATION * Notification
)
{
    SendSynchronousIoctl(
        m_handle.get(),
        IOCTL_EXECUTION_CONTEXT_UNREGISTER_NOTIFICATION,
        Notification);
}

_Use_decl_annotations_
void
UserModeExecutionContext::QueueTask(
    EXECUTION_CONTEXT_TASK * Task
)
{
    SendSynchronousIoctl(
        m_handle.get(),
        IOCTL_EXECUTION_CONTEXT_QUEUE_TASK,
        Task);
}

_Use_decl_annotations_
void
UserModeExecutionContext::WorkerThread(
    void
)
{
    auto & um = m_userModeAction;
    um = {};
    um.Action = ExecutionContextUserActionNone;

    while (true)
    {
        auto & km = m_kernelModeAction;
        km = {};

        switch (um.Action)
        {
            case ExecutionContextUserActionTerminateThread:
                return;

            case ExecutionContextUserActionPoll:
            {
                EXECUTION_CONTEXT_POLL_PARAMETERS parameters{};
                parameters.Irql = PASSIVE_LEVEL;

                um.u.ExecutePoll.Pfn(
                    um.u.ExecutePoll.Context,
                    &parameters);

                km.Action = ExecutionContextKernelActionProcessIteration;
                km.UmWorkCounter = parameters.WorkCounter;

                break;
            }

            case ExecutionContextUserActionSetNotification:
            {
                um.u.SetNotification.Pfn(
                    um.u.SetNotification.Context,
                    um.u.SetNotification.Enabled);

                [[fallthrough]];
            }

            case ExecutionContextUserActionStartNextIteration:
            {
                km.Action = ExecutionContextKernelActionStartNextIteration;
                break;
            }

            case ExecutionContextUserActionRunTask:
            {
                um.u.ExecuteTask.Pfn(
                    um.u.ExecuteTask.Context);

                km.Action = ExecutionContextKernelActionWait;
                break;
            }

            case ExecutionContextUserActionNone:

                // Wait in kernel mode
                km.Action = ExecutionContextKernelActionWait;
                break;
        }

        // This is handled in the fast I/O path by EC, so always completed in the same context it is sent
        DeviceIoControl(
            m_handle.get(),
            IOCTL_EXECUTION_CONTEXT_ENTER_KERNEL,
            nullptr,
            0,
            nullptr,
            0,
            nullptr,
            nullptr);
    }
}
