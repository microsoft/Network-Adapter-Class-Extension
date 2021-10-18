// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <ExecutionContextNotification.h>

// This structure is stored in EXECUTION_CONTEXT_NOTIFICATION's Reserved field.
// All member variables will be zero initialized
struct NotificationReserved
{
    BOOLEAN
        Enabled;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    static
    NotificationReserved *
    FromNotificationEntry(
        _In_ LIST_ENTRY * NotificationEntry
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    EXECUTION_CONTEXT_NOTIFICATION *
    GetNotification(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Enable(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Disable(
        void
    );
};

static_assert(
    sizeof(NotificationReserved) <=
    sizeof(EXECUTION_CONTEXT_NOTIFICATION::Reserved));
