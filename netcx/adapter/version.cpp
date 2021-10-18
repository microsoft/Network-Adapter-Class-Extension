// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

//
// This will let us include the auto-generated function table
//
#define NX_DYNAMICS_GENERATE_TABLE
#include <NxApi.hpp>

#include <NetClientDriverConfigurationImpl.hpp>
#include <kstring.h>
#include "NxDevice.hpp"

#include "version.hpp"

#include "NxAdapter.hpp"
#include "verifier.hpp"

#include "version.tmh"
#include "monitor/NxPacketMonitor.hpp"
#include "ExecutionContextStatistics.hpp"

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
void
NETEXPORT(NetAdapterOffloadSetLsoCapabilities_2_1_1)(
    _In_ NET_DRIVER_GLOBALS *,
    _In_ NETADAPTER,
    _In_ void *
)
{
}

TRACELOGGING_DEFINE_PROVIDER(
    g_hNetAdapterCxEtwProvider,
    "Microsoft.Windows.Ndis.NetAdapterCx",
    // {828a1d06-71b8-5fc5-b237-28ae6aeac7c4}
    (0x828a1d06, 0x71b8, 0x5fc5, 0xb2, 0x37, 0x28, 0xae, 0x6a, 0xea, 0xc7, 0xc4));


EXTERN_C
DRIVER_INITIALIZE
    DriverEntry;

EXTERN_C
CONST NPI_MODULEID NPI_MS_NDIS_TRANSLATOR_MODULEID;

static
EVT_WDF_DRIVER_UNLOAD
    EvtDriverUnload;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

static WDFDEVICE   ClassLibraryDevice = NULL;
static ULONG       ClientCount        = 0;
NDIS_WDF_CX_DRIVER NetAdapterCxDriverHandle = NULL;

#define NET_ADAPTER_CX_CONTROL_DEVICE_NAME L"\\Device\\netadaptercx"

WDFWAITLOCK g_RegistrationLock = NULL;


NTSTATUS
CxDriverContext::Init(
    _In_ UNICODE_STRING const * RegistryPath
)
{
    m_registryPath = Rtl::DuplicateUnicodeString(
        *RegistryPath,
        'rDxC');

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_registryPath);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        DriverConfigurationInitialize(),
        "Failed to load driver configurations");

    VerifierInitialize();

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

    context->GetDeviceCollection().ForEach([](NxDevice & nxDevice)
    {
        nxDevice.WaitDeviceResetFinishIfRequested();
    });

    FreeGlobalEcStatistics();
    context->~CxDriverContext();
}

NxCollection<NxDevice> &
CxDriverContext::GetDeviceCollection(
    void
)
{
    return m_deviceCollection;
}

UNICODE_STRING const *
CxDriverContext::GetRegistryPath(
    void
) const
{
    return m_registryPath.get();
}

_Use_decl_annotations_
static
void
EvtDriverUnload(
    WDFDRIVER Driver
)
/*++
Routine Description:
    Driver Unload routine. Client register this routine with WDF at the time
    of WDFDRIVER object creation.

--*/
{
    if (NetAdapterCxDriverHandle) {
        //
        // Deregister with NDIS.
        //
        LogError(FLAG_DRIVER, "Calling NdisWdfDeregisterCx");
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

    PktMonClientUninitialize();

    ExecutionContext::UninitializeSubsystem();
    WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));
    TraceLoggingUnregister(g_hNetAdapterCxEtwProvider);
}

static
NTSTATUS
NetAdapterCxInitialize(
    void
)
{
    return STATUS_SUCCESS;
}

static
void
NetAdapterCxDeinitialize(
    void
)
{
}

