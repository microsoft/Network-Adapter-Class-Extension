// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module implements version specific library.

--*/

//
// This will let us include the auto-generated function table
//
#define  NX_DYNAMICS_GENERATE_TABLE   1
#include "nx.hpp"
#include <NetClientDriverConfigurationImpl.hpp>

#include "version.tmh"

TRACELOGGING_DEFINE_PROVIDER(
    g_hNetAdapterCxEtwProvider,
    "Microsoft.Windows.Ndis.NetAdapterCx",
    // {828a1d06-71b8-5fc5-b237-28ae6aeac7c4}
    (0x828a1d06, 0x71b8, 0x5fc5, 0xb2, 0x37, 0x28, 0xae, 0x6a, 0xea, 0xc7, 0xc4));

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

static WDFDEVICE   ClassLibraryDevice = NULL;
static ULONG       ClientCount        = 0;
NDIS_WDF_CX_DRIVER NetAdapterCxDriverHandle = NULL;

#define NET_ADAPTER_CX_CONTROL_DEVICE_NAME L"\\Device\\netadaptercx"

WDFWAITLOCK g_RegistrationLock = NULL;

//
// Library registration info
//
WDF_CLASS_LIBRARY_INFO WdfClassLibraryInfo =
{
    sizeof(WDF_CLASS_LIBRARY_INFO), // Size
    {
        NETADAPTER_CURRENT_MAJOR_VERSION,   // Major
        NETADAPTER_CURRENT_MINOR_VERSION,   // Minor
        0,                                  // Build
    },                                      // Version
    NetAdapterCxInitialize,                 // ClassLibraryInitialize
    NetAdapterCxDeinitialize,               // ClassLibraryDeinitialize
    NetAdapterCxBindClient,                 // ClassLibraryBindClient
    NetAdapterCxUnbindClient,               // ClassLibraryUnbindClient
};

extern "C"
NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT  DriverObject,
    __in PUNICODE_STRING  RegistryPath
    )
/*++
Routine Description:
    The standard DriverEntry routine for any driver.
--*/
{
    NTSTATUS                             status;
    WDF_DRIVER_CONFIG                    config;
    WDFDRIVER                            driver;
    NDIS_WDF_CX_CHARACTERISTICS          chars;
    BOOLEAN                              deleteControlDeviceOnError = FALSE;
    NDIS_WDF_CX_DRIVER                   netAdapterCxDriverHandleTmp;

    WPP_INIT_TRACING(DriverObject, RegistryPath);
    FuncEntry(FLAG_DRIVER);

    DECLARE_UNICODE_STRING_SIZE(name, 100);

    TraceLoggingRegister(g_hNetAdapterCxEtwProvider);

    WDF_DRIVER_CONFIG_INIT(&config, NULL);
    config.DriverInitFlags = WdfDriverInitNonPnpDriver;
    config.EvtDriverUnload = EvtDriverUnload;

    WDF_OBJECT_ATTRIBUTES driverAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&driverAttributes, CxDriverContext);

    driverAttributes.EvtDestroyCallback = CxDriverContext::Destroy;

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &driverAttributes,
                             &config,
                             &driver);
    if (!NT_SUCCESS(status)) {
        LogInitMsg(TRACE_LEVEL_ERROR, "WdfDriverCreate failed status = %!STATUS!", status);
        goto Exit;
    }

    CxDriverContext *context = GetCxDriverContextFromHandle(driver);
    new (context) CxDriverContext();

    status = context->Init();
    if (!NT_SUCCESS(status)) {
        LogInitMsg(TRACE_LEVEL_ERROR, "CxDriverContext::Init failed status = %!STATUS!", status);
        goto Exit;
    }

    status = WdfWaitLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &g_RegistrationLock);
    if (!NT_SUCCESS(status)) {
        LogInitMsg(TRACE_LEVEL_ERROR, "WdfWaitLockCreate failed status = %!STATUS!", status);
        goto Exit;
    }

    //
    // create a named control object.
    // The name is passed to the WdfRegisterClassLibrary api.
    // It is important to create a control object so that WDF can keep
    // a handle open on it. This prevents someone running 'sc stop NetAdapterCx'
    // unloading the driver.
    //
    status = CreateControlDevice(&name);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    deleteControlDeviceOnError = TRUE;

    //
    // Initialize a NDIS CX CHARACTERISTIC structure and register
    // Cx callbacks with NDIS. NDIS.sys calls these callbacks at
    // the appropiate time.
    //
    NDIS_WDF_CX_CHARACTERISTICS_INIT(&chars);
    chars.EvtCxGetDeviceObject               = NxAdapter::_EvtNdisGetDeviceObject;
    chars.EvtCxGetNextDeviceObject           = NxAdapter::_EvtNdisGetNextDeviceObject;
    chars.EvtCxGetAssignedFdoName            = NxAdapter::_EvtNdisGetAssignedFdoName;
    chars.EvtCxPowerReference                = NxAdapter::_EvtNdisPowerReference;
    chars.EvtCxPowerDereference              = NxAdapter::_EvtNdisPowerDereference;
    chars.EvtCxPowerAoAcEngage               = NxAdapter::_EvtNdisAoAcEngage;
    chars.EvtCxPowerAoAcDisengage            = NxAdapter::_EvtNdisAoAcDisengage;
    chars.EvtCxGetNdisHandleFromDeviceObject = NxAdapter::_EvtNdisWdmDeviceGetNdisAdapterHandle;
    chars.EvtCxUpdatePMParameters            = NxAdapter::_EvtNdisUpdatePMParameters;
    chars.EvtCxAllocateMiniportBlock         = NxAdapter::_EvtNdisAllocateMiniportBlock;
    chars.EvtCxMiniportCompleteAdd           = NxAdapter::_EvtNdisMiniportCompleteAdd;
    chars.EvtCxDeviceStartComplete           = NxAdapter::_EvtNdisDeviceStartComplete;

    //
    // Register with NDIS
    //
    LogInitMsg(TRACE_LEVEL_VERBOSE, "Calling NdisWdfRegisterCx");
    status = NdisWdfRegisterCx(DriverObject,
                               RegistryPath,
                               (NDIS_WDF_CX_DRIVER_CONTEXT)driver,
                               &chars,
                               &netAdapterCxDriverHandleTmp);

    if (!NT_SUCCESS(status)) {
        LogInitMsg(TRACE_LEVEL_ERROR, "NdisWdfRegisterCx failed status %!STATUS!", status);
        goto Exit;
    }

    //
    // Register this class extension with WDF
    // After this point we don't want any failure points
    //
    status = WdfRegisterClassLibrary(&WdfClassLibraryInfo,
        RegistryPath,
        &name);
    if (!NT_SUCCESS(status)) {
        LogInitMsg(TRACE_LEVEL_ERROR, "WdfRegisterClassLibrary failed status %!STATUS!", status);
        goto Exit;
    }

    //
    // Store the NDIS_WDF_CX_HANDLE returned by Ndis.sys in a global variable
    // This handle is later passed to Ndis in calls such as
    // NdisWdfRegisterMiniportDriver , NdisWdfDeregisterCx, etc.
    //
    NetAdapterCxDriverHandle = netAdapterCxDriverHandleTmp;

