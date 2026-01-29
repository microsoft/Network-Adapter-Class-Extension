// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"
#include <kwaitevent.h>
#include "netadaptercx_triage.h"
#include "ResetInterface.hpp"
#include "ResetInterfaceKm.tmh"

typedef struct _FUNCTION_LEVEL_RESET_PARAMETERS
{
    FUNCTION_LEVEL_DEVICE_RESET_PARAMETERS
        CompletionParameters;

    KAutoEvent
        Event;

    NTSTATUS
        Status;

} FUNCTION_LEVEL_RESET_PARAMETERS;

static
void
_FunctionLevelResetCompletion(
    _In_ NTSTATUS Status,
    _Inout_opt_ void * Context
);

_Use_decl_annotations_
void
ResetInterface::QueryDeviceResetInterface(
    WDFDEVICE Device
)
/*++

Routine description:

    This routine is used to Query for the Device Reset Interface during Adapter Initialization.
    If the underying bus supports the interface then we will store the Reset Function Pointers
    so that it can be used to do FLDR or PLDR

--*/
{
    DEVICE_RESET_INTERFACE_STANDARD resetInterface;
    RtlZeroMemory(&resetInterface, sizeof(resetInterface));
    resetInterface.Size = sizeof(resetInterface);
    resetInterface.Version = 1;

    NTSTATUS status = WdfFdoQueryForInterface(
        Device,
        &GUID_DEVICE_RESET_INTERFACE_STANDARD,
        (PINTERFACE)&resetInterface,
        sizeof(resetInterface),
        1,
        NULL);

    if (!NT_SUCCESS(status))
    {
        LogWarning(FLAG_DEVICE,
            "QueryDeviceResetInterface failed %!STATUS!. WDFDEVICE=%p", status, Device);
    }
    else
    {
        m_deviceResetSupported = resetInterface.DeviceReset != nullptr;
        m_supportedResetTypes = resetInterface.SupportedResetTypes;
        m_resetInterfaceContext = resetInterface.Context;
        m_resetHandler = resetInterface.DeviceReset;
        m_interfaceDereferenceHandler = resetInterface.InterfaceDereference;
    }
}

_Use_decl_annotations_
NTSTATUS
ResetInterface::TriggerFldr(
    void
) const
{
    FUNCTION_LEVEL_RESET_PARAMETERS resetParameters{};
    resetParameters.CompletionParameters.Size = sizeof(FUNCTION_LEVEL_DEVICE_RESET_PARAMETERS);
    resetParameters.CompletionParameters.DeviceResetCompletion = _FunctionLevelResetCompletion;
    resetParameters.CompletionParameters.CompletionContext = &resetParameters;

    NTSTATUS ntStatus = m_resetHandler(
        m_resetInterfaceContext,
        FunctionLevelDeviceReset,
        0,
        &resetParameters.CompletionParameters);

    // Wait for the function-level device reset to complete.
    if (ntStatus == STATUS_PENDING)
    {
        resetParameters.Event.Wait();
        ntStatus = resetParameters.Status;
    }

    return ntStatus;
}

_Use_decl_annotations_
void
_FunctionLevelResetCompletion(
    NTSTATUS Status,
    void * Context
)
/*++

Routine description:

    Completion routine for the Function Level Reset. This function will be called
    by the ACPI driver once the Function Level Reset has been completed.

--*/
{
    auto parameters = reinterpret_cast<FUNCTION_LEVEL_RESET_PARAMETERS *>(Context);
    parameters->Status = Status;
    parameters->Event.Set();
}

_Use_decl_annotations_
NTSTATUS
ResetInterface::CaptureLiveKernelDump(
    WDFDRIVER Driver,
    WDFDEVICE Device,
    void * Context,
    DBGK_LIVEDUMP_CALLBACK_ROUTINE * Callback
) const
{
    DEVICE_OBJECT *deviceObj = WdfDeviceWdmGetDeviceObject(Device);
    DRIVER_OBJECT *driverObj = WdfDriverWdmGetDriverObject(Driver);

    //
    // Request full live dump, capture user-mode process pages.
    //
    const DBGK_LIVEDUMP_FLAGS livedumpFlags = {1, 0, 0, 0};

    return DbgkWerCaptureLiveKernelDump(
        L"NetAdapterCx",
        BUGCODE_NETADAPTER_DRIVER,
        NETADAPTERCX_BUGCHECK_DEVICE_RESET,
        reinterpret_cast<ULONG_PTR>(deviceObj),
        reinterpret_cast<ULONG_PTR>(driverObj),
        reinterpret_cast<ULONG_PTR>(nullptr),
        Context,
        Callback,
        livedumpFlags);
}