static
NTSTATUS
NetAdapterCxBindClient(
    WDF_CLASS_BIND_INFO *  ClassInfo,
    WDF_COMPONENT_GLOBALS * ClientGlobals
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

    ClientGlobals - A pointer to the client driver's WdfDriverGlobals object

Return Value:

    NT_SUCCESS status if the operation succeeds.


--*/
{
    void * functionTable;
    size_t functionCount;

    switch (MAKEVER(ClassInfo->Version.Major, ClassInfo->Version.Minor))
    {
        case MAKEVER(1,0):
        case MAKEVER(1,1):
        case MAKEVER(1,2):
        case MAKEVER(1,3):
        case MAKEVER(1,4):
            DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                "\n\n"
                "**********************************************************\n"
                "* NetAdapterCx 1.%u (Preview) client detected.            \n"
                "* Recompile your source code and target NetAdapterCx 2.%u.\n"
                "**********************************************************\n"
                "\n",
                ClassInfo->Version.Minor,
                NETCX_ADAPTER_MINOR_VERSION);

            __fallthrough;

        default:

            return STATUS_NOT_SUPPORTED;

        case MAKEVER(2,0):
        case MAKEVER(2,1):

            if (ClassInfo->Version.Build > 0)
            {
                DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                    "\n\n"
                    "**********************************************************\n"
                    "* NetAdapterCx 2 (Preview) client detected.               \n"
                    "* Recompile your source code and target NetAdapterCx 2.x  \n"
                    "* (Stable) or NetAdapterCx 2.2 (Preview or Stable).       \n"
                    "**********************************************************\n"
                    "\n");

                return STATUS_NOT_SUPPORTED;
            }

            __fallthrough;

        case MAKEVER(NETCX_ADAPTER_MAJOR_VERSION, NETCX_ADAPTER_MINOR_VERSION):

            switch (ClassInfo->Version.Build)
            {

                case 0:
                    __fallthrough;

                case NETCX_ADAPTER_BUILD_VERSION:
                    functionTable = &NetVersion.Functions;
                    functionCount = NetVersion.FuncCount;
                    break;

                default:
                    return STATUS_NOT_SUPPORTED;
            }

            if (ClassInfo->Version.Build == NETCX_ADAPTER_BUILD_VERSION &&
                ClassInfo->FunctionTableCount != functionCount)
            {
                //
                // If a client driver is using the latest preview version of NetAdapterCx we only
                // let it bind if it is targeting the latest preview the OS supports
                //

                DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                        "\n\n************************* \n"
                        "* NetAdapterCx detected a function count mismatch. The driver \n"
                        "* using this extension will not load until it is re-compiled \n"
                        "* with the latest version of the extension's header and libs \n");
                DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
                        "* Actual function table count  : %u \n"
                        "* Expected function table count: %u \n"
                        "*************************** \n\n",
                        ClassInfo->FunctionTableCount,
                        NetFunctionTableNumEntries);

                return STATUS_INVALID_PARAMETER;
            }
    }

    //
    // Verify our function table has at least the amount of entries the class extension
    // is requesting. If not we messed up the version check above
    //
    NT_FRE_ASSERT(functionCount >= ClassInfo->FunctionTableCount);

    //
    // Setup the function table
    //
    #pragma warning(suppress:22107) //The analysis code identify buffer size
    RtlCopyMemory(ClassInfo->FunctionTable,
                  functionTable,
                  ClassInfo->FunctionTableCount * sizeof(void *));

    //
    // Allocate driver globals
    //
    auto pGlobals = (NX_PRIVATE_GLOBALS *) ExAllocatePoolWithTag(NonPagedPoolNx,
                                                           sizeof(NX_PRIVATE_GLOBALS),
                                                           NETADAPTERCX_TAG);

    if (pGlobals == NULL) {
        LogError(FLAG_DRIVER, "Failed to allocate globals  STATUS_INSUFFICIENT_RESOURCES");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(pGlobals, sizeof(NX_PRIVATE_GLOBALS));
    pGlobals->Signature = NX_PRIVATE_GLOBALS_SIG;

    //
    // If driver verifier is enabled on the client, enable Cx runtime
    // verification checks automatically
    //
    pGlobals->CxVerifierOn = (0 != MmIsDriverVerifyingByAddress(
                                            (void *)(ClassInfo->FunctionTable)));


    if (pGlobals->CxVerifierOn &&
        ! MmIsDriverVerifyingByAddress((void *)&DriverEntry))
    {
        //
        // If DV is enabled on client but not on Cx, break into the debugger
        // to warn the developer
        //
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "\n\n************************* \n"
            "* Driver verifier should be enabled on both client and Cx\n");
        DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL,
            "* If not, DV may report false violations while verifiying NDIS rules\n"
            "*************************** \n\n");
        NxDbgBreak(pGlobals);
    }

    pGlobals->ClientDriverGlobals = reinterpret_cast<WDF_DRIVER_GLOBALS *>(ClientGlobals);
    pGlobals->ClientVersion = ClassInfo->Version;

    *((NET_DRIVER_GLOBALS **)ClassInfo->ClassBindInfo) = &pGlobals->Public;
    return STATUS_SUCCESS;
}