Exit:

     if (!NT_SUCCESS(status)) {
         if (deleteControlDeviceOnError) {
             DeleteControlDevice();
         }

         FuncExit(FLAG_DRIVER);
         TraceLoggingUnregister(g_hNetAdapterCxEtwProvider);
         WPP_CLEANUP(DriverObject);
     } else {
         FuncExit(FLAG_DRIVER);
     }
     return status;
}

NTSTATUS
CxDriverContext::Init()
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        DriverConfigurationInitialize(),
        "Failed to load driver configurations");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        TranslationAppFactory.Initialize(),
        "Failed to initialize the translation app");

    return STATUS_SUCCESS;
}

void
CxDriverContext::Destroy(
    _In_ WDFOBJECT Driver
    )
{
    CxDriverContext *context = GetCxDriverContextFromHandle(Driver);

    context->~CxDriverContext();
}

VOID
EvtDriverUnload(
    _In_ WDFDRIVER Driver
    )
/*++
Routine Description:
    Driver Unload routine. Client register this routine with WDF at the time
    of WDFDRIVER object creation.

--*/
{
    FuncEntry(FLAG_DRIVER);

    if (NetAdapterCxDriverHandle) {
        //
        // Deregister with NDIS.
        //
        LogInitMsg(TRACE_LEVEL_VERBOSE, "Calling NdisWdfDeregisterCx");
        NdisWdfDeregisterCx(NetAdapterCxDriverHandle);
        NetAdapterCxDriverHandle = NULL;
    }

    NT_ASSERT(ClientCount == 0);
    if (ClassLibraryDevice != NULL) {
        //
        // Delete the Control device object that was created in the
        // DriverEntry routine.
        //
        WdfObjectDelete(ClassLibraryDevice);
        ClassLibraryDevice = NULL;
    }

    FuncExit(FLAG_DRIVER);
    WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));
    TraceLoggingUnregister(g_hNetAdapterCxEtwProvider);
}

NTSTATUS
NetAdapterCxInitialize(
    VOID
    )
{
    FuncEntry(FLAG_DRIVER);
    FuncExit(FLAG_DRIVER);
    return STATUS_SUCCESS;
}

