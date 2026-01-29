// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <ExecutionContextNotification.h>
#include <ExecutionContextPoll.h>
#include <ExecutionContextTask.h>

DECLARE_HANDLE( EXECUTION_CONTEXT_USER_MODE_CONTEXT );

struct _EXECUTION_CONTEXT_USER_ACTION;
typedef struct _EXECUTION_CONTEXT_USER_ACTION EXECUTION_CONTEXT_USER_ACTION;

struct _EXECUTION_CONTEXT_KERNEL_ACTION;
typedef struct _EXECUTION_CONTEXT_KERNEL_ACTION EXECUTION_CONTEXT_KERNEL_ACTION;

#define EXECUTION_CONTEXT_BUFFERED_IOCTL(func,access) CTL_CODE(FILE_DEVICE_UNKNOWN, func, METHOD_BUFFERED, access)

typedef struct _EXECUTION_CONTEXT_INITIALIZE_IN
{
    EXECUTION_CONTEXT_USER_MODE_CONTEXT UserModeContext;
    EXECUTION_CONTEXT_USER_ACTION *UserModeAction;
    EXECUTION_CONTEXT_KERNEL_ACTION *KernelModeAction;
    HANDLE Thread;
    HANDLE PollEvent;
    HANDLE ExitEvent;
} EXECUTION_CONTEXT_INITIALIZE_IN;

// Input: EXECUTION_CONTEXT_INITIALIZE_IN
#define IOCTL_EXECUTION_CONTEXT_INITIALIZE EXECUTION_CONTEXT_BUFFERED_IOCTL(0xb10, FILE_WRITE_DATA | FILE_READ_DATA)

// Input: EXECUTION_CONTEXT_POLL
// Output: void
#define IOCTL_EXECUTION_CONTEXT_REGISTER_POLL EXECUTION_CONTEXT_BUFFERED_IOCTL(0xb11, FILE_WRITE_DATA)

// Input: EXECUTION_CONTEXT_POLL
// Output: void
#define IOCTL_EXECUTION_CONTEXT_UNREGISTER_POLL EXECUTION_CONTEXT_BUFFERED_IOCTL(0xb12, FILE_WRITE_DATA)

// Input: EXECUTION_CONTEXT_NOTIFICATION
// Output: void
#define IOCTL_EXECUTION_CONTEXT_REGISTER_NOTIFICATION EXECUTION_CONTEXT_BUFFERED_IOCTL(0xb13, FILE_WRITE_DATA)

// Input: EXECUTION_CONTEXT_NOTIFICATION
// Output: void
#define IOCTL_EXECUTION_CONTEXT_UNREGISTER_NOTIFICATION EXECUTION_CONTEXT_BUFFERED_IOCTL(0xb14, FILE_WRITE_DATA)

// Input: EXECUTION_CONTEXT_TASK
// Output: void
#define IOCTL_EXECUTION_CONTEXT_QUEUE_TASK EXECUTION_CONTEXT_BUFFERED_IOCTL(0xb15, FILE_WRITE_DATA)

typedef enum _EXECUTION_CONTEXT_USER_ACTION_TYPE
{
    ExecutionContextUserActionNone = 0,
    ExecutionContextUserActionTerminateThread,
    ExecutionContextUserActionPoll,
    ExecutionContextUserActionSetNotification,
    ExecutionContextUserActionRunTask,
    ExecutionContextUserActionStartNextIteration,
} EXECUTION_CONTEXT_USER_ACTION_TYPE;

typedef enum _EXECUTION_CONTEXT_KERNEL_ACTION_TYPE
{
    ExecutionContextKernelActionWait = 0,
    ExecutionContextKernelActionProcessIteration,
    ExecutionContextKernelActionStartNextIteration,
} EXECUTION_CONTEXT_KERNEL_ACTION_TYPE;

typedef struct _EXECUTION_CONTEXT_KERNEL_ACTION
{
    EXECUTION_CONTEXT_KERNEL_ACTION_TYPE Action;
    UINT32 UmWorkCounter;
} EXECUTION_CONTEXT_KERNEL_ACTION;

typedef struct _EXECUTION_CONTEXT_USER_ACTION
{
    EXECUTION_CONTEXT_USER_ACTION_TYPE Action;
    union
    {
        struct
        {
            void * Context;
            PFN_EXECUTION_CONTEXT_POLL Pfn;
        } ExecutePoll;

        struct
        {
            void * Context;
            PFN_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED Pfn;
            BOOLEAN Enabled;
        } SetNotification;

        struct
        {
            void * Context;
            PFN_EXECUTION_CONTEXT_TASK Pfn;
        } ExecuteTask;
    } u;
} EXECUTION_CONTEXT_USER_ACTION;

// Input: void
// Output: void
#define IOCTL_EXECUTION_CONTEXT_ENTER_KERNEL EXECUTION_CONTEXT_BUFFERED_IOCTL(0xb16, FILE_WRITE_DATA | FILE_READ_DATA)

// Input: void
// Output: void
#define IOCTL_EXECUTION_CONTEXT_REGISTER_USER_THREAD_MONITOR EXECUTION_CONTEXT_BUFFERED_IOCTL(0xb17, FILE_READ_DATA)
