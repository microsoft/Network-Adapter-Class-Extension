// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"
#include <ExecutionContextDispatch.h>
#include "ExecutionContextStatistics.hpp"
#include "EcEvents.h"
#include "driver.tmh"

extern
NET_EXECUTION_CONTEXT_DISPATCH
    ExecutionContextDispatch;

INITCODE EXTERN_C
DRIVER_INITIALIZE
    DriverEntry;

static PAGEDX
EVT_WDF_DRIVER_UNLOAD
    EvtDriverUnload;

static PAGEDX
EVT_WDF_OBJECT_CONTEXT_CLEANUP
    EvtDriverCleanup;

static PAGEDX
KLOADER_MODULE_QUERY_DISPATCH_TABLE
    OnQueryDispatchTable;

DECLARE_UNICODE_STRING_SIZE(ModuleDeviceName, ARRAYSIZE(L"\\Device\\ExecutionContextXYZW"));
WDFDEVICE ModuleDevice;

_IRQL_requires_(PASSIVE_LEVEL)
INITCODE
NTSTATUS
CreateModuleDevice(
    void
)
{
    wil::unique_any<PWDFDEVICE_INIT, decltype(WdfDeviceInitFree), WdfDeviceInitFree> moduleDeviceInit
    {
        WdfControlDeviceInitAllocate(
            WdfGetDriver(),
            &SDDL_DEVOBJ_KERNEL_ONLY)
    };

    if (!moduleDeviceInit)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS createDeviceStatus;

    for (size_t i = 1; i <= 9999; i++)
    {
        RETURN_IF_NOT_SUCCESS(
            RtlUnicodeStringPrintf(
                &ModuleDeviceName,
                L"\\Device\\ExecutionContext%04u",
                i));

        RETURN_IF_NOT_SUCCESS(
            WdfDeviceInitAssignName(
                moduleDeviceInit.get(),
                &ModuleDeviceName));

        createDeviceStatus = WdfDeviceCreate(
            moduleDeviceInit.addressof(),
            WDF_NO_OBJECT_ATTRIBUTES,
            &ModuleDevice);

        if (createDeviceStatus != STATUS_OBJECT_NAME_COLLISION)
        {
            break;
        }
    }

    RETURN_IF_NOT_SUCCESS(createDeviceStatus);

    WdfControlFinishInitializing(ModuleDevice);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
INITCODE
NTSTATUS
DriverEntry(
    DRIVER_OBJECT * DriverObject,
    UNICODE_STRING * RegistryPath)
{
    WDF_OBJECT_ATTRIBUTES driverAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&driverAttributes);
    driverAttributes.EvtCleanupCallback = EvtDriverCleanup;

    WDF_DRIVER_CONFIG driverConfig;
    WDF_DRIVER_CONFIG_INIT(
        &driverConfig,
        WDF_NO_EVENT_CALLBACK);
    driverConfig.EvtDriverUnload = EvtDriverUnload;
    driverConfig.DriverInitFlags |= WdfDriverInitNonPnpDriver;

    auto const driverCreateStatus = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &driverAttributes,
        &driverConfig,
        WDF_NO_HANDLE);

    if (driverCreateStatus != STATUS_SUCCESS)
    {
        return driverCreateStatus;
    }

    EventRegisterMicrosoft_Windows_Network_ExecutionContext();
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    RETURN_IF_NOT_SUCCESS(
        InitializeGlobalEcStatistic());

    RETURN_IF_NOT_SUCCESS(
        CreateModuleDevice());

    KLOADER_MODULE_CHARACTERISTICS moduleCharacteristics;
    moduleCharacteristics.Size = sizeof(moduleCharacteristics);
    moduleCharacteristics.ModuleID = EXECUTION_CONTEXT_MODULE_ID;
    moduleCharacteristics.ModuleDeviceName = ModuleDeviceName;
    moduleCharacteristics.QueryDispatchTable = OnQueryDispatchTable;

    RETURN_IF_NOT_SUCCESS(
        KLoaderRegisterModule(
            DriverObject,
            RegistryPath,
            nullptr,
            &moduleCharacteristics));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGEDX
void
EvtDriverUnload(
    WDFDRIVER
)
{
    PAGED_CODE();

    if (ModuleDevice != WDF_NO_HANDLE)
    {
        WdfObjectDelete(ModuleDevice);
    }
}

_Use_decl_annotations_
PAGEDX
void
EvtDriverCleanup(
    WDFOBJECT Driver
)
{
    PAGED_CODE();

    FreeGlobalEcStatistics();

    WPP_CLEANUP(
        WdfDriverWdmGetDriverObject(
            static_cast<WDFDRIVER>(Driver)));
    EventUnregisterMicrosoft_Windows_Network_ExecutionContext();
}

_Use_decl_annotations_
PAGEDX
NTSTATUS
OnQueryDispatchTable(
    void *,
    GUID const *DispatchTableID,
    KLOADER_DISPATCH_TABLE const **Dispatch
)
{
    if (*DispatchTableID != EXECUTION_CONTEXT_DISPATCH_TABLE_ID)
    {
        return STATUS_NOINTERFACE;
    }

    *Dispatch = reinterpret_cast<KLOADER_DISPATCH_TABLE *>(&ExecutionContextDispatch);

    return STATUS_SUCCESS;
}