VOID
NetAdapterCxDeinitialize(
    VOID
    )
{
    FuncEntry(FLAG_DRIVER);
    FuncExit(FLAG_DRIVER);
}

constexpr
ULONG64
MAKEVER(
    ULONG major,
    ULONG minor
    )
{
    return (static_cast<ULONG64>(major) << 16) | minor;
}

NTSTATUS
NetAdapterCxBindClient(
    PWDF_CLASS_BIND_INFO   ClassInfo,
    PWDF_COMPONENT_GLOBALS ClientGlobals
    )
/*++
Routine Description:
    This call is called by WDF to bind a NetAdapterCx client driver to the NetAdapterCx.
    This allows the client to be able to call NetAdapterCx provided methods seemlessly
    This call happens before the client's DriverEntry routine is called.

Arguments:

    ClassInfo - A pointer to the WDF_CLASS_BIND_INFO structure that has
        version information, class name. This function fills all the controller
        extension function pointers in this structure.

    ClientGlobals - Unused

Return Value:

    NT_SUCCESS status if the operation succeeds.


--*/
{
    FuncEntry(FLAG_DRIVER);

    PNX_PRIVATE_GLOBALS      pGlobals;

    UNREFERENCED_PARAMETER(ClientGlobals);

    switch (MAKEVER(ClassInfo->Version.Major, ClassInfo->Version.Minor))
    {
        case MAKEVER(1,0):
        case MAKEVER(1,1):

            DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                "\n\n"
                "**********************************************************\n"
                "* NetAdapterCx 1.%u (Preview) client detected.            \n"
                "* Recompile your source code and target NetAdapterCx 1.%u.\n"
                "**********************************************************\n"
                "\n",
                ClassInfo->Version.Minor,
                NETADAPTER_CURRENT_MINOR_VERSION
            );

            __fallthrough;

        default:

            FuncExit(FLAG_DRIVER);
            return STATUS_NOT_SUPPORTED;

        case MAKEVER(NETADAPTER_CURRENT_MAJOR_VERSION, NETADAPTER_CURRENT_MINOR_VERSION):

            // The current in-development version has a changing ABI.
            // To detect common ABI mismatches, check if the function table looks correct.

            if (ClassInfo->FunctionTableCount != NetFunctionTableNumEntries) {
                DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                        "\n\n************************* \n"
                        "* NetAdapterCx detected a function count mismatch. The driver \n"
                        "* using this extension will not load until it is re-compiled \n"
                        "* with the latest version of the extension's header and libs \n"
                        );
                DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                        "* Actual function table count  : %u \n"
                        "* Expected function table count: %u \n"
                        "*************************** \n\n",
                        ClassInfo->FunctionTableCount,
                        NetFunctionTableNumEntries);

                FuncExit(FLAG_DRIVER);
                return STATUS_INVALID_PARAMETER;
            }
    }

    //
    // Setup the function table
    //
    #pragma warning(suppress:22107) //The analysis code identify buffer size
    RtlCopyMemory(ClassInfo->FunctionTable,
                  &NetVersion.Functions,
                  ClassInfo->FunctionTableCount * sizeof(PVOID));

    //
    // Allocate driver globals
    //
    pGlobals = (PNX_PRIVATE_GLOBALS) ExAllocatePoolWithTag(NonPagedPoolNx,
                                                           sizeof(NX_PRIVATE_GLOBALS),
                                                           NETADAPTERCX_TAG);

    if (pGlobals == NULL) {
        LogInitMsg(TRACE_LEVEL_ERROR, "Failed to allocate globals  STATUS_INSUFFICIENT_RESOURCES");
        FuncExit(FLAG_DRIVER);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(pGlobals, sizeof(NX_PRIVATE_GLOBALS));
    pGlobals->Signature = NX_PRIVATE_GLOBALS_SIG;

    //
    // If driver verifier is enabled on the client, enable Cx runtime
    // verification checks automatically
    //
    pGlobals->CxVerifierOn = (0 != MmIsDriverVerifyingByAddress(
                                            (PVOID)(ClassInfo->FunctionTable)));


    if (pGlobals->CxVerifierOn &&
        ! MmIsDriverVerifyingByAddress((PVOID)&DriverEntry))
    {
        //
        // If DV is enabled on client but not on Cx, break into the debugger
        // to warn the developer
        //
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "\n\n************************* \n"
            "* Driver verifier should be enabled on both client and Cx\n"
            );
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "* If not, DV may report false violations while verifiying NDIS rules\n"
            "*************************** \n\n");
        NxDbgBreak(pGlobals);
    }

    *((PNET_DRIVER_GLOBALS*)ClassInfo->ClassBindInfo) = &pGlobals->Public;
    FuncExit(FLAG_DRIVER);
    return STATUS_SUCCESS;
}

