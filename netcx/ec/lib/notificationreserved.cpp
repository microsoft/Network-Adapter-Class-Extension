// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NotificationReserved.hpp"

_Use_decl_annotations_
NotificationReserved *
NotificationReserved::FromNotificationEntry(
    LIST_ENTRY * NotificationEntry
)
{
    auto notification = CONTAINING_RECORD(
        NotificationEntry,
        EXECUTION_CONTEXT_NOTIFICATION,
        Link);

    return reinterpret_cast<NotificationReserved *>(&notification->Reserved[0]);
}

_Use_decl_annotations_
EXECUTION_CONTEXT_NOTIFICATION *
NotificationReserved::GetNotification(
    void
)
{
    return CONTAINING_RECORD(
        this,
        EXECUTION_CONTEXT_NOTIFICATION,
        Reserved);
}

_Use_decl_annotations_
void
NotificationReserved::Enable(
    void
)
{
    if (!Enabled)
    {
        Enabled = TRUE;

        auto notification = GetNotification();
        notification->SetNotificationFn(notification->Context, Enabled);
    }
}

_Use_decl_annotations_
void
NotificationReserved::Disable(
    void
)
{
    if (Enabled)
    {
        Enabled = FALSE;

        auto notification = GetNotification();
        notification->SetNotificationFn(notification->Context, Enabled);
    }
}
