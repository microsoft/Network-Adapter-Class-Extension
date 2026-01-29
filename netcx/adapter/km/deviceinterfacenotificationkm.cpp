// Copyright (C) Microsoft Corporation. All rights reserved.
#include "Nx.hpp"
#include "DeviceInterfaceNotification.hpp"
#include "DeviceInterfaceNotificationKm.tmh"

_Use_decl_annotations_
NTSTATUS
NotificationCallback(
    void * NotificationStructure,
    void * Context
    )
{
    auto notification = static_cast<DeviceInterfaceNotification *>(Context);

    auto const notificationData = static_cast<DEVICE_INTERFACE_CHANGE_NOTIFICATION const *>(NotificationStructure);

    if (notificationData->Event == GUID_DEVICE_INTERFACE_ARRIVAL)
    {
        return notification->Callback(
            DeviceInterfaceNotificationType::Arrival,
            notificationData->SymbolicLinkName,
            notification->Context);
    }
    else if (notificationData->Event == GUID_DEVICE_INTERFACE_REMOVAL)
    {
        return notification->Callback(
            DeviceInterfaceNotificationType::Removal,
            notificationData->SymbolicLinkName,
            notification->Context);
    }

    return STATUS_SUCCESS;
}