VOID
NetAdapterCxUnbindClient(
    PWDF_CLASS_BIND_INFO   ClassInfo,
    PWDF_COMPONENT_GLOBALS ClientGlobals
    )
/*++

Routine Description:

    A unbind call that is called by Wdf to unbind the client from NetAdapterCx.

Arguments:

    ClassInfo - A pointer to the WDF_CLASS_BIND_INFO structure

    ClientGlobals - Unused parameter

--*/
{
    FuncEntry(FLAG_DRIVER);
    PNX_PRIVATE_GLOBALS    pGlobals;
    PNET_DRIVER_GLOBALS    publicGlobals;
    UNREFERENCED_PARAMETER(ClientGlobals);

    if (ClassInfo->ClassBindInfo == NULL) {
        FuncExit(FLAG_DRIVER);
        return;
    }

    publicGlobals = *((PNET_DRIVER_GLOBALS*)ClassInfo->ClassBindInfo);

    if (publicGlobals == NULL) {
        FuncExit(FLAG_DRIVER);
        return;
    }
    pGlobals = GetPrivateGlobals(*((PNET_DRIVER_GLOBALS*)ClassInfo->ClassBindInfo));

    Verifier_VerifyPrivateGlobals(pGlobals);

    ExFreePool(pGlobals);

    *((PVOID*)ClassInfo->ClassBindInfo) = NULL;

    FuncExit(FLAG_DRIVER);
}

NTSTATUS
CreateControlDevice(
    _Inout_ PUNICODE_STRING Name
    )
/*++

Routine Description:

    This routine creates a named control device object.
    It runs in a loop trying to create a \\Device\\NetAdapterCx01, \\Device\\NetAdapterCx02, ...
    until it is succesful.

    Is stores the control device object in a global variable.

Arguments:

    Name - output - returns the name of the control device object that was
        created.

--*/
{
    FuncEntry(FLAG_DRIVER);
    NTSTATUS status = STATUS_SUCCESS;

    PWDFDEVICE_INIT pInit;
    ULONG i;

    pInit = WdfControlDeviceInitAllocate(WdfGetDriver(), &SDDL_DEVOBJ_KERNEL_ONLY);
    if (pInit == NULL) {
        LogInitMsg(TRACE_LEVEL_ERROR, "Failed to allocate ControlDeviceInit");
        FuncExit(FLAG_DRIVER);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    i = 0;

    do {
        status = RtlUnicodeStringPrintf(Name, L"%s%d", NET_ADAPTER_CX_CONTROL_DEVICE_NAME, i++);
        if (!NT_SUCCESS(status)) {
            break;
        }

        status = WdfDeviceInitAssignName(pInit, Name);
        if (!NT_SUCCESS(status)) {
            LogInitMsg(TRACE_LEVEL_ERROR, "WdfDeviceInitAssignName failed %!STATUS!", status);
            break;
        }

        status = WdfDeviceCreate(&pInit, WDF_NO_OBJECT_ATTRIBUTES, &ClassLibraryDevice);

    } while (status == STATUS_OBJECT_NAME_COLLISION);

    if (!NT_SUCCESS(status)) {
        LogInitMsg(TRACE_LEVEL_ERROR, "ControlDevice creation failed, %!STATUS!", status);
        ASSERT(pInit != NULL);
        WdfDeviceInitFree(pInit);
        FuncExit(FLAG_DRIVER);
        return status;
    }

    WdfControlFinishInitializing(ClassLibraryDevice);

    LogInitMsg(TRACE_LEVEL_VERBOSE, "ControlDevice Created WDF 0x%p, WDM 0x%p, Name %S",
               ClassLibraryDevice, WdfDeviceWdmGetDeviceObject(ClassLibraryDevice), Name->Buffer);

    FuncExit(FLAG_DRIVER);
    return status;
}

VOID
DeleteControlDevice(
    VOID
    )
/*++

Routine Description:

    This routine deletes the named control device object that was
    created in a previous call to CreateControlDevice

    NOTE: The control device object handle is stored in a global.

--*/
{
    FuncEntry(FLAG_DRIVER);
    LogInitMsg(TRACE_LEVEL_VERBOSE, "ControlDevice Being Deleted WDF 0x%p, WDM 0x%p",
               ClassLibraryDevice, WdfDeviceWdmGetDeviceObject(ClassLibraryDevice));
    WdfObjectDelete(ClassLibraryDevice);
    ClassLibraryDevice = NULL;
    FuncExit(FLAG_DRIVER);
}
