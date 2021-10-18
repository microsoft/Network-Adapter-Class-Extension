// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
void
EVT_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED(
    _In_ void * Context,
    _In_ BOOLEAN NotificationEnabled
);

typedef EVT_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED *PFN_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED;

typedef struct _EXECUTION_CONTEXT_NOTIFICATION
{
    void *
        Context;

    PFN_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED
        SetNotificationFn;

    LIST_ENTRY
        Link;

    void *
        Reserved[4];
} EXECUTION_CONTEXT_NOTIFICATION;

inline
void
INITIALIZE_EXECUTION_CONTEXT_NOTIFICATION(
    _Out_ EXECUTION_CONTEXT_NOTIFICATION * Notification,
    _In_opt_ void * Context,
    _In_ PFN_EXECUTION_CONTEXT_SET_NOTIFICATION_ENABLED SetNotificationFn
)
{
    RtlZeroMemory(Notification, sizeof(*Notification));
    Notification->Context = Context;
    Notification->SetNotificationFn = SetNotificationFn;
    InitializeListHead(&Notification->Link);
}
