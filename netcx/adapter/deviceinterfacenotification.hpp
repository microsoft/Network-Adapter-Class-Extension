// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <kptr.h>
#include <KNew.h>

enum class DeviceInterfaceNotificationType
{
    Arrival = 1,
    Removal,
};

#ifdef NETCX_KMDF
using SymbolicLinkNameType = UNICODE_STRING;
using unique_registration_handle = wil::unique_any<void *, decltype(::IoUnregisterPlugPlayNotificationEx), ::IoUnregisterPlugPlayNotificationEx>;
using NotificationCallbackType = DRIVER_NOTIFICATION_CALLBACK_ROUTINE;
#else
using SymbolicLinkNameType = wchar_t;
using unique_registration_handle = wil::unique_hcmnotification;
using NotificationCallbackType = wistd::remove_pointer_t<PCM_NOTIFY_CALLBACK>;
#endif

NotificationCallbackType
    NotificationCallback;

typedef
NTSTATUS
DEVICE_INTERFACE_NOTIFICATION_CALLBACK(
    DeviceInterfaceNotificationType NotificationType,
    SymbolicLinkNameType * SymbolicLinkName,
    void * Context
);

struct DeviceInterfaceNotification : PAGED_OBJECT<'NPnP'>
{
    void *
        Context{ nullptr };

    DEVICE_INTERFACE_NOTIFICATION_CALLBACK *
        Callback{ nullptr };

    unique_registration_handle
        RegistrationHandle;
};

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
RegisterDeviceInterfaceNotification(
    _In_ GUID const & DeviceInterfaceGuid,
    _In_ WDFDRIVER Driver,
    _In_ DEVICE_INTERFACE_NOTIFICATION_CALLBACK * Callback,
    _In_ void * Context,
    _Out_ wistd::unique_ptr<DeviceInterfaceNotification> & Notification
);