static
void
NetAdapterCxUnbindClient(
    WDF_CLASS_BIND_INFO * ClassInfo,
    WDF_COMPONENT_GLOBALS * ClientGlobals
)
/*++

Routine Description:

    A unbind call that is called by Wdf to unbind the client from NetAdapterCx.

Arguments:

    ClassInfo - A pointer to the WDF_CLASS_BIND_INFO structure

    ClientGlobals - Unused parameter

--*/
{
    NET_DRIVER_GLOBALS * publicGlobals;
    UNREFERENCED_PARAMETER(ClientGlobals);

    if (ClassInfo->ClassBindInfo == NULL) {
        return;
    }

    publicGlobals = *((NET_DRIVER_GLOBALS **)ClassInfo->ClassBindInfo);

    if (publicGlobals == NULL) {
        return;
    }
    auto pGlobals = GetPrivateGlobals(*((NET_DRIVER_GLOBALS **)ClassInfo->ClassBindInfo));

    Verifier_VerifyPrivateGlobals(pGlobals);

    ExFreePool(pGlobals);

    *((void **)ClassInfo->ClassBindInfo) = NULL;
}

static
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
    NTSTATUS status = STATUS_SUCCESS;

    WDFDEVICE_INIT * pInit;
    ULONG i;

    pInit = WdfControlDeviceInitAllocate(WdfGetDriver(), &SDDL_DEVOBJ_KERNEL_ONLY);
    if (pInit == NULL) {
        LogError(FLAG_DRIVER, "Failed to allocate ControlDeviceInit");
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
            LogError(FLAG_DRIVER, "WdfDeviceInitAssignName failed %!STATUS!", status);
            break;
        }

        status = WdfDeviceCreate(&pInit, WDF_NO_OBJECT_ATTRIBUTES, &ClassLibraryDevice);

    } while (status == STATUS_OBJECT_NAME_COLLISION);

    if (!NT_SUCCESS(status)) {
        LogError(FLAG_DRIVER, "ControlDevice creation failed, %!STATUS!", status);
        ASSERT(pInit != NULL);
        WdfDeviceInitFree(pInit);
        return status;
    }

    WdfControlFinishInitializing(ClassLibraryDevice);

    LogError(FLAG_DRIVER, "ControlDevice Created WDF 0x%p, WDM 0x%p, Name %S",
               ClassLibraryDevice, WdfDeviceWdmGetDeviceObject(ClassLibraryDevice), Name->Buffer);

    return status;
}

static
void
DeleteControlDevice(
    void
)
/*++

Routine Description:

    This routine deletes the named control device object that was
    created in a previous call to CreateControlDevice

    NOTE: The control device object handle is stored in a global.

--*/
{
    LogError(FLAG_DRIVER, "ControlDevice Being Deleted WDF 0x%p, WDM 0x%p",
               ClassLibraryDevice, WdfDeviceWdmGetDeviceObject(ClassLibraryDevice));
    WdfObjectDelete(ClassLibraryDevice);
    ClassLibraryDevice = NULL;
}

