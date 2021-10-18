// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include "WdfObjectCallback.hpp"
#include "NxDevice.hpp"

#include "NxAdapter.hpp"
#include "NxDriver.hpp"
#include "powerpolicy/NxPowerList.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include <ndisguid.h>

#include <net/virtualaddress_p.h>
#include <net/mdl_p.h>
#include <net/logicaladdress_p.h>
#include <net/wifi/exemptionaction_p.h>

#include <wmi/NetDevice.h>

#include "netadaptercx_triage.h"

#include "NxDevice.tmh"

#define NX_WMI_TAG 'mWxN'

static void *
    PLDR_TAG = reinterpret_cast<void *>('RDlP');

static EVT_WDF_DEVICE_PREPARE_HARDWARE EvtCxDevicePrePrepareHardware;
static EVT_WDFCX_DEVICE_PRE_PREPARE_HARDWARE_FAILED_CLEANUP EvtCxDevicePrePrepareHardwareFailedCleanup;
static EVT_WDF_DEVICE_RELEASE_HARDWARE EvtCxDevicePreReleaseHardware;
static EVT_WDF_DEVICE_SURPRISE_REMOVAL EvtCxDevicePreSurpriseRemoval;
static EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtDeviceCleanup;
static EVT_WDFCX_DEVICE_POST_D0_ENTRY_POST_HARDWARE_ENABLED EvtCxDevicePostD0EntryPostHardwareEnabled;
static EVT_WDFCX_DEVICE_PRE_D0_EXIT_PRE_HARDWARE_DISABLED EvtCxDevicePreD0ExitPreHardwareDisabled;
static EVT_WDFCX_DEVICE_PRE_D0_ENTRY EvtCxDevicePreD0Entry;
static EVT_WDFCX_DEVICE_POST_D0_ENTRY EvtCxDevicePostD0Entry;
static EVT_WDFCX_DEVICE_POST_SELF_MANAGED_IO_INIT EvtCxDevicePostSelfManagedIoInit;
static EVT_WDFCX_DEVICE_POST_SELF_MANAGED_IO_RESTART_EX EvtCxDevicePostSelfManagedIoRestartEx;
static EVT_WDFCX_DEVICE_PRE_SELF_MANAGED_IO_SUSPEND_EX EvtCxDevicePreSelfManagedIoSuspendEx;
static EVT_WDFCX_DEVICE_POST_D0_EXIT EvtCxDevicePostD0Exit;

static EVT_WDFCX_DEVICE_PRE_WAKE_FROM_S0_TRIGGERED EvtCxPreWakeFromS0Triggered;
static EVT_WDFCX_DEVICE_POST_DISARM_WAKE_FROM_S0 EvtCxPostDisarmWakeFromS0;
static EVT_WDFCX_DEVICE_PRE_WAKE_FROM_SX_TRIGGERED EvtCxPreWakeFromSxTriggered;
static EVT_WDFCX_DEVICE_POST_DISARM_WAKE_FROM_SX EvtCxPostDisarmWakeFromSx;

static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmIrpPreprocessRoutine;
static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmPnpIrpPreprocessRoutine;
static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmWmiIrpPreprocessRoutine;

// Note: 0x5 and 0x3 were chosen as the device interface signatures because
// ExAllocatePool will allocate memory aligned to at least 8-byte boundary
static void * const NETADAPTERCX_DEVICE_INTERFACE_SIGNATURE = (void *)(0x3);
static void * const CLIENT_DRIVER_DEVICE_INTERFACE_SIGNATURE = (void *)(0x5);

NETADAPTERCX_GLOBAL_TRIAGE_BLOCK g_NetAdapterCxTriageBlock =
{
    /* Signature                       */ NETADAPTERCX_TRIAGE_SIGNATURE,

    /* Version                         */ NETADAPTERCX_TRIAGE_VERSION_2,

    /* Size                            */ sizeof(NETADAPTERCX_GLOBAL_TRIAGE_BLOCK),

    /* StateMachineEngineOffset        */ 0U,

    /* NxDeviceAdapterCollectionOffset */ 0U,

    /* NxAdapterCollectionCountOffset  */ 0U,

    /* NxAdapterLinkageOffset          */ 0U,

    /* ResetDiagnosticsStartAddr       */ 0U,

    /* ResetDiagnosticsSize            */ 0U,
};

NDIS_OID const ndisHandledWdfOids[] =
{
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_PHYSICAL_MEDIUM,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_STATISTICS,
    OID_GEN_TRANSMIT_QUEUE_LENGTH,       // Optional
    OID_GEN_LINK_PARAMETERS,
    OID_GEN_INTERRUPT_MODERATION,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_DEFERRED,             // Optional
    OID_802_3_XMIT_MAX_COLLISIONS,       // Optional
    OID_802_3_RCV_OVERRUN,               // Optional
    OID_802_3_XMIT_UNDERRUN,             // Optional
    OID_802_3_XMIT_HEARTBEAT_FAILURE,    // Optional
    OID_802_3_XMIT_TIMES_CRS_LOST,       // Optional
    OID_802_3_XMIT_LATE_COLLISIONS,      // Optional
};

enum class DeviceInterfaceType
{
    Ndis = 0,
    NetAdapterCx,
    ClientDriver,
};

static
void
_FunctionLevelResetCompletion(
    _In_ NTSTATUS Status,
    _Inout_opt_ void * Context
);

_Use_decl_annotations_
IdleStateMachine *
IdleStateMachineFromDevice(
    WDFDEVICE Object
)
{
    auto nxDevice = GetNxDeviceFromHandle(Object);
    return &nxDevice->IdleStateMachine;
}

_Use_decl_annotations_
NTSTATUS
WdfCxDeviceInitAssignPreprocessorRoutines(
    WDFCXDEVICE_INIT * CxDeviceInit
)
/*++
Routine Description:

    This routine sets the preprocessor routine for the IRPs that NetAdapterCx
    needs to directly forward to NDIS.

--*/
{

    // Only add in this array IRP_MJ_* codes that do not have minor codes
    UCHAR const irpCodes[] =
    {
        IRP_MJ_CREATE,
        IRP_MJ_CLOSE,
        IRP_MJ_DEVICE_CONTROL,
        IRP_MJ_INTERNAL_DEVICE_CONTROL,
        IRP_MJ_WRITE,
        IRP_MJ_READ,
        IRP_MJ_CLEANUP
    };

    for (size_t i = 0; i < ARRAYSIZE(irpCodes); i++)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
                CxDeviceInit,
                EvtWdmIrpPreprocessRoutine,
                irpCodes[i],
                nullptr,
                0),
            "WdfDeviceInitAssignWdmIrpPreprocessCallback failed for 0x%x", irpCodes[i]);
    }

    UCHAR pnpMinorCodes[] =
    {
        IRP_MN_QUERY_REMOVE_DEVICE
    };

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
            CxDeviceInit,
            EvtWdmPnpIrpPreprocessRoutine,
            IRP_MJ_PNP,
            pnpMinorCodes,
            ARRAYSIZE(pnpMinorCodes)),
        "WdfDeviceInitAssignWdmIrpPreprocessCallback failed for IRP_MJ_PNP");

    UCHAR wmiMinorCodes[] =
    {
        IRP_MN_QUERY_ALL_DATA,
        IRP_MN_QUERY_SINGLE_INSTANCE,
        IRP_MN_CHANGE_SINGLE_INSTANCE,
        IRP_MN_CHANGE_SINGLE_ITEM,
        IRP_MN_ENABLE_EVENTS,
        IRP_MN_DISABLE_EVENTS,
        IRP_MN_ENABLE_COLLECTION,
        IRP_MN_DISABLE_COLLECTION,
        IRP_MN_REGINFO,
        IRP_MN_EXECUTE_METHOD,
        IRP_MN_REGINFO_EX,
    };

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
            CxDeviceInit,
            EvtWdmWmiIrpPreprocessRoutine,
            IRP_MJ_SYSTEM_CONTROL,
            wmiMinorCodes,
            ARRAYSIZE(wmiMinorCodes)),
        "WdfCxDeviceInitAssignWdmIrpPreprocessCallback failed for IRP_MJ_SYSTEM_CONTROL");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
SetCxPnpPowerCallbacks(
    WDFCXDEVICE_INIT * CxDeviceInit
)
/*++
Routine Description:

    Register pre and post PnP and Power callbacks.

--*/
{
    WDFCX_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDFCX_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

    //
    // EvtDevicePrepareHardware:
    //
    pnpCallbacks.EvtCxDevicePrePrepareHardware = EvtCxDevicePrePrepareHardware;
    pnpCallbacks.EvtCxDevicePrePrepareHardwareFailedCleanup = EvtCxDevicePrePrepareHardwareFailedCleanup;

    //
    // EvtDeviceReleaseHardware:
    //
    pnpCallbacks.EvtCxDevicePreReleaseHardware = EvtCxDevicePreReleaseHardware;

    //
    // EvtDeviceSurpriseRemoval:
    //
    pnpCallbacks.EvtCxDevicePreSurpriseRemoval = EvtCxDevicePreSurpriseRemoval;

    pnpCallbacks.EvtCxDevicePostD0EntryPostHardwareEnabled = EvtCxDevicePostD0EntryPostHardwareEnabled;
    pnpCallbacks.EvtCxDevicePreD0ExitPreHardwareDisabled = EvtCxDevicePreD0ExitPreHardwareDisabled;

    pnpCallbacks.EvtCxDevicePostSelfManagedIoInit = EvtCxDevicePostSelfManagedIoInit;
    pnpCallbacks.EvtCxDevicePostSelfManagedIoRestartEx = EvtCxDevicePostSelfManagedIoRestartEx;
    pnpCallbacks.EvtCxDevicePreSelfManagedIoSuspendEx = EvtCxDevicePreSelfManagedIoSuspendEx;

    pnpCallbacks.EvtCxDevicePreD0Entry = EvtCxDevicePreD0Entry;
    pnpCallbacks.EvtCxDevicePostD0Entry = EvtCxDevicePostD0Entry;

    pnpCallbacks.EvtCxDevicePostD0Exit = EvtCxDevicePostD0Exit;

    WdfCxDeviceInitSetPnpPowerEventCallbacks(CxDeviceInit, &pnpCallbacks);
}

void
SetCxPowerPolicyCallbacks(
    _In_ WDFCXDEVICE_INIT * CxDeviceInit
)
{
    WDFCX_POWER_POLICY_EVENT_CALLBACKS cxPowerPolicyCallBacks;
    WDFCX_POWER_POLICY_EVENT_CALLBACKS_INIT(&cxPowerPolicyCallBacks);

    cxPowerPolicyCallBacks.EvtCxDevicePreWakeFromS0Triggered = EvtCxPreWakeFromS0Triggered;
    cxPowerPolicyCallBacks.EvtCxDevicePostDisarmWakeFromS0 = EvtCxPostDisarmWakeFromS0;
    cxPowerPolicyCallBacks.EvtCxDevicePreWakeFromSxTriggered = EvtCxPreWakeFromSxTriggered;
    cxPowerPolicyCallBacks.EvtCxDevicePostDisarmWakeFromSx = EvtCxPostDisarmWakeFromSx;

    //WDF documentation says some system might drop wake signals causing PreWakeFromS0/Sx
    //callback to not be invoked. To handle that case, we also register PreDisarmWakeFromS0/Sx
    //to perform the same operation.
    cxPowerPolicyCallBacks.EvtCxDevicePreDisarmWakeFromS0 = EvtCxPreWakeFromS0Triggered;
    cxPowerPolicyCallBacks.EvtCxDevicePreDisarmWakeFromSx = EvtCxPreWakeFromSxTriggered;

    WdfCxDeviceInitSetPowerPolicyEventCallbacks(CxDeviceInit, &cxPowerPolicyCallBacks);
}

