// Copyright (C) Microsoft Corporation. All rights reserved.
#include "Nx.hpp"
#include "DeviceInterfaceNotification.hpp"
#include "DeviceInterfaceNotification.tmh"

_Use_decl_annotations_
NTSTATUS
RegisterDeviceInterfaceNotification(
    GUID const & DeviceInterfaceGuid,
    WDFDRIVER Driver,
    DEVICE_INTERFACE_NOTIFICATION_CALLBACK * Callback,
    void * Context,
    wistd::unique_ptr<DeviceInterfaceNotification> & Notification
)
{
    auto notification = wil::make_unique_nothrow<DeviceInterfaceNotification>();

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INSUFFICIENT_RESOURCES,
        !notification,
        "Failed to allocate memory for device interface notification context");

    notification->Context = Context;
    notification->Callback = Callback;

#ifdef NETCX_KMDF
    CX_RETURN_IF_NOT_NT_SUCCESS(
        IoRegisterPlugPlayNotification(
            EventCategoryDeviceInterfaceChange,
            0,
            (void *)&DeviceInterfaceGuid,
            WdfDriverWdmGetDriverObject(Driver),
            NotificationCallback,
            notification.get(),
            &notification->RegistrationHandle));
#else
    UNREFERENCED_PARAMETER(Driver);

    CM_NOTIFY_FILTER filter{};
    filter.cbSize = sizeof(filter);
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    filter.u.DeviceInterface.ClassGuid = DeviceInterfaceGuid;

    CONFIGRET cr = CM_Register_Notification(
        &filter,
        notification.get(),
        NotificationCallback,
        &notification->RegistrationHandle);

    CX_RETURN_NTSTATUS_IF_MSG(
        NTSTATUS_FROM_WIN32(
            CM_MapCrToWin32Err(
                cr,
                ERROR_UNIDENTIFIED_ERROR)),
        cr != CR_SUCCESS,
        "Failed to register device interface change notification");
#endif

    Notification = wistd::move(notification);

    return STATUS_SUCCESS;
}