//
// Library registration info
//
static WDF_CLASS_LIBRARY_INFO WdfClassLibraryInfo =
{
    sizeof(WDF_CLASS_LIBRARY_INFO), // Size
    {
        NETCX_ADAPTER_MAJOR_VERSION,   // Major
        NETCX_ADAPTER_MINOR_VERSION,   // Minor
        0,                                  // Build
    },                                      // Version
    NetAdapterCxInitialize,                 // ClassLibraryInitialize
    NetAdapterCxDeinitialize,               // ClassLibraryDeinitialize
    NetAdapterCxBindClient,                 // ClassLibraryBindClient
    NetAdapterCxUnbindClient,               // ClassLibraryUnbindClient
};

_Use_decl_annotations_
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
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

    DECLARE_UNICODE_STRING_SIZE(name, 100);

    TraceLoggingRegister(g_hNetAdapterCxEtwProvider);

    ExecutionContext::InitializeSubsystem();

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
        LogError(FLAG_DRIVER, "WdfDriverCreate failed status = %!STATUS!", status);
        goto Exit;
    }

    CxDriverContext *context = GetCxDriverContextFromHandle(driver);
    new (context) CxDriverContext();

    status = context->Init(RegistryPath);
    if (!NT_SUCCESS(status)) {
        LogError(FLAG_DRIVER, "CxDriverContext::Init failed status = %!STATUS!", status);
        goto Exit;
    }

    status = WdfWaitLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &g_RegistrationLock);
    if (!NT_SUCCESS(status)) {
        LogError(FLAG_DRIVER, "WdfWaitLockCreate failed status = %!STATUS!", status);
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
    // the appropriate time.
    //
    NdisWdfCxCharacteristicsInitialize(chars);

    //
    // Register with NDIS
    //
    LogError(FLAG_DRIVER, "Calling NdisWdfRegisterCx");
    status = NdisWdfRegisterCx(DriverObject,
                               RegistryPath,
                               (NDIS_WDF_CX_DRIVER_CONTEXT)driver,
                               &chars,
                               &netAdapterCxDriverHandleTmp);

    if (!NT_SUCCESS(status)) {
        LogError(FLAG_DRIVER, "NdisWdfRegisterCx failed status %!STATUS!", status);
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
        LogError(FLAG_DRIVER, "WdfRegisterClassLibrary failed status %!STATUS!", status);
        goto Exit;
    }

    //
    // Store the NDIS_WDF_CX_HANDLE returned by Ndis.sys in a global variable
    // This handle is later passed to Ndis in calls such as
    // NdisWdfRegisterMiniportDriver , NdisWdfDeregisterCx, etc.
    //
    NetAdapterCxDriverHandle = netAdapterCxDriverHandleTmp;

    NxDevice::GetTriageInfo();

    status = PktMonClientInitializeEx(
        &NPI_MS_NDIS_TRANSLATOR_MODULEID,
        NxPacketMonitor::EnumerateAndRegisterAdapters,
        NxPacketMonitor::EnumerateAndUnRegisterAdapters
    );

    if (!NT_SUCCESS(status)) {
        LogError(FLAG_ADAPTER, "Packet Monitor failed status %!STATUS!", status);
        //
        // This is intentionally. If fail, check whether NPI_MODULEID is accessible
        // and whether the callback function safely enumerates components.
        //
        status = STATUS_SUCCESS;
    }

Exit:

     if (!NT_SUCCESS(status)) {
         if (deleteControlDeviceOnError) {
             DeleteControlDevice();
         }

         ExecutionContext::UninitializeSubsystem();
         TraceLoggingUnregister(g_hNetAdapterCxEtwProvider);
         WPP_CLEANUP(DriverObject);
     }

     return status;
}

_Use_decl_annotations_
bool
NX_PRIVATE_GLOBALS::IsClientVersionGreaterThanOrEqual(
    WDF_MAJOR_VERSION Major,
    WDF_MINOR_VERSION Minor
)
{
    if (ClientVersion.Major > Major || (ClientVersion.Major == Major && ClientVersion.Minor >= Minor))
    {
        return true;
    }

    return false;
}