_Use_decl_annotations_
void
NxDevice::StoreResetDiagnostics(
    size_t ResetDiagnosticsSize,
    const UINT8 * ResetDiagnosticsBuffer
)
/*++

Routine description:

    Store the reset diagnostics provided by the IHV driver.
    Permit IHV providing empty reset diagnostics.

--*/
{
    //
    // Prevent further calls of NetDeviceStoreResetDiagnostics
    //
    m_collectResetDiagnosticsThread = nullptr;

    if (ResetDiagnosticsSize == 0U)
    {
        return;
    }

    //
    // Store truncated diagnostics when ResetDiagnosticsSize
    // exceeds the prev-advertised maximum size
    //
    auto const diagnosticsSize = min(
        ResetDiagnosticsSize,
        static_cast<size_t>(MAX_RESET_DIAGNOSTICS_SIZE));

    //
    // If preallocation succeeded, store the reset diagnostics provided by the IHV driver
    //
    if (m_resetDiagnosticsBuffer != WDF_NO_HANDLE)
    {
        // Continue to PLDR even if this memcopy fails
        (void) WdfMemoryCopyFromBuffer(
            m_resetDiagnosticsBuffer,
            0U,
            const_cast<UINT8 *>(ResetDiagnosticsBuffer),
            diagnosticsSize);

        //
        // Store reset diagnostic info in NetAdapterCxTriageBlock
        //
        void * buffer = WdfMemoryGetBuffer(m_resetDiagnosticsBuffer, nullptr);
        m_netAdapterCxTriageBlock->ResetDiagnosticsStartAddr = reinterpret_cast<ULONG64>(buffer);
        m_netAdapterCxTriageBlock->ResetDiagnosticsSize = diagnosticsSize;
    }
}

_Use_decl_annotations_
NTSTATUS
NxDevice::ReportResetDiagnostics(
    HANDLE ReportHandle,
    PDBGK_LIVEDUMP_ADDSECONDARYDATA_ROUTINE DbgkLiveDumpAddSecondaryDataRoutine
)
/*++

Routine description:

    Use the DbgkLiveDumpAddSecondaryDataRoutine provided by Dbgk library to add diagnostics to live kernel dump.

--*/
{
    CX_RETURN_NTSTATUS_IF(STATUS_SUCCESS, m_resetDiagnosticsBuffer == WDF_NO_HANDLE);

    size_t resetDiagnosticsSize = 0U;
    void * resetDiagnosticsBuffer = WdfMemoryGetBuffer(m_resetDiagnosticsBuffer, &resetDiagnosticsSize);

    CX_RETURN_NTSTATUS_IF(STATUS_SUCCESS, resetDiagnosticsSize == 0U);

    CX_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !DbgkLiveDumpAddSecondaryDataRoutine);

    return DbgkLiveDumpAddSecondaryDataRoutine(
        ReportHandle,
        &m_resetCapabilities.ResetDiagnosticsGuid,
        resetDiagnosticsBuffer,
        static_cast<ULONG>(resetDiagnosticsSize));
}

static
NTSTATUS
LiveKernelDumpCallback(
    _In_ HANDLE ReportHandle,
    _In_ PDBGK_LIVEDUMP_ADDSECONDARYDATA_ROUTINE DbgkLiveDumpAddSecondaryDataRoutine,
    _In_opt_ ULONG BugCheckCode,
    _In_opt_ ULONG_PTR P1,
    _In_opt_ ULONG_PTR P2,
    _In_opt_ ULONG_PTR P3,
    _In_opt_ ULONG_PTR P4,
    _In_ void * CallbackContext
)
/*++

Routine description:

    Used by the live kernel dump library to acquire diagnostics.
    Expecting CallbackContext to be a pointer to a NxDevice.

--*/
{
    UNREFERENCED_PARAMETER(BugCheckCode);
    UNREFERENCED_PARAMETER(P1);
    UNREFERENCED_PARAMETER(P2);
    UNREFERENCED_PARAMETER(P3);
    UNREFERENCED_PARAMETER(P4);

    auto const nxDevice = reinterpret_cast<NxDevice *>(CallbackContext);

    return nxDevice->ReportResetDiagnostics(
        ReportHandle,
        DbgkLiveDumpAddSecondaryDataRoutine);
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::CollectDiagnostics(
    void
)
/*++

Routine description:

    1) (optionally) collect diagnostics from the driver, then
    2) report them with a live kernel dump.

--*/
{
    //
    // Request full live dump, capture user-mode process pages.
    //
    DBGK_LIVEDUMP_FLAGS const livedumpFlags = {1, 0, 0, 0};

    m_collectResetDiagnosticsThread = KeGetCurrentThread();

    WdfObjectOptionalCallback callback{ GetFxObject(), m_resetCapabilities.EvtNetDeviceCollectResetDiagnostics };
    callback.Invoke();

    m_collectResetDiagnosticsThread = nullptr;

    auto const deviceObj = WdfDeviceWdmGetDeviceObject(m_device);
    auto const driverObj = WdfDriverWdmGetDriverObject(
        reinterpret_cast<WDFDRIVER>(
            WdfObjectContextGetObject(reinterpret_cast<void *>(m_driver))));

    CX_LOG_IF_NOT_NT_SUCCESS_MSG(
        DbgkWerCaptureLiveKernelDump(
            L"NetAdapterCx",
            BUGCODE_NETADAPTER_DRIVER,
            NETADAPTERCX_BUGCHECK_DEVICE_RESET,
            reinterpret_cast<ULONG_PTR>(deviceObj),
            reinterpret_cast<ULONG_PTR>(driverObj),
            reinterpret_cast<ULONG_PTR>(nullptr),
            reinterpret_cast<void *>(this),
            &LiveKernelDumpCallback,
            livedumpFlags),
        "Capturing live kernel dump failed.");

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
void
NxDevice::TriggerPlatformLevelDeviceReset(
    void
)
/*++

Routine description:

    Entry function for TriggeringPldr state.
    Invoke m_resetInterface's DeviceReset interface to complete PLDR.
    Cleanup references taken in PlatformLevelDeviceResetWithDiagnostics.

--*/
{
    NTSTATUS status = STATUS_DEVICE_FEATURE_NOT_SUPPORTED;

    SetFailingDeviceRequestingResetFlag();

    if (DeviceResetTypeSupported(PlatformLevelDeviceReset))
    {
        ++m_resetAttempts;

        LogInfo(FLAG_DEVICE, "Performing Platform-Level Device Reset on WDFDEVICE=%p", GetFxObject());

        // Dispatch the specified device reset request to ACPI.
        status = m_resetInterface.DeviceReset(m_resetInterface.Context, PlatformLevelDeviceReset, 0, NULL);

        if (!NT_SUCCESS(status))
        {
            LogWarning(FLAG_DEVICE, "Platform-Level Device Reset Failed %!STATUS!. WDFDEVICE=%p", status, GetFxObject());
        }
        else
        {
            LogInfo(FLAG_DEVICE, "Platform-Level Device Reset Complete Successfully on WDFDEVICE=%p", GetFxObject());
        }
    }

    if (!NT_SUCCESS(status))
    {
        // If device doesn't support PLDR or PLDR failed,
        // use WdfDeviceSetFailed and attempt to restart the device.
        // When the driver is reloaded, it will reinitialize the device.
        // If several consecutive restart attempts fail (because the restarted driver again reports an error),
        // the framework stops trying to restart the device.
        LogInfo(FLAG_DEVICE,
            "Performing WdfDeviceSetFailed with WdfDeviceFailedAttemptRestart WDFDEVICE=%p", GetFxObject());

        WdfDeviceSetFailed(GetFxObject(), WdfDeviceFailedAttemptRestart);

        LogInfo(FLAG_DEVICE,
            "WdfDeviceSetFailed Complete WDFDEVICE=%p", GetFxObject());
    }

    m_pldrComplete.Set();

    if (m_resetDiagnosticsBuffer != WDF_NO_HANDLE)
    {
        WdfObjectDereferenceWithTag(m_resetDiagnosticsBuffer, PLDR_TAG);
    }

    WdfObjectDereferenceWithTag(GetFxObject(), PLDR_TAG);
}

_Use_decl_annotations_
void
NxDevice::PlatformLevelDeviceResetWithDiagnostics(
    void
)
/*++

Routine description:
    Generate RequestPldr event in state machine,
    who will take care of collecting diagnostics and triggering PLDR.

--*/
{
    if (!m_pldrRequested)
    {
        m_pldrRequested = true;

        if (m_resetDiagnosticsBuffer != WDF_NO_HANDLE)
        {
            WdfObjectReferenceWithTag(m_resetDiagnosticsBuffer, PLDR_TAG);
        }

        WdfObjectReferenceWithTag(GetFxObject(), PLDR_TAG);
        EnqueueEvent(NxDevice::Event::RequestPlatformLevelDeviceReset);
    }
}

_Use_decl_annotations_
void
NxDevice::WaitDeviceResetFinishIfRequested(
    void
)
{
    if (m_pldrRequested)
    {
        m_pldrComplete.Wait();
    }
}

_Use_decl_annotations_
NTSTATUS
NxDevice::_Create(
    NX_PRIVATE_GLOBALS * PrivateGlobals,
    WDFDEVICE_INIT * DeviceInit
)
/*++
Routine Description:
    Static method that creates the NXDEVICE object, corresponding to the
    WDFDEVICE.

    The NxDevice object is created either when the first NxAdapter object
    is created or in PrePrepareHardware.

--*/
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxDevice);

    attributes.EvtCleanupCallback = EvtDeviceCleanup;
    attributes.EvtDestroyCallback = [](WDFOBJECT Object)
    {
        // Note: Do not assume 'Object' is a valid WDFDEVICE handle. In case of failure
        // we might get a half initialized device object. The only thing that is guaranteed
        // to succeed is getting the typed context
        auto nxDevice = static_cast<NxDevice *>(WdfObjectGetTypedContext(
            Object,
            NxDevice));

        nxDevice->~NxDevice();
    };

    void * storage;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfCxDeviceInitAllocateContext(
            DeviceInit,
            &attributes,
            &storage));

    new (storage) NxDevice(PrivateGlobals);

    return STATUS_SUCCESS;
}

WDFDEVICE
NxDevice::GetFxObject(
    void
) const
{
    return m_device;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::EnsureInitialized(
    WDFDEVICE Device
)
{
    if (m_device == nullptr)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(Initialize(Device));
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::Initialize(
    WDFDEVICE Device
)
{
    m_device = Device;

    //
    // Querying device bus address is best effort
    //
    WDF_DEVICE_PROPERTY_DATA data;
    WDF_DEVICE_PROPERTY_DATA_INIT(&data, &DEVPKEY_Device_Address);

    DEVPROPTYPE devPropType;
    ULONG requiredSize;

    WdfDeviceQueryPropertyEx(
        m_device,
        &data,
        sizeof(m_deviceBusAddress),
        &m_deviceBusAddress,
        &requiredSize,
        &devPropType);

    WDF_DEVICE_PROPERTY_DATA_INIT(&data, &DEVPKEY_Device_InstanceId);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = m_device;

    WDFMEMORY memory;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfDeviceAllocAndQueryPropertyEx(
            m_device,
            &data,
            NonPagedPoolNx,
            &attributes,
            &memory,
            &devPropType));

    size_t bufferSize;
    m_pnpInstanceId = reinterpret_cast<wchar_t * const>(WdfMemoryGetBuffer(
        memory,
        &bufferSize));

    // Store the size in bytes not counting the NULL terminator
    m_cbPnpInstanceId = bufferSize - sizeof(UNICODE_NULL);

    //
    // TODO:: When IRP_MN_QUERY_INTERFACE is hijacked and not return as PendingReturned, the completion event will never be signaled.
    // Add it back once WDF has a solution for it. We have to remove it now or some devices will crash always
    //
    QueryDeviceResetInterface();

    //
    // Preallocate buffer for reset diagnostics. Continue device initialization if preallocation
    // fails; but if PLDR happens later, reset diagnostics won't be collected.
    //
    if (DeviceResetTypeSupported(PlatformLevelDeviceReset) &&
        m_resetCapabilities.EvtNetDeviceCollectResetDiagnostics != nullptr)
    {
        WDF_OBJECT_ATTRIBUTES objectAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = GetFxObject();

        CX_LOG_IF_NOT_NT_SUCCESS_MSG(
            WdfMemoryCreate(
                &objectAttributes,
                NonPagedPoolNx,
                NETADAPTERCX_TAG,
                MAX_RESET_DIAGNOSTICS_SIZE,
                &m_resetDiagnosticsBuffer,
                nullptr),
            "Preallocate reset diagnostics buffer failed"
        );
    }

    m_pldrComplete.Clear();

    #ifdef _KERNEL_MODE
    StateMachineEngineConfig smConfig(WdfDeviceWdmGetDeviceObject(GetFxObject()), NETADAPTERCX_TAG);
    #else
    StateMachineEngineConfig smConfig;
    #endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxDeviceStateMachine::Initialize(smConfig),
        "StateMachineEngine_Init failed");

    auto driverContext = GetCxDriverContextFromHandle(WdfGetDriver());
    driverContext->GetDeviceCollection().Add(this);
    CX_RETURN_IF_NOT_NT_SUCCESS(PnpPower.Initialize(this));

    // Default supported OID List
    m_oidListCount = ARRAYSIZE(ndisHandledWdfOids);

    auto allocationSize = m_oidListCount * sizeof(NDIS_OID);
    m_oidList = MakeSizedPoolPtrNP<NDIS_OID>(NETADAPTERCX_TAG, allocationSize);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_oidList);

    RtlCopyMemory(m_oidList.get(), &ndisHandledWdfOids[0], allocationSize);

    return STATUS_SUCCESS;
}

NxDriver *
NxDevice::GetNxDriver(
    void
) const
{
    return m_driver;
}

_Use_decl_annotations_
NxDevice::NxDevice(
    NX_PRIVATE_GLOBALS * NxPrivateGlobals
)
    : m_driver(NxPrivateGlobals->NxDriver)
    , PnpPower(m_adapterCollection)
    , IdleStateMachine(&m_device)
{
    m_netAdapterCxTriageBlock = &g_NetAdapterCxTriageBlock;
}

UINT32
NxDevice::GetDeviceBusAddress(
    void
) const
{
    return m_deviceBusAddress;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePrePrepareHardware(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesRaw,
    WDFCMRESLIST ResourcesTranslated
)
{
    UNREFERENCED_PARAMETER((ResourcesRaw, ResourcesTranslated));
    auto nxDevice = GetNxDeviceFromHandle(Device);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        nxDevice->EnsureInitialized(Device),
        "Failed to initialize NxDevice. WDFDEVICE=%p", Device);

    nxDevice->PnpPower.Start();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
EvtCxDevicePrePrepareHardwareFailedCleanup(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesRaw,
    WDFCMRESLIST ResourcesTranslated
)
{
    UNREFERENCED_PARAMETER((ResourcesRaw, ResourcesTranslated));
    auto nxDevice = GetNxDeviceFromHandle(Device);

    nxDevice->PnpPower.Stop();
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePreReleaseHardware(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesTranslated
)
/*++

Routine Description:

    Called by WDF before calling the client's EvtDeviceReleaseHardware.

    The state machine will manage posting the surprise remove or remove
    notification to NDIS.
--*/
{
    auto nxDevice = GetNxDeviceFromHandle(Device);

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    nxDevice->PnpPower.Stop();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePostD0EntryPostHardwareEnabled(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousTargetState
)
{
    NT_FRE_ASSERT(PreviousTargetState >= WdfPowerDeviceD1);
    NT_FRE_ASSERT(PreviousTargetState <= WdfPowerDeviceD3Final);

    auto nxDevice = GetNxDeviceFromHandle(Device);

    nxDevice->EnqueueEvent(NxDevice::Event::D0EntryPostHardwareEnabled);

    nxDevice->IdleStateMachine.HardwareEnabled();
    nxDevice->PnpPower.ChangePowerState(WdfPowerDeviceD0);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePreD0ExitPreHardwareDisabled(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);

    NT_FRE_ASSERT(TargetState == WdfPowerDeviceD1 ||
        TargetState == WdfPowerDeviceD2 ||
        TargetState == WdfPowerDeviceD3 ||
        TargetState == WdfPowerDeviceD3Final);

    nxDevice->PnpPower.ChangePowerState(TargetState);
    nxDevice->IdleStateMachine.HardwareDisabled(TargetState == WdfPowerDeviceD3Final);

    if (TargetState == WdfPowerDeviceD3Final)
    {
        nxDevice->EnqueueEvent(NxDevice::Event::D0ExitPreHardwareDisabledD3Final);
    }
    else
    {
        nxDevice->EnqueueEvent(NxDevice::Event::D0ExitPreHardwareDisabledDx);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID
EvtCxPreWakeFromS0Triggered(
    WDFDEVICE Device
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->EnableWakeReasonReporting();
}

_Use_decl_annotations_
VOID
EvtCxPostDisarmWakeFromS0(
    WDFDEVICE Device
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->DisableWakeReasonReporting();
}

_Use_decl_annotations_
VOID
EvtCxPreWakeFromSxTriggered(
    WDFDEVICE Device
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->EnableWakeReasonReporting();
}

_Use_decl_annotations_
VOID
EvtCxPostDisarmWakeFromSx(
    WDFDEVICE Device
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->DisableWakeReasonReporting();
}

void
NxDevice::EnableWakeReasonReporting(
    void
)
{
    m_isWakeInProgress = true;
}

void
NxDevice::DisableWakeReasonReporting(
    void
)
{
    m_isWakeInProgress = false;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePreD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);

    if (PreviousState != WdfPowerDeviceD3Final)
    {
        nxDevice->BeginPowerTransition();
    }

    nxDevice->IdleStateMachine.D0Entry();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePostD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
)
{
    return GetNxDeviceFromHandle(Device)->PostD0Entry(PreviousState);
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePostSelfManagedIoInit(
    WDFDEVICE Device
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->IdleStateMachine.SelfManagedIoStart();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePostSelfManagedIoRestartEx(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);

    if (PreviousState != WdfPowerDeviceD3Final)
    {
        nxDevice->WritePowerStateChangeEvent(WdfPowerDeviceD0);
        nxDevice->EndPowerTransition();
    }

    nxDevice->IdleStateMachine.SelfManagedIoStart();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePreSelfManagedIoSuspendEx(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);

    if (TargetState != WdfPowerDeviceD3Final)
    {
        nxDevice->BeginPowerTransition();
        nxDevice->UpdatePowerPolicyParameters();
    }

    nxDevice->IdleStateMachine.SelfManagedIoSuspend();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePostD0Exit(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);

    if (TargetState != WdfPowerDeviceD3Final)
    {
        nxDevice->WritePowerStateChangeEvent(TargetState);
        nxDevice->EndPowerTransition();
    }

    nxDevice->IdleStateMachine.D0Exit();

    return STATUS_SUCCESS;
}

void
NxDevice::BeginPowerTransition(
    void
)
{
    m_flags.InPowerTransition = 1;
}

void
NxDevice::EndPowerTransition(
    void
)
{
    m_flags.InPowerTransition = 0;
}

bool
NxDevice::IsDeviceInPowerTransition(
    void
)
{
    return !!m_flags.InPowerTransition;
}

bool
NxDevice::IsWakeInProgress(
    void
)
{
    return m_isWakeInProgress;
}

_Use_decl_annotations_
void
EvtCxDevicePreSurpriseRemoval(
    WDFDEVICE Device
)
{
    GetNxDeviceFromHandle(Device)->PnpPower.SurpriseRemoved();
}

static
UNICODE_STRING
GetIrpFileName(
    _In_ IO_STACK_LOCATION const &StackLocation
)
{
    // Make an unicode string without the initial backslash
    UNICODE_STRING refString = {};
    refString.Buffer = &StackLocation.FileObject->FileName.Buffer[1];
    refString.Length = StackLocation.FileObject->FileName.Length - sizeof(OBJ_NAME_PATH_SEPARATOR);
    refString.MaximumLength = StackLocation.FileObject->FileName.MaximumLength - sizeof(OBJ_NAME_PATH_SEPARATOR);

    NT_ASSERT(
        refString.Length % sizeof(WCHAR) == 0 &&
        refString.MaximumLength % sizeof(WCHAR) == 0);

    return refString;
}

static
DeviceInterfaceType
DeviceInterfaceTypeFromFileName(
    _In_ NxAdapterCollection * AdapterCollection,
    _In_ IO_STACK_LOCATION const &StackLocation,
    _Out_opt_ NxAdapter ** Adapter
)
/*++

Routine Description:

    This is called by WdmCreateIrpPreProcess to evaluate what kind
    of device interface is being used to open a handle to this device.
    We use a reference string to determine which device interface
    was used in the open call. The reference string 'NetAdapterCx'
    is reserved for system use, as well as the network interface
    GUIDs of the adapters created on top of this device. All other
    requests will be sent to the client.

    If this handle is being opened on the NetAdapterCx device
    interface or a private device interface, we mark the FsContext
    so that in future calls for the same handle we know where to
    dispatch the IRP.

    If a valid Adapter is returned, it must be dereferenced after
    returning from this function.

Arguments:

    AdapterCollection - Pointer to the NxAdapterCollection object
    StackLocation - Pointer to the current I/O stack location
    Adapter - [Optional] Pointer to the adapter referenced

Return Value:

    The device interface type.

--*/
{
    NT_ASSERT(StackLocation.MajorFunction == IRP_MJ_CREATE);

    // If there is no FileObject, the handle is being opened
    // on a private device interface
    if (StackLocation.FileObject == nullptr)
    {
        return DeviceInterfaceType::ClientDriver;
    }

    // If the FileName is empty this is a private device interface
    if (StackLocation.FileObject->FileName.Length == 0)
    {
        return DeviceInterfaceType::ClientDriver;
    }

    if (StackLocation.FileObject->FileName.Length > sizeof(WCHAR))
    {
        // Make an unicode string without the initial backslash
        UNICODE_STRING refString = GetIrpFileName(StackLocation);

        // Check if the reference string is the network interface GUID
        // of any of the adapters
        *Adapter = AdapterCollection->FindAndReferenceAdapterByBaseName(&refString);

        if (*Adapter != nullptr)
        {
            return DeviceInterfaceType::Ndis;
        }
    }

    return DeviceInterfaceType::ClientDriver;
}

static
DeviceInterfaceType
DeviceInterfaceTypeFromFsContext(
    _In_ IO_STACK_LOCATION const & StackLocation
)
/*++

Routine description:

    After a handle is opened we can check what is the device interface
    type by looking at the FsContext field of the FileObject, if one
    exists.

Arguments:

    StackLocation - Pointer to the current I/O stack location

Return Value:

    The device interface type.

--*/
{
    if (StackLocation.FileObject == nullptr)
    {
        return DeviceInterfaceType::ClientDriver;
    }

    if (StackLocation.FileObject->FsContext == CLIENT_DRIVER_DEVICE_INTERFACE_SIGNATURE)
    {
        return DeviceInterfaceType::ClientDriver;
    }

    return DeviceInterfaceType::Ndis;
}

NTSTATUS
NxDevice::WdmCreateIrpPreProcess(
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
)
/*++
Routine Description:

    This is a pre-processor routine for IRP_MJ_CREATE.

    We use this routine to determine what kind of device interface is being
    used to open a handle to this device and dispatch the IRP accordingly.
    The handle context is initialized here.
--*/
{
    auto device = GetFxObject();
    auto irpStack = IoGetCurrentIrpStackLocation(Irp);

    // If this handle is being opened on a private device interface or
    // on the NetAdapterCx device interface, mark the FsContext so that in
    // future calls for the same handle we know where to dispatch the IRP.

    NxAdapter * adapter = nullptr;
    auto deviceInterfaceType = DeviceInterfaceTypeFromFileName(&m_adapterCollection,
        *irpStack,
        &adapter);

    if (deviceInterfaceType == DeviceInterfaceType::ClientDriver)
    {
        if ( irpStack->FileObject != nullptr)
        {
            irpStack->FileObject->FsContext = CLIENT_DRIVER_DEVICE_INTERFACE_SIGNATURE;
        }

        IoSkipCurrentIrpStackLocation(Irp);
        return WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);
    }

    NT_ASSERT(deviceInterfaceType == DeviceInterfaceType::Ndis);

    NTSTATUS status = NdisWdfCreateIrpHandler(adapter->GetNdisHandle(), Irp);
    NdisWdfMiniportDereference(adapter->GetNdisHandle());

    return status;
}

NTSTATUS
NxDevice::WdmCloseIrpPreProcess(
    _Inout_ IRP * Irp,
    _In_ WDFCONTEXT DispatchContext
)
/*++
Routine Description:

    This is a pre-processor routine for IRP_MJ_CLOSE

    If the device interface is not NetAdapterCx or private, the IRP
    is sent to NDIS so it can perform cleanup associated with the
    handle being closed.

Notes:
    NdisWdfCloseIrpHandler will free the context associated with
    the handle. So we check if the handle is on a private interface
    before calling that function.
--*/
{
    auto device = GetFxObject();
    auto irpStack = IoGetCurrentIrpStackLocation(Irp);

    auto deviceInterfaceType = DeviceInterfaceTypeFromFsContext(*irpStack);

    if (deviceInterfaceType == DeviceInterfaceType::ClientDriver)
    {
        IoSkipCurrentIrpStackLocation(Irp);
        return WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);
    }

    NT_ASSERT(deviceInterfaceType == DeviceInterfaceType::Ndis);

    return NdisWdfCloseIrpHandler(Irp);
}

NTSTATUS
NxDevice::WdmIoIrpPreProcess(
    _Inout_ IRP * Irp,
    _In_ WDFCONTEXT DispatchContext
)
{
    auto device = GetFxObject();
    auto irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_ASSERT(
        irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ||
        irpStack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL ||
        irpStack->MajorFunction == IRP_MJ_WRITE ||
        irpStack->MajorFunction == IRP_MJ_READ ||
        irpStack->MajorFunction == IRP_MJ_CLEANUP);

    auto deviceInterfaceType = DeviceInterfaceTypeFromFsContext(*irpStack);

    if(deviceInterfaceType == DeviceInterfaceType::ClientDriver)
    {
        IoSkipCurrentIrpStackLocation(Irp);
        return WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);
    }

    NT_ASSERT(deviceInterfaceType == DeviceInterfaceType::Ndis);

    NTSTATUS status;
    switch (irpStack->MajorFunction)
    {
    case IRP_MJ_DEVICE_CONTROL:
        status = NdisWdfDeviceControlIrpHandler(Irp);
        break;
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
        status = NdisWdfDeviceInternalControlIrpHandler(Irp);
        break;
    case IRP_MJ_CLEANUP:
        // NDIS always completes IRP_MJ_CLEANUP with STATUS_SUCCESS
        status = STATUS_SUCCESS;

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        break;
    default:
        // NDIS does not support IRP_MJ_READ and IRP_MJ_WRITE
        status = STATUS_NOT_SUPPORTED;

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        break;
    }

    return status;
}

NTSTATUS
NxDevice::WdmSystemControlIrpPreProcess(
    _Inout_ IRP * Irp,
    _In_ WDFCONTEXT DispatchContext
)
{
    LogVerbose(FLAG_DEVICE,
        "Preprocessing IRP_MJ_SYSTEM_CONTROL. WDFDEVICE=%p, IRP %p\n", GetFxObject(), Irp);

    auto device = GetFxObject();

    auto const location = IoGetCurrentIrpStackLocation(Irp);
    switch (location->MinorFunction)
    {
    case IRP_MN_QUERY_SINGLE_INSTANCE:
    case IRP_MN_CHANGE_SINGLE_INSTANCE:
    {
        auto const & wmi = location->Parameters.WMI;
        auto const & guid = static_cast<WNODE_SINGLE_INSTANCE const *>(wmi.Buffer)->WnodeHeader.Guid;

        if (RtlEqualMemory(&guid, &GUID_POWER_DEVICE_ENABLE, sizeof(guid)) ||
            RtlEqualMemory(&guid, &GUID_POWER_DEVICE_WAKE_ENABLE, sizeof(guid)))
        {
            IoSkipCurrentIrpStackLocation(Irp);
            return WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);
        }

    }
        break;

    }

    return WmiIrpDispatch(Irp, DispatchContext);
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiIrpDispatch(
    IRP * Irp,
    WDFCONTEXT DispatchContext
)
{
    PIO_STACK_LOCATION pirpSp = IoGetCurrentIrpStackLocation(Irp);
    void * dataPath = pirpSp->Parameters.WMI.DataPath;
    ULONG bufferSize = pirpSp->Parameters.WMI.BufferSize;
    void * buffer = pirpSp->Parameters.WMI.Buffer;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG returnSize = 0;

    WDFDEVICE device = GetFxObject();
    PDEVICE_OBJECT deviceObject = WdfDeviceWdmGetDeviceObject(device);

    if (pirpSp->Parameters.WMI.ProviderId != (ULONG_PTR)deviceObject)
    {
        // Return IRP to wdm
        IoSkipCurrentIrpStackLocation(Irp);
        return WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);
    }

    // Call on common mapping
    switch (pirpSp->MinorFunction)
    {
        case IRP_MN_REGINFO:

            status = WmiRegister((ULONG_PTR)dataPath,
                reinterpret_cast<PWMIREGINFO>(buffer),
                bufferSize,
                false,
                &returnSize);

            break;

        case IRP_MN_REGINFO_EX:

            status = WmiRegister((ULONG_PTR)dataPath,
                reinterpret_cast<PWMIREGINFO>(buffer),
                bufferSize,
                true,
                &returnSize);

            break;

        case IRP_MN_QUERY_ALL_DATA:

            status = WmiQueryAllData((LPGUID)dataPath,
                reinterpret_cast<WNODE_ALL_DATA*>(buffer),
                bufferSize,
                &returnSize);

            break;

        case IRP_MN_QUERY_SINGLE_INSTANCE:
        case IRP_MN_CHANGE_SINGLE_INSTANCE:

            status = WmiProcessSingleInstance(
                reinterpret_cast<WNODE_SINGLE_INSTANCE*>(buffer),
                bufferSize,
                pirpSp->MinorFunction,
                &returnSize);

            break;

        case IRP_MN_ENABLE_EVENTS:

            status = WmiEnableEvents(*((LPGUID)dataPath));
            break;

        case IRP_MN_DISABLE_EVENTS:

            status = WmiDisableEvents(*((LPGUID)dataPath));
            break;

        case IRP_MN_EXECUTE_METHOD:

            status = WmiExecuteMethod(
                reinterpret_cast<WNODE_METHOD_ITEM*>(buffer),
                bufferSize,
                &returnSize);

            break;

        case IRP_MN_CHANGE_SINGLE_ITEM:
        case IRP_MN_ENABLE_COLLECTION:
        case IRP_MN_DISABLE_COLLECTION:
            status = STATUS_NOT_SUPPORTED;
            break;

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    ASSERT(status != STATUS_PENDING);

    Irp->IoStatus.Status = status;
    ASSERT(returnSize <= bufferSize);

    if (status == STATUS_BUFFER_TOO_SMALL)
    {
        Irp->IoStatus.Information = returnSize;
    }
    else
    {
        Irp->IoStatus.Information = NT_SUCCESS(status) ? returnSize : 0;
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

static
NTSTATUS
WmiRegInfoWriteString(
    _Inout_ WMIREGINFO * WmiRegInfo,
    _In_ size_t Offset,
    _In_ wchar_t const * SourceString,
    _In_ size_t cbSourceString
)
{
    //
    // Write the string size followed by the string starting at 'Offset' from the WMIREGINFO pointer
    //

    CX_RETURN_NTSTATUS_IF(
        STATUS_INTEGER_OVERFLOW,
        cbSourceString > USHORT_MAX);

    auto pSize = reinterpret_cast<USHORT *>(
        reinterpret_cast<BYTE *>(WmiRegInfo) + Offset);

    *pSize = static_cast<USHORT>(cbSourceString);

    auto pString = pSize + 1;

    RtlCopyMemory(
        pString,
        SourceString,
        cbSourceString);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiRegister(
    ULONG_PTR RegistrationType,
    WMIREGINFO * WmiRegInfo,
    ULONG WmiRegInfoSize,
    bool ShouldReferenceDriver,
    ULONG * ReturnSize
)
/*++

Routine Description:
    This function handles IRP_MN_REGINFO and IRP_MN_REGINFO_EX for NetAdapterCx.
    The supported GUID to OID list for the device is created here and stored
    in m_pGuidToOidMap along with the count of GUIDs supported, m_cGuidToOidMap.

Arguments:
    RegistrationType - Indicates whether this is a new registration or an update
    WmiRegInfo - Buffer for WMIREGINFO
    WmiRegInfoSize - Size of wmiRegInfo
    ReturnSize - Size of return data

--*/
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    *ReturnSize = 0;

    if (WMIREGISTER == RegistrationType)
    {
        //
        //  Get the supported list of OIDs
        //
        if (m_pGuidToOidMap == NULL)
        {
            NdisWdfGetGuidToOidMap(m_oidList.get(),
                (USHORT)m_oidListCount,
                NULL,
                &m_cGuidToOidMap);

            auto allocationSize = m_cGuidToOidMap * sizeof(NDIS_GUID);

            m_pGuidToOidMap = MakeSizedPoolPtrNP<NDIS_GUID>(NX_WMI_TAG, allocationSize);

            CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_pGuidToOidMap);

            RtlZeroMemory(m_pGuidToOidMap.get(), allocationSize);

            NdisWdfGetGuidToOidMap(m_oidList.get(),
                (USHORT)m_oidListCount,
                m_pGuidToOidMap.get(),
                &m_cGuidToOidMap);
        }

        auto registryPath = GetCxDriverContextFromHandle(WdfGetDriver())->GetRegistryPath();
        auto const cbRegistryPath = registryPath->Length;

        //
        //  Determine how much memory we need to allocate.
        //

        wchar_t const netAdapterMofResourceName[] = L"NetAdapterMof";
        ULONG const cbNetAdapterMofResourceName = sizeof(netAdapterMofResourceName) - sizeof(UNICODE_NULL);

        ULONG const cNetAdapterGuids = 1;

        // Size of WMIREGINFO struct with the necessary size to hold the WMIREGGUID array at the end
        ULONG const netAdapterRegistrationSize = sizeof(WMIREGINFO) + cNetAdapterGuids * sizeof(WMIREGGUID);

        // Size of WMIREGINFO + the necessary size to hold the MOF resource name and registry path
        ULONG netAdapterSizeNeeded =
            netAdapterRegistrationSize +
            sizeof(USHORT) + cbNetAdapterMofResourceName +
            sizeof(USHORT); /* + cbRegistryPath */

        CX_RETURN_IF_NOT_NT_SUCCESS(
            RtlULongAdd(
                netAdapterSizeNeeded,
                cbRegistryPath,
                &netAdapterSizeNeeded));

        // Size needed to ensure the next WMIREGINFO is properly aligned
        ULONG const netAdapterAlignedSizeNeeded = ALIGN_UP(netAdapterSizeNeeded, void *);

        ULONG const cNdisGuids = m_cGuidToOidMap;
        ULONG ndisSizeNeeded; /* = sizeof(WMIREGINFO) + cNdisGuids * sizeof(WMIREGGUID) */
        ULONG cbNdisRegGuids;

        CX_RETURN_IF_NOT_NT_SUCCESS(
            RtlULongMult(
                cNdisGuids,
                sizeof(WMIREGGUID),
                &cbNdisRegGuids));

        CX_RETURN_IF_NOT_NT_SUCCESS(
            RtlULongAdd(
                sizeof(WMIREGINFO),
                cbNdisRegGuids,
                &ndisSizeNeeded));

        ULONG totalSizeNeeded; /* = netAdapterAlignedSizeNeeded + ndisSizeNeeded */
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RtlULongAdd(
                netAdapterAlignedSizeNeeded,
                ndisSizeNeeded,
                &totalSizeNeeded));

        //
        //  We need to give this above information back to WMI.
        //
        if (WmiRegInfoSize < totalSizeNeeded)
        {
            ASSERT(WmiRegInfoSize >= 4);

            *((ULONG *)WmiRegInfo) = (totalSizeNeeded);
            *ReturnSize = sizeof(ULONG);
            return STATUS_BUFFER_TOO_SMALL;
        }

        *ReturnSize = totalSizeNeeded;

        RtlZeroMemory(WmiRegInfo, totalSizeNeeded);

        //
        //  Initialize the NetAdapterCx WMI registration info
        //
        PWMIREGINFO pwri = WmiRegInfo;

        pwri->BufferSize = netAdapterSizeNeeded;
        pwri->NextWmiRegInfo = netAdapterAlignedSizeNeeded;
        pwri->GuidCount = cNetAdapterGuids;

        pwri->WmiRegGuid[0].Guid = GUID_NETCX_DEVICE_POWER_STATE_CHANGE;
        pwri->WmiRegGuid[0].Flags = WMIREG_FLAG_EVENT_ONLY_GUID;

        // Write the MOF resource name right after the WMIREGINFO structure
        pwri->MofResourceName = netAdapterRegistrationSize;

        CX_RETURN_IF_NOT_NT_SUCCESS(
            WmiRegInfoWriteString(
                pwri,
                pwri->MofResourceName,
                &netAdapterMofResourceName[0],
                cbNetAdapterMofResourceName));

        // Write the registry path right after the MOF resource name
        pwri->RegistryPath = sizeof(USHORT) + cbNetAdapterMofResourceName; /* + pwri->MofResourceName */

        CX_RETURN_IF_NOT_NT_SUCCESS(
            RtlULongAdd(
                pwri->RegistryPath,
                pwri->MofResourceName,
                &pwri->RegistryPath));

        CX_RETURN_IF_NOT_NT_SUCCESS(
            WmiRegInfoWriteString(
                pwri,
                pwri->RegistryPath,
                &registryPath->Buffer[0],
                cbRegistryPath));

        //
        // Initialize the NDIS WMI registration info
        //
        pwri = reinterpret_cast<WMIREGINFO *>(
            reinterpret_cast<BYTE *>(WmiRegInfo) + pwri->NextWmiRegInfo);

        pwri->BufferSize = ndisSizeNeeded;
        pwri->NextWmiRegInfo = 0;
        pwri->GuidCount = cNdisGuids;

        //
        //  Go through the GUIDs NDIS supports
        //

        PNDIS_GUID pndisguid;
        PWMIREGGUID pwrg;
        size_t c;
        for (c = 0, pndisguid = m_pGuidToOidMap.get(), pwrg = pwri->WmiRegGuid;
            (c < cNdisGuids);
            c++, pndisguid++, pwrg++)
        {
            if (pndisguid->Guid == GUID_POWER_DEVICE_ENABLE ||
                pndisguid->Guid == GUID_POWER_DEVICE_WAKE_ENABLE ||
                pndisguid->Guid == GUID_NDIS_WAKE_ON_MAGIC_PACKET_ONLY)
            {
                auto device = static_cast<WDFDEVICE>(GetFxObject());
                auto pdo = WdfDeviceWdmGetPhysicalDevice(device);
                pwrg->InstanceInfo = (ULONG_PTR)pdo;
                pwrg->Flags = WMIREG_FLAG_INSTANCE_PDO;
                pwrg->InstanceCount = 1;

                if (ShouldReferenceDriver)
                {
                    ObReferenceObject(pdo);
                }
            }
            pwrg->Guid = pndisguid->Guid;
        }

        pwri->RegistryPath = 0;
        pwri->MofResourceName = 0;

        status = STATUS_SUCCESS;
    }

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiQueryAllData(
    GUID * Guid,
    WNODE_ALL_DATA * Wnode,
    ULONG BufferSize,
    ULONG * ReturnSize
)
/*++

Routine Description:
    This function handles IRP_MN_QUERY_ALL_DATA for NetAdapterCx. It queries the data for all
    the adapters in the device and creates a single WNODE_ALL_DATA reply. A copy of the original
    WNODE_ALL_DATA is sent to all the adapters and the replies are aggregated into Wnode.

Arguments:
    Guid - GUID that is being queried
    Wnode - WNODE_ALL_DATA buffer which gets populated in this function
    BufferSize - Size of Wnode
    ReturnSize - Size of the WNODE_ALL_DATA being returned

--*/
{
    CX_RETURN_NTSTATUS_IF(STATUS_BUFFER_TOO_SMALL, BufferSize < sizeof(WNODE_TOO_SMALL));

    NTSTATUS status;
    PNDIS_GUID pNdisGuid;
    USHORT miniportCount = 0;

    status = WmiGetGuid(Wnode->WnodeHeader.Guid, &pNdisGuid);
    if (!NT_SUCCESS(status))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Copy of the wnode. This will be sent to all the adapters to query them.
    KPoolPtrNP<WNODE_ALL_DATA> wnodeCopy = MakeSizedPoolPtrNP<WNODE_ALL_DATA>(NX_WMI_TAG,
        BufferSize);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !wnodeCopy);

    RtlZeroMemory(wnodeCopy.get(), BufferSize);

    RtlCopyMemory(wnodeCopy.get(),
        Wnode,
        sizeof(WNODE_ALL_DATA));

    // Temporary buffer to send/receive query data to adapters
    KPoolPtrNP<WNODE_ALL_DATA> dataBuffer = MakeSizedPoolPtrNP<WNODE_ALL_DATA>(NX_WMI_TAG,
        BufferSize);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !dataBuffer);

    m_adapterCollection.ForEach(
        [this, &status, &ReturnSize, &pNdisGuid, &Guid, BufferSize, &Wnode, &miniportCount, &dataBuffer, &wnodeCopy](NxAdapter & adapter)
    {
        RtlZeroMemory(dataBuffer.get(), BufferSize);

        //
        // For the following conditions, we do not need to send the IRP to rest
        // of the miniports:
        //    1. WMI IRP is not supported/invalid
        //    2. WMI BufferSize is too small
        //    3. Request is for GUID_NDIS_WAKE_ON_MAGIC_PACKET_ONLY
        //
        if (!NT_SUCCESS(status) || WI_IsFlagSet(Wnode->WnodeHeader.Flags, WNODE_FLAG_TOO_SMALL) || (pNdisGuid->Guid == GUID_NDIS_WAKE_ON_MAGIC_PACKET_ONLY && miniportCount > 0))
            return;

        NDIS_HANDLE miniport = adapter.GetNdisHandle();
        RtlCopyMemory(dataBuffer.get(),
            wnodeCopy.get(),
            sizeof(WNODE_ALL_DATA));

        status = NdisWdfQueryAllData(miniport,
            pNdisGuid,
            Guid,
            dataBuffer.get(),
            BufferSize,
            ReturnSize);

        if (!NT_SUCCESS(status))
            return;

        if (WI_IsFlagSet(((PWNODE_ALL_DATA)(dataBuffer.get()))->WnodeHeader.Flags, WNODE_FLAG_TOO_SMALL))
        {
            RtlCopyMemory(Wnode,
                dataBuffer.get(),
                sizeof(WNODE_ALL_DATA));
            return;
        }

        status = WmiWnodeAllDataMerge((PWNODE_ALL_DATA)Wnode,
            (PWNODE_ALL_DATA)(dataBuffer.get()),
            BufferSize,
            miniportCount,
            ReturnSize);

        miniportCount++;
    });

    return status;
}

inline
NTSTATUS
SetWmiBufferTooSmall(
    _In_ ULONG BufferSize,
    _In_ void * Wnode,
    _In_ ULONG WnodeSize,
    _Out_ PULONG PReturnSize
    )
/*++
Routine Description:
    This method checks the buffer size of the WNODE to check if it is smaller than
    WNODE_TOO_SMALL and returns the status accordingly. Otherwise, it sets the
    WNODE_FLAG_TOO_SMALL flag on the WNODE header and updates the size needed in
    the WNODE.
--*/
{
    if (BufferSize < sizeof(WNODE_TOO_SMALL))
    {
        *PReturnSize = sizeof(ULONG);
        return STATUS_BUFFER_TOO_SMALL;
    }
    else
    {
        ((PWNODE_TOO_SMALL)Wnode)->WnodeHeader.BufferSize = sizeof(WNODE_TOO_SMALL);
        ((PWNODE_TOO_SMALL)Wnode)->WnodeHeader.Flags |= WNODE_FLAG_TOO_SMALL;
        ((PWNODE_TOO_SMALL)Wnode)->SizeNeeded = WnodeSize;
        *PReturnSize= sizeof(WNODE_TOO_SMALL);
        return STATUS_SUCCESS;
    }
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiWnodeAllDataMerge(
    WNODE_ALL_DATA * AllData,
    WNODE_ALL_DATA * SingleData,
    ULONG BufferSize,
    USHORT MiniportCount,
    ULONG * ReturnSize
)
/*++

Routine Description:
    This routine combines two WNODE_ALL_DATA structures - AllData and SingleData. Information from
    SingleData is added to AllData. Refer to the structure of WNODE_ALL_DATA. SingleData's
    WNODE_ALL_DATA information is updated in AllData. A buffer is used to store AllData's instance
    name array when the SingleData's data block is added to AllData. Then, the AllData's instance
    name array is copied to the end of the data blocks. A buffer is used to store the instance name
    strings. The instance name offsets are updated. SingleData's instance name offset is added.
    The instance name strings are copied back into AllData. Then SingleData's instance name is
    added at the end of AllData.

    WNODE_ALL_DATA structure:

    +--------------+-----------------+------------------+---------------------------+-------------------+
    | WndodeHeader | DataBlockOffset | InstanceCount(n) | OffsetInstanceNameOffsets | FixedInstanceSize |
    +---------------------------------------------------+---------------------------+-------------------+

    +--------------+----------------+------------+----------------+
    | Data Block 1 |  Data Block 2  |  . . . . . |  Data Block n  |
    +--------------+----------------+------------+----------------+
    |                       |
    DataBlockOffset    FixedInstanceSize

    +----------+- - - -+-----------+-------------------+-----------------+- - - -+-----------------+
    | Offset 1 | . . . |  Offset n |  Instance Name 1  | Instance Name 2 | . . . | Instance Name n |
    +----------+- - - -+-----------+-------------------+-----------------+- - - -+-----------------+
    |                              |                                             |
    OffsetInstanceNameOffset    Offset 1                                      Offset n

Arguments:
    AllData - The WNODE_ALL_DATA structure that SingleData will be aggregated into
    SingleData - The WNODE_ALL_DATA structure that will be aggregated into AllData
    BufferSize - Size of AllDAta
    ReturnSize - Return size of AllData after the aggregation
    MiniportCount - The total count of Miniports in AllData

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    const ULONG roundedFixedInstanceSize = ((SingleData->FixedInstanceSize + 7) & ~7);
    const ULONG instanceNameSize = SingleData->WnodeHeader.BufferSize - SingleData->OffsetInstanceNameOffsets;
    const ULONG dataToBeAddedSize = roundedFixedInstanceSize + instanceNameSize;
    const ULONG allDataBufferSize = AllData->WnodeHeader.BufferSize;
    const ULONG totalBufferSizeReqd = SingleData->WnodeHeader.BufferSize + dataToBeAddedSize * (m_adapterCollection.Count() - 1);

    if (BufferSize < totalBufferSizeReqd)
    {
        Status = SetWmiBufferTooSmall(BufferSize,
            (void *)AllData,
            totalBufferSizeReqd,
            ReturnSize);
        return Status;
    }

    if (MiniportCount == 0)
    {
        // This is the first instance in the WNODE
        RtlCopyMemory(AllData,
            SingleData,
            SingleData->WnodeHeader.BufferSize);
    }
    else
    {
        const ULONG tempInstanceNameSize = AllData->WnodeHeader.BufferSize - AllData->OffsetInstanceNameOffsets;

        KPoolPtrNP<PUCHAR> tempInstanceNameBuffer = MakeSizedPoolPtrNP<PUCHAR>(NX_WMI_TAG,
            tempInstanceNameSize);

        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !tempInstanceNameBuffer);

        RtlZeroMemory(tempInstanceNameBuffer.get(), tempInstanceNameSize);

        // Store the Instance Names - This has the offset to the InstanceName, then a USHORT specifying the UNICODE string
        // length and then the string itself.
        RtlCopyMemory(tempInstanceNameBuffer.get(),
            (PUCHAR)AllData + AllData->OffsetInstanceNameOffsets,
            tempInstanceNameSize);

        // Copy the WNODE_ALL_DATA
        RtlCopyMemory(AllData,
            SingleData,
            sizeof(WNODE_ALL_DATA));

        // Copy the new Data Block. Remaining data blocks will remain intact (after WNODE_ALL_DATA)
        RtlCopyMemory((PUCHAR)AllData + AllData->DataBlockOffset + roundedFixedInstanceSize *MiniportCount,
            (PUCHAR)SingleData + SingleData->DataBlockOffset,
            roundedFixedInstanceSize);

        AllData->InstanceCount = MiniportCount + 1;

        // Copy the Existing Instance Names offset array
        RtlCopyMemory((PUCHAR)AllData + AllData->DataBlockOffset + roundedFixedInstanceSize * AllData->InstanceCount,
            tempInstanceNameBuffer.get(),
            tempInstanceNameSize);

        RtlZeroMemory(tempInstanceNameBuffer.get(), tempInstanceNameSize);

        RtlCopyMemory(tempInstanceNameBuffer.get(),
            (PUCHAR)AllData + AllData->DataBlockOffset + roundedFixedInstanceSize * AllData->InstanceCount + MiniportCount*sizeof(ULONG),
            tempInstanceNameSize - MiniportCount * sizeof(ULONG));

        AllData->OffsetInstanceNameOffsets = AllData->DataBlockOffset + roundedFixedInstanceSize * AllData->InstanceCount;

        //Update the instance name offsets array
        for (size_t i = 0; i < MiniportCount; i++)
        {
            *(ULONG *)((PUCHAR)AllData + AllData->OffsetInstanceNameOffsets + i * sizeof(ULONG)) += roundedFixedInstanceSize + sizeof(ULONG);
        }

        // Offset of the last Instance Name
        *(ULONG *)((PUCHAR)AllData + AllData->DataBlockOffset + roundedFixedInstanceSize * AllData->InstanceCount + MiniportCount * sizeof(ULONG)) =
            AllData->OffsetInstanceNameOffsets
            + sizeof(ULONG)
            + tempInstanceNameSize;

        // Copy the Existing Instance Names
        RtlCopyMemory((PUCHAR)AllData + AllData->DataBlockOffset + roundedFixedInstanceSize * AllData->InstanceCount + AllData->InstanceCount * sizeof(ULONG),
            tempInstanceNameBuffer.get(),
            tempInstanceNameSize - MiniportCount * sizeof(ULONG));

        // Copy the new Instance Name
        RtlCopyMemory((PUCHAR)AllData + AllData->OffsetInstanceNameOffsets + sizeof(ULONG) + tempInstanceNameSize,
            (PUCHAR)SingleData + SingleData->OffsetInstanceNameOffsets + sizeof(ULONG),
            instanceNameSize);

        //Assert that singledata fixedinstancesize == alldata fixedinstancesize
        AllData->WnodeHeader.BufferSize = allDataBufferSize + dataToBeAddedSize;
        *ReturnSize = AllData->WnodeHeader.BufferSize;
    }

    return Status;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiProcessSingleInstance(
    WNODE_SINGLE_INSTANCE * Wnode,
    ULONG BufferSize,
    UCHAR MinorFunction,
    ULONG * ReturnSize
)
/*++

Routine Description:
    This function handles IRP_MN_QUERY_SINGLE_INSTANCE and IRP_MN_CHANGE_SINGLE_INSTANCE for
    NetAdapterCx. It searches the adapters in the device for the matching instance name and
    sends the query request to the corresponding adapter.

Arguments:
    Wnode - WNODE_SINGLE_INSTANCE buffer which gets populated in this function
    BufferSize - Size of Wnode
    MinorFunction - Indicates whether its a Query or a Change IRP
    ReturnSize - Size of the WNODE_SINGLE_INSTANCE being returned

--*/
{
    NDIS_GUID * guid;

    CX_RETURN_IF_NOT_NT_SUCCESS(WmiGetGuid(Wnode->WnodeHeader.Guid, &guid));

    if (guid->Guid == GUID_NDIS_WAKE_ON_MAGIC_PACKET_ONLY)
    {
        return WmiProcessSingleInstanceMagicPacket(
            Wnode,
            guid,
            BufferSize,
            MinorFunction,
            ReturnSize);
    }

    return WmiProcessSingleInstanceDefault(
        Wnode,
        guid,
        BufferSize,
        MinorFunction,
        ReturnSize);
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiProcessSingleInstanceDefault(
    WNODE_SINGLE_INSTANCE * Wnode,
    NDIS_GUID * Guid,
    ULONG BufferSize,
    UCHAR MinorFunction,
    ULONG * ReturnSize
)
{
    NTSTATUS status;

    UNICODE_STRING instanceName;
    instanceName.Buffer = (PWSTR)((PUCHAR)Wnode + Wnode->OffsetInstanceName + sizeof(USHORT));
    instanceName.Length = *(PUSHORT)((PUCHAR)Wnode + Wnode->OffsetInstanceName);
    instanceName.MaximumLength = *(PUSHORT)((PUCHAR)Wnode + Wnode->OffsetInstanceName);

    auto miniport = m_adapterCollection.FindAndReferenceMiniportByInstanceName(&instanceName);

    CX_WARNING_RETURN_NTSTATUS_IF_MSG(STATUS_WMI_INSTANCE_NOT_FOUND,
        !miniport,
        "Adapter(%wZ) not found.",
        &instanceName);

    switch(MinorFunction)
    {
        case IRP_MN_QUERY_SINGLE_INSTANCE:
            status = NdisWdfQuerySingleInstance(miniport.get(), Guid, Wnode, BufferSize, ReturnSize);
            break;

        case IRP_MN_CHANGE_SINGLE_INSTANCE:
            status = NdisWdfChangeSingleInstance(miniport.get(), Guid, Wnode);
            break;

        default:
            status = STATUS_NOT_SUPPORTED;
    }

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiProcessSingleInstanceMagicPacket(
    WNODE_SINGLE_INSTANCE * Wnode,
    NDIS_GUID * Guid,
    ULONG BufferSize,
    UCHAR MinorFunction,
    ULONG * ReturnSize
)
{
    Rtl::KArray<unique_miniport_reference> targetAdapters;

    // For queries we pick the first miniport we can and invoke NDIS to run the logic
    // For change instance we need to send the request to every miniport we can reference
    m_adapterCollection.ForEach([&targetAdapters, MinorFunction](NxAdapter & adapter)
    {
        if (MinorFunction == IRP_MN_QUERY_SINGLE_INSTANCE && targetAdapters.count() > 0)
        {
            // For query we only need one miniport
            return;
        }

        auto miniport = adapter.GetMiniportReference();

        if (!miniport)
        {
            LogVerbose(FLAG_DEVICE,
                "Failed to reference miniport for NETADAPTER %p",
                adapter.GetFxObject());

            return;
        }

        if (!targetAdapters.append(wistd::move(miniport)))
        {
            LogWarning(FLAG_DEVICE,
                "Failed to store miniport reference in targetAdapters array");
        }
    });

    CX_WARNING_RETURN_NTSTATUS_IF_MSG(
        STATUS_WMI_INSTANCE_NOT_FOUND,
        targetAdapters.count() == 0,
        "No adapters were found to handle WMI request");

    NTSTATUS ntStatus = STATUS_SUCCESS;

    switch (MinorFunction)
    {
        case IRP_MN_QUERY_SINGLE_INSTANCE:
        {
            auto & miniport = targetAdapters[0];

            ntStatus = NdisWdfQuerySingleInstance(
                miniport.get(),
                Guid,
                Wnode,
                BufferSize,
                ReturnSize);

            break;
        }
        case IRP_MN_CHANGE_SINGLE_INSTANCE:
        {
            for (auto & miniport : targetAdapters)
            {
                auto tmpStatus = NdisWdfChangeSingleInstance(
                    miniport.get(),
                    Guid,
                    Wnode);

                // If the miniport does not support wake on magic packet ignore the error, otherwise record the
                // last failure to report to the caller
                if (tmpStatus == STATUS_INVALID_DEVICE_REQUEST)
                {
                    LogVerbose(FLAG_DEVICE,
                        "Miniport %p does not support wake on magic packet",
                        miniport.get());
                }
                else if (tmpStatus != STATUS_SUCCESS)
                {
                    ntStatus = tmpStatus;
                }
            }

            break;
        }
        default:
        {
            ntStatus = STATUS_NOT_SUPPORTED;
            break;
        }
    }

    return ntStatus;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiEnableEvents(
    GUID const & Guid
)
{
    if (Guid == GUID_NETCX_DEVICE_POWER_STATE_CHANGE)
    {
        m_flags.WmiPowerEventEnabled = 1;

        return STATUS_SUCCESS;
    }

    NDIS_GUID * ndisGuid = NULL;
    CX_RETURN_IF_NOT_NT_SUCCESS(WmiGetGuid(Guid, &ndisGuid));

    //  Is this GUID an event indication?
    CX_RETURN_NTSTATUS_IF(STATUS_INVALID_DEVICE_REQUEST,
        (ndisGuid->Flags & fNDIS_GUID_TO_STATUS) == 0);

    //  Mark the guid as enabled
    ndisGuid->Flags |= NX_GUID_EVENT_ENABLED;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiDisableEvents(
    GUID const & Guid
)
{
    if (Guid == GUID_NETCX_DEVICE_POWER_STATE_CHANGE)
    {
        m_flags.WmiPowerEventEnabled = 0;

        return STATUS_SUCCESS;
    }

    NDIS_GUID * ndisGuid = NULL;
    CX_RETURN_IF_NOT_NT_SUCCESS(WmiGetGuid(Guid, &ndisGuid));

    //  Is this GUID an event indication?
    CX_RETURN_NTSTATUS_IF(STATUS_INVALID_DEVICE_REQUEST,
        (ndisGuid->Flags & fNDIS_GUID_TO_STATUS) == 0);

    //  Mark the guid as disabled
    ndisGuid->Flags &= ~NX_GUID_EVENT_ENABLED;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiExecuteMethod(
    WNODE_METHOD_ITEM * Wnode,
    ULONG BufferSize,
    ULONG * ReturnSize
)
/*++

Routine Description:
    This function handles IRP_MN_QUERY_SINGLE_INSTANCE and IRP_MN_CHANGE_SINGLE_INSTANCE for
    NetAdapterCx. It searches the adapters in the device for the matching instance name and
    sends the query request to the corresponding adapter.

Arguments:
    Wnode - WNODE_SINGLE_INSTANCE buffer which gets populated in this function
    BufferSize - Size of Wnode
    Method - Indicates whether its a Query or a Change IRP
    ReturnSize - Size of the WNODE_SINGLE_INSTANCE being returned

--*/
{
    NDIS_GUID * guid;

    CX_RETURN_IF_NOT_NT_SUCCESS(WmiGetGuid(Wnode->WnodeHeader.Guid, &guid));

    UNICODE_STRING instanceName;
    instanceName.Buffer = (PWSTR)((PUCHAR)Wnode + Wnode->OffsetInstanceName + sizeof(USHORT));
    instanceName.Length = *(PUSHORT)((PUCHAR)Wnode + Wnode->OffsetInstanceName);
    instanceName.MaximumLength = *(PUSHORT)((PUCHAR)Wnode + Wnode->OffsetInstanceName);

    auto miniport = m_adapterCollection.FindAndReferenceMiniportByInstanceName(&instanceName);

    CX_WARNING_RETURN_NTSTATUS_IF_MSG(STATUS_WMI_INSTANCE_NOT_FOUND,
        !miniport,
        "Adapter(%wZ) not found.",
        &instanceName);

    NTSTATUS status = NdisWdfExecuteMethod(miniport.get(), guid, Wnode, BufferSize, ReturnSize);

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiGetGuid(
    GUID const & Guid,
    NDIS_GUID ** NdisGuid
)
/*++

Routine Description:
    Get the GUID to OID mapping for Guid.

Arguments:
    NdisGuid - Pointer to the GUID to OID mapping entry in m_pGuidToOidMap
    Guid - The GUID for which we are finding a mapping

--*/
{
    NTSTATUS status = STATUS_WMI_GUID_NOT_FOUND;

    *NdisGuid = NULL;

    //
    //  Search the GUID map
    //
    if (m_pGuidToOidMap)
    {
        auto pNdisGuid = m_pGuidToOidMap.get();
        for (UINT c = 0; c < m_cGuidToOidMap; c++, pNdisGuid++)
        {
            //
            //  We are to look for a guid to oid mapping.
            //
            if (pNdisGuid->Guid == Guid)
            {
                //
                //  We found the GUID, save the OID that we will need to
                //  send to the miniport.
                //
                status = STATUS_SUCCESS;
                *NdisGuid = pNdisGuid;

                break;
            }
        }
    }

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiGetEventGuid(
    NTSTATUS GuidStatus,
    NDIS_GUID ** Guid
) const
/*++

Routine Description:
    Get the GUID to OID mapping for the GuidStatus.

Arguments:
    PPNdisGuid - Pointer to the GUID to OID mapping entry in m_pGuidToOidMap
    NTSTATUS GuidStatus - GUID status indication

--*/
{
    *Guid = nullptr;

    //  Search the GUID map
    if (m_pGuidToOidMap)
    {
        auto pNdisGuid = m_pGuidToOidMap.get();
        for (size_t c = 0; c < m_cGuidToOidMap; c++, pNdisGuid++)
        {
            // Find the Guid with the matching GuidStatus
            if (((pNdisGuid->Flags & fNDIS_GUID_TO_STATUS) != 0) &&
                    (pNdisGuid->Status == GuidStatus))
            {
                *Guid = pNdisGuid;
                return STATUS_SUCCESS;
            }
        }
    }

    return STATUS_WMI_GUID_NOT_FOUND;
}

NTSTATUS
NxDevice::WdmPnPIrpPreProcess(
    _Inout_ IRP * Irp
)
/*++
Routine Description:

    This is a pre-processor routine for PNP IRP's

    In case this is an IRP_MN_QUERY_REMOVE_DEVICE if we are the device stack requesting
    for a reset, then we mark the status as DEVICE_HUNG so that PnP can skip orderly removal
    and send a Surprise Removal to the stack
--*/
{
    NTSTATUS                       status = STATUS_SUCCESS;
    PIO_STACK_LOCATION             irpStack;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->MinorFunction)
    {
    case IRP_MN_QUERY_REMOVE_DEVICE:
        if (m_failingDeviceRequestingReset)
        {
            // In the case we are the one who requested for reset/recovery, then mark the IRP as
            // STATUS_DEVICE_HUNG which is used as a hint to ACPI that this is the device failing and
            // not a veto attempt. This will ensure that a Surprise Removal will come to this device stack
            status = STATUS_DEVICE_HUNG;
            Irp->IoStatus.Status = status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
        }
        break;
    }

    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtWdmIrpPreprocessRoutine(
    WDFDEVICE Device,
    IRP * Irp,
    WDFCONTEXT DispatchContext
)
/*++
Routine Description:

    This is a pre-processor routine for several IRPs.

    From this routine NetAdapterCx forwards the following irps directly to NDIS.sys
        CREATE, CLOSE, DEVICE_CONTROL, INTERNAL_DEVICE_CONTROL, SYSTEM_CONTRL

--*/
{
    PIO_STACK_LOCATION             irpStack;
    NTSTATUS                       status = STATUS_SUCCESS;
    PDEVICE_OBJECT                 deviceObj;

    deviceObj = WdfDeviceWdmGetDeviceObject(Device);
    auto nxDevice = GetNxDeviceFromHandle(Device);

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch(irpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        LogVerbose(FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ_CREATE Device 0x%p, IRP 0x%p\n", deviceObj, Irp);

        status = nxDevice->WdmCreateIrpPreProcess(Irp, DispatchContext);
        break;
    case IRP_MJ_CLOSE:
        LogVerbose(FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ_CLOSE Device 0x%p, IRP 0x%p\n", deviceObj, Irp);

        status = nxDevice->WdmCloseIrpPreProcess(Irp, DispatchContext);
        break;
    case IRP_MJ_DEVICE_CONTROL:
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
    case IRP_MJ_WRITE:
    case IRP_MJ_READ:
    case IRP_MJ_CLEANUP:
        LogVerbose(FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ code 0x%x, Device 0x%p, IRP 0x%p\n", irpStack->MajorFunction, deviceObj, Irp);

        status = nxDevice->WdmIoIrpPreProcess(Irp, DispatchContext);
        break;
    default:
        NT_ASSERTMSG("Unexpected Irp", FALSE);
        status = STATUS_INVALID_PARAMETER;

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        break;
    }

    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtWdmPnpIrpPreprocessRoutine(
    WDFDEVICE Device,
    IRP * Irp,
    WDFCONTEXT DispatchContext
)
{
    auto irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_FRE_ASSERT(irpStack->MajorFunction == IRP_MJ_PNP);
    NT_FRE_ASSERT(irpStack->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE);

    auto nxDevice = GetNxDeviceFromHandle(Device);

    LogVerbose(FLAG_DEVICE,
        "EvtWdmPnpIrpPreprocessRoutine IRP_MJ_PNP. WDFDEVICE=%p, IRP=%p, irpStack->MinorFunction=0x%x",
        Device,
        Irp,
        irpStack->MinorFunction);

    NTSTATUS ntStatus = nxDevice->WdmPnPIrpPreProcess(Irp);

    if (ntStatus == STATUS_DEVICE_HUNG)
    {
        // special case this since the IRP has been already completed in WdmPnPIrpPreProcess
        // which is the only place where this error code is set, so just return here.
        return ntStatus;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return WdfDeviceWdmDispatchIrp(
        Device,
        Irp,
        DispatchContext);
}

_Use_decl_annotations_
NTSTATUS
EvtWdmWmiIrpPreprocessRoutine(
    WDFDEVICE Device,
    IRP * Irp,
    WDFCONTEXT DispatchContext
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    return nxDevice->WdmSystemControlIrpPreProcess(Irp, DispatchContext);
}

_Use_decl_annotations_
void
EvtDeviceCleanup(
    WDFOBJECT Device
)
/*++
Routine Description:
    A WDF Event Callback

    This routine is called when the client driver WDFDEVICE object is being
    deleted. This happens when WDF is processing the REMOVE irp for the client.

--*/
{
    auto nxDevice = GetNxDeviceFromHandle(static_cast<WDFDEVICE>(Device));
    nxDevice->WaitDeviceResetFinishIfRequested();

    auto driverContext = GetCxDriverContextFromHandle(WdfGetDriver());
    driverContext->GetDeviceCollection().Remove(nxDevice);
    nxDevice->PnpPower.Cleanup();

    // Only post an event if the state machine had a chance to initialize
    if (nxDevice->IsInitialized())
    {
        nxDevice->EnqueueEvent(NxDevice::Event::WdfDeviceObjectCleanup);
    }
}

_Use_decl_annotations_
void
NxDevice::EvtLogTransition(
    _In_ SmFx::TransitionType TransitionType,
    _In_ StateId SourceState,
    _In_ EventId ProcessedEvent,
    _In_ StateId TargetState
)
{
    TraceLoggingWrite(
        g_hNetAdapterCxEtwProvider,
        "NxDeviceStateTransition",
        TraceLoggingDescription("NxDevicePnPStateTransition"),
        TraceLoggingKeyword(NET_ADAPTER_CX_NXDEVICE_PNP_STATE_TRANSITION),
        TraceLoggingUInt32(GetDeviceBusAddress(), "DeviceBusAddress"),
        TraceLoggingHexInt32(static_cast<INT32>(SourceState), "DeviceStateTransitionFrom"),
        TraceLoggingHexInt32(static_cast<INT32>(ProcessedEvent), "DeviceStateTransitionEvent"),
        TraceLoggingHexInt32(static_cast<INT32>(TargetState), "DeviceStateTransitionTo"));

    LogInfo(FLAG_DEVICE, "WDFDEVICE: %p"
        " [%!SMFXTRANSITIONTYPE!]"
        " From: %!DEVICEPNPPOWERSTATE!,"
        " Event: %!DEVICEPNPPOWEREVENT!,"
        " To: %!DEVICEPNPPOWERSTATE!",
        GetFxObject(),
        static_cast<unsigned>(TransitionType),
        static_cast<unsigned>(SourceState),
        static_cast<unsigned>(ProcessedEvent),
        static_cast<unsigned>(TargetState));
}

_Use_decl_annotations_
void
NxDevice::EvtLogMachineException(
    _In_ SmFx::MachineException exception,
    _In_ EventId relevantEvent,
    _In_ StateId relevantState
)
{
    UNREFERENCED_PARAMETER((exception, relevantEvent, relevantState));

    if (!KdRefreshDebuggerNotPresent())
    {
        DbgBreakPoint();
    }
}

_Use_decl_annotations_
void
NxDevice::EvtMachineDestroyed(
    void
)
{
}

_Use_decl_annotations_
void
NxDevice::EvtLogEventEnqueue(
    _In_ EventId relevantEvent
)
{
    UNREFERENCED_PARAMETER(relevantEvent);
}

_Use_decl_annotations_
void
NxDevice::GetPowerOffloadList(
    NxPowerOffloadList * PowerList
)
{
    m_adapterCollection.ForEach(
        [this, PowerList](NxAdapter & Adapter)
        {
            Adapter.m_powerPolicy.UpdatePowerOffloadList(IsDeviceInPowerTransition(), PowerList);
        });
}

_Use_decl_annotations_
void
NxDevice::GetWakeSourceList(
    NxWakeSourceList * PowerList
)
{
    m_adapterCollection.ForEach(
        [this, PowerList](NxAdapter & Adapter)
        {
            Adapter.m_powerPolicy.UpdateWakeSourceList(IsDeviceInPowerTransition(), PowerList);
        });
}

_Use_decl_annotations_
void
NxDevice::SetFailingDeviceRequestingResetFlag(
    void
)
/*++

Routine description:

    This routine is used to set a flag to indicate that we are the ones who are
    requesting for a device reset. This flag will be taken into account in the
    IRP_MN_QUERY_REMOVE call to return STATUS_DEVICE_HUNG in case we requested
    for a Reset

--*/
{
    m_failingDeviceRequestingReset = true;
}

_Use_decl_annotations_
void
NxDevice::QueryDeviceResetInterface(
    void
)
/*++

Routine description:

    This routine is used to Query for the Device Reset Interface during Adapter Initialization.
    If the underying bus supports the interface then we will store the Reset Function Pointers
    so that it can be used to do FLDR or PLDR

--*/
{
    NTSTATUS status;

    RtlZeroMemory(&m_resetInterface, sizeof(m_resetInterface));
    m_resetInterface.Size = sizeof(m_resetInterface);
    m_resetInterface.Version = 1;

    status = WdfFdoQueryForInterface(
        GetFxObject(),
        &GUID_DEVICE_RESET_INTERFACE_STANDARD,
        (PINTERFACE)&m_resetInterface,
        sizeof(m_resetInterface),
        1,
        NULL);
    if (!NT_SUCCESS(status))
    {
        LogWarning(FLAG_DEVICE,
            "QueryDeviceResetInterface failed %!STATUS!. WDFDEVICE=%p", status, GetFxObject());
        RtlZeroMemory(&m_resetInterface, sizeof(m_resetInterface));
    }
}

_Use_decl_annotations_
bool
NxDevice::DeviceResetTypeSupported(
    DEVICE_RESET_TYPE ResetType
) const
/*++

Routine description:

    This routine checks whether a Reset Type requested is supported by the Underlying bus.

--*/
{
    if ((m_resetInterface.DeviceReset != NULL) &&
        ((m_resetInterface.SupportedResetTypes & (1 << ResetType)) != 0))
    {
        return TRUE;
    }

    return FALSE;
}

_Use_decl_annotations_
bool
NxDevice::GetSupportedDeviceResetType(
    _Inout_ ULONG * SupportedDeviceResetTypes
) const
/*++

Routine description:

    This routine returns the support Device Reset Types

--*/
{
    bool fDeviceResetSupported = FALSE;

    if (m_resetInterface.DeviceReset != NULL)
    {
        *SupportedDeviceResetTypes = m_resetInterface.SupportedResetTypes;
        fDeviceResetSupported = TRUE;
    }

    return fDeviceResetSupported;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::DispatchDeviceReset(
    DEVICE_RESET_TYPE ResetType
)
/*++

Routine description:

    This routine is used to perform a Device Reset operation. The requested ResetType is passed in
    as the parameter

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    FUNCTION_LEVEL_RESET_PARAMETERS ResetParameters;

    // If the client driver has registered for this callback, then it means that it wants
    // to handle the reset itself. This is useful in the case of USB where CyclePort can
    // internally trigger a Device Reset Operation
    if (m_evtNetDeviceReset != NULL)
    {
        m_evtNetDeviceReset(GetFxObject());
    }
    // This else block calls into the reset interface directly. Mostly applicable to PCIe based
    // devices.
    else if((m_resetInterface.DeviceReset != NULL) && DeviceResetTypeSupported(ResetType))
    {
        ++m_resetAttempts;  // Currently this is not used. Will use it later.

        LogInfo(FLAG_DEVICE, "Performing Device Reset");
        // Set the completion routine and context for function-level device resets.
        if (ResetType == FunctionLevelDeviceReset)
        {
            RtlZeroMemory(&ResetParameters, sizeof(FUNCTION_LEVEL_RESET_PARAMETERS));
            ResetParameters.CompletionParameters.Size = sizeof(FUNCTION_LEVEL_DEVICE_RESET_PARAMETERS);
            ResetParameters.CompletionParameters.DeviceResetCompletion = _FunctionLevelResetCompletion;
            ResetParameters.CompletionParameters.CompletionContext = &ResetParameters;
            KeInitializeEvent(&ResetParameters.Event, SynchronizationEvent, FALSE);
        }

        // Dispatch the specified device reset request to ACPI.
        status = m_resetInterface.DeviceReset(m_resetInterface.Context, ResetType, 0, NULL);

        // Wait for the function-level device reset to complete.
        if ((status == STATUS_PENDING) && (ResetType == FunctionLevelDeviceReset))
        {
            KeWaitForSingleObject(&ResetParameters.Event, Executive, KernelMode, FALSE, NULL);
            status = ResetParameters.Status;
        }
        if (!NT_SUCCESS(status))
        {
            LogWarning(FLAG_DEVICE,
                "Device Reset failed %!STATUS!. WDFDEVICE=%p", status, GetFxObject());

            // If Device Reset fails, perform WdfDeviceSetFailed.
            LogInfo(FLAG_DEVICE,
                "Performing WdfDeviceSetFailed with WdfDeviceFailedAttemptRestart WDFDEVICE=%p", GetFxObject());

            WdfDeviceSetFailed(GetFxObject(), WdfDeviceFailedAttemptRestart);

            LogInfo(FLAG_DEVICE,
                "WdfDeviceSetFailed Complete WDFDEVICE=%p", GetFxObject());
        }
    }
    else
    {
        // If the underlying device doesn't support the reset type we requested,
        // then use WdfDeviceSetFailed and attempt to restart the device.
        // When the driver is reloaded, it will reinitialize the device.
        // If several consecutive restart attempts fail (because the restarted driver again reports an error),
        // the framework stops trying to restart the device.
        LogInfo(FLAG_DEVICE,
            "Performing WdfDeviceSetFailed with WdfDeviceFailedAttemptRestart WDFDEVICE=%p", GetFxObject());

        WdfDeviceSetFailed(GetFxObject(), WdfDeviceFailedAttemptRestart);

        LogInfo(FLAG_DEVICE,
            "WdfDeviceSetFailed Complete WDFDEVICE=%p", GetFxObject());
    }

    return status;
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
    auto Parameters = reinterpret_cast<FUNCTION_LEVEL_RESET_PARAMETERS *>(Context);
    Parameters->Status = Status;
    KeSetEvent(&Parameters->Event, IO_NO_INCREMENT, FALSE);
    return;
}

_Use_decl_annotations_
void
NxDevice::SetEvtDeviceResetCallback(
    PFN_NET_DEVICE_RESET NetDeviceReset
)
/*++

Routine description:

    Used by the Client Drivers to set a Device Reset callback in which they will
    do the rundown of the objects. Currently this is used only by the Inbox MBB
    Class Driver

--*/
{
    m_evtNetDeviceReset = NetDeviceReset;
}

_Use_decl_annotations_
void
NxDevice::SetResetCapabilities(
    NET_DEVICE_RESET_CAPABILITIES const * ResetCapabilities
)
{
    RtlCopyMemory(
        &m_resetCapabilities,
        ResetCapabilities,
        ResetCapabilities->Size
    );
}

_Use_decl_annotations_
void
NxDevice::SetPowerPolicyEventCallbacks(
    NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS const * Callbacks
)
{
    RtlCopyMemory(
        &m_powerPolicyEventCallbacks,
        Callbacks,
        Callbacks->Size);
}

void
NxDevice::GetTriageInfo(
    void
)
{
    g_NetAdapterCxTriageBlock.StateMachineEngineOffset = 0;
    g_NetAdapterCxTriageBlock.NxDeviceAdapterCollectionOffset = FIELD_OFFSET(NxDevice, m_adapterCollection);

    NxAdapter::GetTriageInfo();
    NxAdapterCollection::GetTriageInfo();
}

_Use_decl_annotations_
NTSTATUS
NxDevice::AssignSupportedOidList(
    NDIS_OID const * SupportedOids,
    size_t SupportedOidsCount
)
{
    auto allocationSize = SupportedOidsCount * sizeof(NDIS_OID);
    m_oidList = MakeSizedPoolPtrNP<NDIS_OID>(NETADAPTERCX_TAG, allocationSize);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_oidList);

    RtlCopyMemory(m_oidList.get(), SupportedOids, allocationSize);

    m_oidListCount = SupportedOidsCount;
    return STATUS_SUCCESS;
}

NDIS_OID const *
NxDevice::GetOidList(
    void
) const
{
    return m_oidList.get();
}

size_t
NxDevice::GetOidListCount(
    void
) const
{
    return m_oidListCount;
}

NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS const &
NxDevice::GetPowerPolicyEventCallbacks(
    void
) const
{
    return m_powerPolicyEventCallbacks;
}

_Use_decl_annotations_
bool
NxDevice::IsCollectingResetDiagnostics(
    void
) const
{
    return m_collectResetDiagnosticsThread == KeGetCurrentThread();
}

_Use_decl_annotations_
void
NxDevice::UpdatePowerPolicyParameters(
    void
)
{
    auto const powerAction = WdfDeviceGetSystemPowerAction(GetFxObject());

    m_adapterCollection.ForEach([powerAction](NxAdapter & Adapter)
    {
        if (powerAction == PowerActionNone)
        {
            auto const ntStatus = NdisWdfPnpPowerEventHandler(
                Adapter.GetNdisHandle(),
                NdisWdfActionPowerDx,
                NdisWdfActionPowerNone);

            NT_FRE_ASSERT(ntStatus == STATUS_SUCCESS);
        }
        else if (powerAction == PowerActionSleep || powerAction == PowerActionHibernate)
        {
            auto const ntStatus = NdisWdfPnpPowerEventHandler(
                Adapter.GetNdisHandle(),
                NdisWdfActionPowerDxOnSystemSx,
                NdisWdfActionPowerNone);

            NT_FRE_ASSERT(ntStatus == STATUS_SUCCESS);
        }
    });
}

_Use_decl_annotations_
void
NxDevice::WritePowerStateChangeEvent(
    WDF_POWER_DEVICE_STATE TargetState
) const
{
    if (!m_flags.WmiPowerEventEnabled)
    {
        return;
    }

    size_t const wnodeSize = ALIGN_UP_BY(sizeof(WNODE_SINGLE_INSTANCE), 8);
    size_t const wnodeInstanceNameSize = ALIGN_UP_BY(m_cbPnpInstanceId + sizeof(USHORT), 8);
    size_t const allocationSize = wnodeSize + wnodeInstanceNameSize + sizeof(NETCX_DEVICE_POWER_STATE_CHANGE);

    auto wnode = MakeSizedPoolPtr<WNODE_SINGLE_INSTANCE>('imWN', allocationSize);

    if (!wnode)
    {
        LogWarning(FLAG_DEVICE, "Failed to allocate memory for WMI power change event");
        return;
    }

    wnode->WnodeHeader.BufferSize = static_cast<ULONG>(allocationSize);
    wnode->WnodeHeader.ProviderId = IoWMIDeviceObjectToProviderId(WdfDeviceWdmGetDeviceObject(m_device));
    wnode->WnodeHeader.Version = 1;
    wnode->WnodeHeader.Guid = GUID_NETCX_DEVICE_POWER_STATE_CHANGE;
    wnode->WnodeHeader.Flags = WNODE_FLAG_EVENT_ITEM | WNODE_FLAG_SINGLE_INSTANCE;
    wnode->OffsetInstanceName = static_cast<ULONG>(wnodeSize);
    wnode->DataBlockOffset = wnode->OffsetInstanceName + static_cast<ULONG>(wnodeInstanceNameSize);
    wnode->SizeDataBlock = sizeof(NETCX_DEVICE_POWER_STATE_CHANGE);
    KeQuerySystemTime(&wnode->WnodeHeader.TimeStamp);

    auto ptr = reinterpret_cast<BYTE *>(wnode.get()) + wnodeSize;
    auto cbInstanceName = reinterpret_cast<USHORT *>(ptr);
    auto instanceName = cbInstanceName + 1;

    *cbInstanceName = static_cast<USHORT>(m_cbPnpInstanceId);

    RtlCopyMemory(
        instanceName,
        m_pnpInstanceId,
        m_cbPnpInstanceId);

    auto data = reinterpret_cast<NETCX_DEVICE_POWER_STATE_CHANGE *>(reinterpret_cast<BYTE *>(wnode.get()) + wnode->DataBlockOffset);

    switch (TargetState)
    {
        case WdfPowerDeviceD0:
            data->TargetState = PowerDeviceD0;
            break;

        case WdfPowerDeviceD1:
            data->TargetState = PowerDeviceD1;
            break;

        case WdfPowerDeviceD2:
            data->TargetState = PowerDeviceD2;
            break;

        case WdfPowerDeviceD3:
            data->TargetState = PowerDeviceD3;
            break;
    }

    data->PowerAction = WdfDeviceGetSystemPowerAction(m_device);

    auto ntStatus = IoWMIWriteEvent(wnode.get());

    CX_LOG_IF_NOT_NT_SUCCESS_MSG(ntStatus, "Failed to send power transition WMI event");

    if (ntStatus == STATUS_SUCCESS)
    {
        // Memory is now owned by the WMI subsystem
        wnode.release();
    }
}

_Use_decl_annotations_
NxAdapterCollection &
NxDevice::GetAdapterCollection(
    void
)
{
    return m_adapterCollection;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::PostD0Entry(
    WDF_POWER_DEVICE_STATE PreviousState
)
{
    if (PreviousState == WdfPowerDeviceD3Final)
    {
        // WDF StopIdle references can only be taken after the framework has called the driver's EvtDeviceD0Entry
        // for the first time. Tell each adapter, since they may need to act on this.
        LogInfo(FLAG_DEVICE, "Initial D0 Entry Reached on WDFDEVICE=%p", GetFxObject());
        m_adapterCollection.ForEach([](NxAdapter& adapter)
        {
            adapter.m_powerPolicy.OnWdfStopIdleRefsSupported();
        });
    }

    return STATUS_SUCCESS;
}
