// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include "NxDevice.tmh"
#include "NxDevice.hpp"

#include "NxAdapter.hpp"
#include "NxDriver.hpp"
#include "powerpolicy/NxPowerList.hpp"
#include "NxMacros.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include <ndisguid.h>

#include "netadaptercx_triage.h"

static EVT_WDF_DEVICE_PREPARE_HARDWARE EvtCxDevicePrePrepareHardware;
static EVT_WDFCX_DEVICE_PRE_PREPARE_HARDWARE_FAILED_CLEANUP EvtCxDevicePrePrepareHardwareFailedCleanup;
static EVT_WDF_DEVICE_RELEASE_HARDWARE EvtCxDevicePreReleaseHardware;
static EVT_WDF_DEVICE_RELEASE_HARDWARE EvtCxDevicePostReleaseHardware;
static EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT EvtCxDevicePostSelfManagedIoInit;
static EVT_WDF_DEVICE_SELF_MANAGED_IO_RESTART EvtCxDevicePostSelfManagedIoRestart;
static EVT_WDF_DEVICE_SELF_MANAGED_IO_SUSPEND EvtCxDevicePreSelfManagedIoSuspend;
static EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP EvtCxDevicePostSelfManagedIoCleanup;
static EVT_WDF_DEVICE_SURPRISE_REMOVAL EvtCxDevicePreSurpriseRemoval;
static EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtDeviceCleanup;

static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmIrpPreprocessRoutine;
static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmPnpIrpPreprocessRoutine;
static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmPowerIrpPreprocessRoutine;
static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmWmiIrpPreprocessRoutine;

static IO_COMPLETION_ROUTINE WdmIrpCompleteFromNdis;
static IO_COMPLETION_ROUTINE WdmIrpCompleteSetPower;

static DRIVER_NOTIFICATION_CALLBACK_ROUTINE DeviceInterfaceChangeNotification;

// Note: 0x5 and 0x3 were chosen as the device interface signatures because
// ExAllocatePool will allocate memory aligned to at least 8-byte boundary
static void * const NETADAPTERCX_DEVICE_INTERFACE_SIGNATURE = (void *)(0x3);
static void * const CLIENT_DRIVER_DEVICE_INTERFACE_SIGNATURE = (void *)(0x5);

// {0B425340-6A9D-45CB-81D6-DF9A5D78E9DE}
DEFINE_GUID(GUID_DEVINTERFACE_NETCX,
    0xb425340, 0x6a9d, 0x45cb, 0x81, 0xd6, 0xdf, 0x9a, 0x5d, 0x78, 0xe9, 0xde);

DECLARE_CONST_UNICODE_STRING(DeviceInterface_RefString, L"NetAdapterCx");

NETADAPTERCX_GLOBAL_TRIAGE_BLOCK g_NetAdapterCxTriageBlock =
{
    /* Signature                       */ NETADAPTERCX_TRIAGE_SIGNATURE,
    /* Version                         */ NETADAPTERCX_TRIAGE_VERSION_1,
    /* Size                            */ sizeof(NETADAPTERCX_GLOBAL_TRIAGE_BLOCK),

    /* StateMachineEngineOffset        */ 0,

    /* NxDeviceAdapterCollectionOffset */ 0,

    /* NxAdapterCollectionCountOffset  */ 0,

    /* NxAdapterLinkageOffset          */ 0
};

NDIS_OID const ndisHandledWdfOids[] =
{
    OID_GEN_HARDWARE_STATUS,
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
NTSTATUS
WdfCxDeviceInitAssignPreprocessorRoutines(
    WDFCXDEVICE_INIT *CxDeviceInit
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
        IRP_MN_REMOVE_DEVICE,
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

    UCHAR powerMinorCodes[] =
    {
        IRP_MN_SET_POWER
    };

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
            CxDeviceInit,
            EvtWdmPowerIrpPreprocessRoutine,
            IRP_MJ_POWER,
            powerMinorCodes,
            ARRAYSIZE(powerMinorCodes)),
        "WdfDeviceInitAssignWdmIrpPreprocessCallback failed for IRP_MJ_POWER");

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
    pnpCallbacks.EvtCxDevicePostReleaseHardware = EvtCxDevicePostReleaseHardware;

    //
    // EvtDeviceSurpriseRemoval:
    //
    pnpCallbacks.EvtCxDevicePreSurpriseRemoval = EvtCxDevicePreSurpriseRemoval;

    //
    // EvtDeviceSelfManagedIoInit:
    //
    pnpCallbacks.EvtCxDevicePostSelfManagedIoInit = EvtCxDevicePostSelfManagedIoInit;

    //
    // EvtDeviceSelfManagedIoRestart:
    //
    pnpCallbacks.EvtCxDevicePostSelfManagedIoRestart = EvtCxDevicePostSelfManagedIoRestart;

    //
    // EvtDeviceSelfManagedIoSuspend:
    //
    pnpCallbacks.EvtCxDevicePreSelfManagedIoSuspend = EvtCxDevicePreSelfManagedIoSuspend;

    //
    // EvtDeviceSelfManagedIoCleanup
    //
    pnpCallbacks.EvtCxDevicePostSelfManagedIoCleanup = EvtCxDevicePostSelfManagedIoCleanup;

    WdfCxDeviceInitSetPnpPowerEventCallbacks(CxDeviceInit, &pnpCallbacks);
}

void
NxDevice::AdapterCreated(
    _In_ NxAdapter *Adapter
)
/*++
Routine Description:
    Invoked by adapter object to inform the device about adapter creation
    As a result NxDevice adds the adapter to its collection.
--*/
{
    m_adapterCollection.Add(Adapter);
}

_Use_decl_annotations_
void
NxDevice::AdapterDestroyed(
    NxAdapter *Adapter
)
{
    if (m_adapterCollection.Remove(Adapter))
    {
        LogVerbose(
            GetRecorderLog(),
            FLAG_DEVICE,
            "Adapter removed from device collection. WDFDEVICE=%p, NETADAPTER=%p",
            GetFxObject(),
            Adapter->GetFxObject());
    }
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::AreAllAdaptersHalted(
    void
)
{
    if(InterlockedCompareExchange((LONG *)&m_ndisInitializeCount, 0, 0) == 0)
    {
        return NxDevice::Event::Yes;
    }
    else
    {
        return NxDevice::Event::No;
    }
}

void
NxDevice::AdapterInitialized(
    void
)
{
    InterlockedIncrement((LONG *)&m_ndisInitializeCount);
}

void
NxDevice::AdapterHalted(
    void
)
{
    InterlockedDecrement((LONG *)&m_ndisInitializeCount);

    KIRQL irql;

    //
    // Raise the IRQL to dispatch level so the device state machine doesnt
    // process removal on the current thread and deadlock.
    //
    KeRaiseIrql(DISPATCH_LEVEL, &irql);
    EnqueueEvent(NxDevice::Event::AdapterHalted);
    KeLowerIrql(irql);
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingIsSurpriseRemoved()
{
    m_state = DeviceState::ReleasingPhase2Pending;

    if (m_flags.SurpriseRemoved)
    {
        return NxDevice::Event::Yes;
    } else
    {
        return NxDevice::Event::No;
    }
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingReportSurpriseRemoveToNdis()
{
    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.ReportSurpriseRemove();
    });

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingReportDeviceAddFailureToNdis()
{
    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.FullStop(m_state);
    });

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingReportPreReleaseToNdis()
{
    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.StopPhase1();
    });

    CxPreReleaseHardwareHandled.Set();

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingReportPostReleaseToNdis()
{
    m_state = DeviceState::Released;

    m_adapterCollection.ForEach(
        [this](NxAdapter &adapter)
    {
        adapter.StopPhase2();
    });

    CxPostReleaseHardwareHandled.Set();

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::CheckPowerPolicyOwnership(
    void
)
{
    m_flags.IsPowerPolicyOwner = GetPowerPolicyOwnership();
    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::RemovedReportRemoveToNdis()
{
    m_state = DeviceState::Removed;

    (void)IoWMIRegistrationControl(WdfDeviceWdmGetDeviceObject(GetFxObject()),
        WMIREG_ACTION_DEREGISTER);

    Verifier_VerifyDeviceAdapterCollectionIsEmpty(
        m_nxDriver->GetPrivateGlobals(),
        this,
        &m_adapterCollection);

    WdfDeviceObjectCleanupHandled.Set();

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::PrepareForRebalance()
/*
Description:

    This state entry function handles a PnP start after a PnP stop due to a
    resource rebalance
*/
{
    m_state = DeviceState::Initialized;

    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.PrepareForRebalance();
    });

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::Started(
    void
)
{
    m_state = DeviceState::Started;

    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.PnPStartComplete();
    });

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::PrepareHardware(
    void
)
{
    auto ntStatus = WdfDeviceCreateDeviceInterface(
        GetFxObject(),
        &GUID_DEVINTERFACE_NETCX,
        &DeviceInterface_RefString);

    CxPrePrepareHardwareHandled.Set(ntStatus);

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::PrepareHardwareFailedCleanup(
    void
)
{
    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.FullStop(m_state);
    });

    CxPrePrepareHardwareFailedCleanupHandled.Set();
    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
void
NxDevice::RefreshAdapterList(
    void
)
/*

Description:

    This SM entry function is called when the client driver calls NetAdapterStart or
    NetAdapterStop, it goes through the list of adapters looking for pending states
    and perform the needed operations.

*/
{
    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.Refresh(m_state);
    });
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







    #ifdef _KERNEL_MODE
    StateMachineEngineConfig smConfig(WdfDeviceWdmGetDeviceObject(GetFxObject()), NETADAPTERCX_TAG);
    #else
    StateMachineEngineConfig smConfig;
    #endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxDeviceStateMachine::Initialize(smConfig),
        "StateMachineEngine_Init failed");

    // Default supported OID List
    m_oidListCount = ARRAYSIZE(ndisHandledWdfOids);

    auto allocationSize = m_oidListCount * sizeof(NDIS_OID);
    m_oidList = MakeSizedPoolPtrNP<NDIS_OID>(NETADAPTERCX_TAG, allocationSize);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_oidList);

    RtlCopyMemory(m_oidList.get(), &ndisHandledWdfOids[0], allocationSize);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxDevice::ReleaseRemoveLock(
    IRP * Irp
)
{
    Mx::MxReleaseRemoveLock(&m_removeLock, Irp);
}

void
NxDevice::SurpriseRemoved(
    void
)
{
    NT_ASSERT(!m_flags.SurpriseRemoved);
    m_flags.SurpriseRemoved = true;
}

NxDriver *
NxDevice::GetNxDriver(
    void
) const
{
    return m_nxDriver;
}

_Use_decl_annotations_
NxDevice::NxDevice(
    NX_PRIVATE_GLOBALS * NxPrivateGlobals
)
    : m_nxDriver(NxPrivateGlobals->NxDriver)
{
    Mx::MxInitializeRemoveLock(&m_removeLock, NETADAPTERCX_TAG, 0, 0);
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

    //
    // register WMI after NxDevice being succesfully created
    // because we will deregister inside NxDevice's cleanup callback
    //
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        IoWMIRegistrationControl(WdfDeviceWdmGetDeviceObject(Device),
                                 WMIREG_ACTION_REGISTER),
        "IoWMIRegisterationControl Failed, device=%p", nxDevice);

    nxDevice->EnqueueEvent(NxDevice::Event::CxPrePrepareHardware);
    return nxDevice->CxPrePrepareHardwareHandled.Wait();
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

    nxDevice->EnqueueEvent(NxDevice::Event::CxPrePrepareHardwareFailedCleanup);

    nxDevice->CxPrePrepareHardwareFailedCleanupHandled.Wait();
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

    nxDevice->EnqueueEvent(NxDevice::Event::CxPreReleaseHardware);

    // This will ensure the client's release hardware won't be called
    // before we are in DeviceRemovingReleaseClient state where the
    // adapter has been halted and surprise remove has been handled
    //
    // Even if the status is not a success code WDF will call the client's
    // EvtDeviceReleaseHardware
    nxDevice->CxPreReleaseHardwareHandled.Wait();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePostReleaseHardware(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesTranslated
)
/*++

Routine Description:

    Called by WDF after calling the client's EvtDeviceReleaseHardware.

--*/
{
    auto nxDevice = GetNxDeviceFromHandle(Device);

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    nxDevice->EnqueueEvent(NxDevice::Event::CxPostReleaseHardware);
    nxDevice->CxPostReleaseHardwareHandled.Wait();

    return STATUS_SUCCESS;
}

bool
NxDevice::GetPowerPolicyOwnership(
    void
)
{
    NTSTATUS ntStatus = WdfDeviceStopIdleWithTag(
        GetFxObject(),
        FALSE,
        PTR_TO_TAG(EvtCxDevicePostSelfManagedIoInit));

    if (!NT_SUCCESS(ntStatus))
    {
        //
        // Device is not the power policy owner. Replace with better WDF API
        // once available.
        //
        LogInfo(GetRecorderLog(), FLAG_DEVICE, "Device is not the power policy owner");
        return false;
    }

    WdfDeviceResumeIdleWithTag(GetFxObject(), PTR_TO_TAG(EvtCxDevicePostSelfManagedIoInit));
    return true;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePostSelfManagedIoInit(
    WDFDEVICE Device
)
/*++

Routine Description:

    Called by WDF after a successful call to the client's
    EvtDeviceSelfManagedIoInit.

    If the client's EvtDeviceSelfManagedIoInit fails this
    callback won't be called.

    This routine informs NDIS that it is ok to start the data path for the
    client.

--*/
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->EnqueueEvent(NxDevice::Event::CxPostSelfManagedIoInit);
    return nxDevice->CxPostSelfManagedIoInitHandled.Wait();
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::InitializeSelfManagedIo(
    void
)
{
    m_state = DeviceState::SelfManagedIoInitialized;

    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.InitializeSelfManagedIO();
    });

    auto ntStatus = IoRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        0,
        (void *)&GUID_DEVINTERFACE_NETCX,
        WdfDriverWdmGetDriverObject(WdfGetDriver()),
        DeviceInterfaceChangeNotification,
        GetFxObject(),
        &m_plugPlayNotificationHandle);

    CxPostSelfManagedIoInitHandled.Set(ntStatus);

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReinitializeSelfManagedIo(
    void
)
{
    m_state = DeviceState::SelfManagedIoInitialized;

    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.InitializeSelfManagedIO();
    });

    CxPostSelfManagedIoRestartHandled.Set();
    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePostSelfManagedIoRestart(
    WDFDEVICE Device
)
/*++

Routine Description:

    Called by WDF after a successful call to the client's
    EvtDeviceSelfManagedIoRestart.

    If the client's EvtDeviceSelfManagedIoRestart fails
    this callback won't be called.

    This routine informs the adapter that it is ok to start the data path
    for the client from WDF perspective.

--*/
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->EnqueueEvent(NxDevice::Event::CxPostSelfManagedIoRestart);
    nxDevice->CxPostSelfManagedIoRestartHandled.Wait();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::RestartSelfManagedIo(
    void
)
{
    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.RestartSelfManagedIo(
            m_systemPowerAction,
            m_targetSystemPowerState,
            m_targetDevicePowerState);
    });

    CxPostSelfManagedIoRestartHandled.Set();

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NTSTATUS
EvtCxDevicePreSelfManagedIoSuspend(
    WDFDEVICE Device
)
/*++

Routine Description:

    Called by WDF before calling the client's
    EvtDeviceSelfManagedIoSuspend.

    This routine informs NDIS that it must synchronously pause the clients
    datapath.
--*/
{
    auto nxDevice = GetNxDeviceFromHandle(Device);

    nxDevice->EnqueueEvent(NxDevice::Event::CxPreSelfManagedIoSuspend);

    nxDevice->CxPreSelfManagedIoSuspendHandled.Wait();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
EvtCxDevicePostSelfManagedIoCleanup(
    WDFDEVICE Device
)
{
    auto nxDevice = GetNxDeviceFromHandle(Device);
    nxDevice->EnqueueEvent(NxDevice::Event::CxPostSelfManagedIoCleanup);
    nxDevice->CxPostSelfManagedIoCleanupHandled.Wait();
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::SelfManagedIoCleanup(
    void
)
{
    IoUnregisterPlugPlayNotificationEx(m_plugPlayNotificationHandle);
    CxPostSelfManagedIoCleanupHandled.Set();

    return NxDevice::Event::SyncSuccess;
}

void
NxDevice::StartComplete(
    void
)
{
    EnqueueEvent(NxDevice::Event::StartComplete);
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::SuspendSelfManagedIo(
    void
)
/*++
Routine Description:
    If the device is in the process of being powered down then it notifies
    NDIS of the power transition along with the reason for the device power
    transition.

--*/
{
    if (m_targetDevicePowerState == PowerDeviceUnspecified)
    {
        m_state = DeviceState::ReleasingPhase1Pending;
    }

    m_adapterCollection.ForEach(
        [this](NxAdapter & adapter)
    {
        adapter.SuspendSelfManagedIo(
            m_systemPowerAction,
            m_targetSystemPowerState,
            m_targetDevicePowerState);
    });

    CxPreSelfManagedIoSuspendHandled.Set();

    return NxDevice::Event::SyncSuccess;
}

bool
NxDevice::IsDeviceInPowerTransition(
    void
)
{
    NT_ASSERT(m_targetDevicePowerState < PowerDeviceMaximum);
    return m_targetDevicePowerState != PowerDeviceUnspecified;
}

_Use_decl_annotations_
void
EvtCxDevicePreSurpriseRemoval(
    WDFDEVICE Device
)
/*++

Routine Description:

    Called by WDF before calling the client's
    EvtDeviceSurpriseRemoval.

    This routine keeps a note that this is a case of Surprise Remove.

--*/
{
    GetNxDeviceFromHandle(Device)->SurpriseRemoved();
}

_Use_decl_annotations_
NTSTATUS
WdmIrpCompleteFromNdis(
    DEVICE_OBJECT *DeviceObject,
    IRP *Irp,
    void *Context
)
/*++
Routine Description:

    This is a wdm irp completion routine for the IRPs that were forwarded to
    ndis.sys directly from a pre-processor routine.

    It release a removelock that was acquired in the preprocessor routine

--*/
{
    UNREFERENCED_PARAMETER(DeviceObject);

    auto device = static_cast<NxDevice *>(Context);
    device->ReleaseRemoveLock(Irp);

    //
    // Propagate the Pending Flag
    //
    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }

    return STATUS_CONTINUE_COMPLETION;
}

_Use_decl_annotations_
NTSTATUS
WdmIrpCompleteSetPower(
    DEVICE_OBJECT *DeviceObject,
    IRP *Irp,
    void *Context
)
/*++
Routine Description:
    This completion routine is registered when the device
    receives a power IRP requesting it to go to D0/Dx.

--*/
{
    UNREFERENCED_PARAMETER(DeviceObject);

    auto irpStack = IoGetCurrentIrpStackLocation(Irp);

    auto device = static_cast<NxDevice *>(Context);
    device->PostSetPowerIrp(irpStack->Parameters.Power);

    //
    // Propagate the Pending Flag
    //
    if (Irp->PendingReturned)
    {
        IoMarkIrpPending(Irp);
    }

    return STATUS_CONTINUE_COMPLETION;
}

_Use_decl_annotations_
NTSTATUS
DeviceInterfaceChangeNotification(
    void *NotificationStructure,
    void *Context
)
{
    auto notification = static_cast<DEVICE_INTERFACE_CHANGE_NOTIFICATION *>(NotificationStructure);

    if (notification->Event == GUID_DEVICE_INTERFACE_ARRIVAL)
    {
        NT_ASSERT(notification->InterfaceClassGuid == GUID_DEVINTERFACE_NETCX);

        // This notification is not synchronized with other PnP events,
        // because of that before we notify the device of the start
        // completion we open a handle as a way to synchronize against
        // PnP removal
        OBJECT_ATTRIBUTES objAttributes;

        InitializeObjectAttributes(
            &objAttributes,
            notification->SymbolicLinkName,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            nullptr,
            nullptr);

        wil::unique_any<HANDLE, decltype(::ZwClose), ZwClose> handle;
        IO_STATUS_BLOCK statusBlock = {};

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            ZwOpenFile(
                handle.addressof(),
                0,
                &objAttributes,
                &statusBlock,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                FILE_OPEN),
            "Failed to open handle to device");

        FILE_OBJECT *fileObject;

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            ObReferenceObjectByHandle(
                handle.get(),
                0,
                *IoFileObjectType,
                KernelMode,
                reinterpret_cast<void **>(&fileObject),
                nullptr),
            "ObReferenceObjectByHandle failed.");

        NT_ASSERT(fileObject->FsContext == NETADAPTERCX_DEVICE_INTERFACE_SIGNATURE);

        auto device = static_cast<WDFDEVICE>(Context);
        auto pdo = WdfDeviceWdmGetPhysicalDevice(device);

        if (pdo == fileObject->DeviceObject)
        {
            auto nxDevice = GetNxDeviceFromHandle(device);
            nxDevice->StartComplete();
        }

        ObDereferenceObject(fileObject);
    }

    return STATUS_SUCCESS;
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

        if (RtlEqualUnicodeString(
            &refString,
            &DeviceInterface_RefString,
            TRUE))
        {
            return DeviceInterfaceType::NetAdapterCx;
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
    else if (StackLocation.FileObject->FsContext == NETADAPTERCX_DEVICE_INTERFACE_SIGNATURE)
    {
        return DeviceInterfaceType::NetAdapterCx;
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

    if (deviceInterfaceType == DeviceInterfaceType::NetAdapterCx)
    {
        irpStack->FileObject->FsContext = NETADAPTERCX_DEVICE_INTERFACE_SIGNATURE;

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
        return STATUS_SUCCESS;
    }
    else if (deviceInterfaceType == DeviceInterfaceType::ClientDriver)
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

    if (deviceInterfaceType == DeviceInterfaceType::NetAdapterCx)
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
        return STATUS_SUCCESS;
    }
    else if (deviceInterfaceType == DeviceInterfaceType::ClientDriver)
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
    else if (deviceInterfaceType == DeviceInterfaceType::NetAdapterCx)
    {
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

        return STATUS_NOT_SUPPORTED;
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
    LogVerbose(GetRecorderLog(), FLAG_DEVICE,
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

    //
    // Acquire a wdm remove lock, the lock is released in the
    // completion routine: WdmIrpCompleteFromNdis
    //
    NTSTATUS status = Mx::MxAcquireRemoveLock(&m_removeLock, Irp);

    if (!NT_SUCCESS(status)) {

        NT_ASSERT(status == STATUS_DELETE_PENDING);
        LogInfo(GetRecorderLog(), FLAG_DEVICE,
            "Mx::MxAcquireRemoveLock failed, irp 0x%p, %!STATUS!", Irp, status);

        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    IoCopyCurrentIrpStackLocationToNext(Irp);

#pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    SetCompletionRoutineSmart(WdfDeviceWdmGetDeviceObject(device),
        Irp,
        WdmIrpCompleteFromNdis,
        (void *)this,
        TRUE,
        TRUE,
        TRUE);

    //
    // We need to explicitly adjust the IrpStackLocation since we do not call
    // IoCallDriver to send this Irp to NDIS.sys. Rather we hand this irp to
    // Ndis.sys in function call.
    //
    IoSetNextIrpStackLocation(Irp);

    status = WmiIrpDispatch(Irp, DispatchContext);

    return status;
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

        //
        //  Determine how much memory we need to allocate.
        //
        ULONG cCommonGuids = m_cGuidToOidMap;
        ULONG commonSizeNeeded = sizeof(WMIREGINFO) + (cCommonGuids * sizeof(WMIREGGUID));

        //
        //  We need to give this above information back to WMI.
        //
        if (WmiRegInfoSize < commonSizeNeeded)
        {
            ASSERT(WmiRegInfoSize >= 4);

            *((ULONG *)WmiRegInfo) = (commonSizeNeeded);
            *ReturnSize = sizeof(ULONG);
            return STATUS_BUFFER_TOO_SMALL;
        }

        //
        //  Get a pointer to the buffer passed in.
        //
        PWMIREGINFO pwri = WmiRegInfo;

        *ReturnSize = commonSizeNeeded;

        RtlZeroMemory(pwri, commonSizeNeeded);

        //
        //  Initialize the pwri struct for the common Oids.
        //
        pwri->BufferSize = commonSizeNeeded;
        pwri->NextWmiRegInfo = 0;
        pwri->GuidCount = cCommonGuids;

        //
        //  Go through the GUIDs that we support.
        //

        PNDIS_GUID pndisguid;
        PWMIREGGUID pwrg;
        size_t c;
        for (c = 0, pndisguid = m_pGuidToOidMap.get(), pwrg = pwri->WmiRegGuid;
            (c < cCommonGuids);
            c++, pndisguid++, pwrg++)
        {
            if (pndisguid->Guid == GUID_POWER_DEVICE_ENABLE ||
                pndisguid->Guid == GUID_POWER_DEVICE_WAKE_ENABLE)
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
        //
        if (!NT_SUCCESS(status) || WI_IsFlagSet(Wnode->WnodeHeader.Flags, WNODE_FLAG_TOO_SMALL))
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
    NTSTATUS status;
    NDIS_GUID * guid;

    CX_RETURN_IF_NOT_NT_SUCCESS(WmiGetGuid(Wnode->WnodeHeader.Guid, &guid));

    UNICODE_STRING instanceName;
    instanceName.Buffer = (PWSTR)((PUCHAR)Wnode + Wnode->OffsetInstanceName + sizeof(USHORT));
    instanceName.Length = *(PUSHORT)((PUCHAR)Wnode + Wnode->OffsetInstanceName);
    instanceName.MaximumLength = *(PUSHORT)((PUCHAR)Wnode + Wnode->OffsetInstanceName);

    auto adapter = m_adapterCollection.FindAndReferenceAdapterByInstanceName(&instanceName);

    CX_WARNING_RETURN_NTSTATUS_IF_MSG(STATUS_WMI_INSTANCE_NOT_FOUND,
        !adapter,
        "Adapter(%wZ) not found.",
        &instanceName);

    NDIS_HANDLE miniport = adapter->GetNdisHandle();
    switch(MinorFunction)
    {
        case IRP_MN_QUERY_SINGLE_INSTANCE:
            status = NdisWdfQuerySingleInstance(miniport, guid, Wnode, BufferSize, ReturnSize);
            break;

        case IRP_MN_CHANGE_SINGLE_INSTANCE:
            status = NdisWdfChangeSingleInstance(miniport, guid, Wnode);
            break;

        default:
            status = STATUS_NOT_SUPPORTED;
    }

    NdisWdfMiniportDereference(adapter->GetNdisHandle());

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::WmiEnableEvents(
    GUID const & Guid
)
{
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

    auto adapter = m_adapterCollection.FindAndReferenceAdapterByInstanceName(&instanceName);

    CX_WARNING_RETURN_NTSTATUS_IF_MSG(STATUS_WMI_INSTANCE_NOT_FOUND,
        !adapter,
        "Adapter(%wZ) not found.",
        &instanceName);

    NDIS_HANDLE miniport = adapter->GetNdisHandle();

    NTSTATUS status = NdisWdfExecuteMethod(miniport, guid, Wnode, BufferSize, ReturnSize);

    NdisWdfMiniportDereference(adapter->GetNdisHandle());

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

    In case this is a IRP_MN_REMOVE_DEVICE irp, we want to call
    Mx::MxReleaseRemoveLockAndWait to ensure that all existing remove locks have
    been release and no new remove locks can be acquired.

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
    case IRP_MN_REMOVE_DEVICE:
        //
        // Acquiring a RemoveLock is required before calling
        // Mx::MxReleaseRemoveLockAndWait
        //
        status = Mx::MxAcquireRemoveLock(&m_removeLock, Irp);

        NT_ASSERT(NT_SUCCESS(status));

        Mx::MxReleaseRemoveLockAndWait(&m_removeLock, Irp);
        break;
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
    IRP *Irp,
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
        LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ_CREATE Device 0x%p, IRP 0x%p\n", deviceObj, Irp);

        status = nxDevice->WdmCreateIrpPreProcess(Irp, DispatchContext);
        break;
    case IRP_MJ_CLOSE:
        LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
            "EvtWdfCxDeviceWdmIrpPreProcess IRP_MJ_CLOSE Device 0x%p, IRP 0x%p\n", deviceObj, Irp);

        status = nxDevice->WdmCloseIrpPreProcess(Irp, DispatchContext);
        break;
    case IRP_MJ_DEVICE_CONTROL:
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
    case IRP_MJ_WRITE:
    case IRP_MJ_READ:
    case IRP_MJ_CLEANUP:
        LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
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
    IRP *Irp,
    WDFCONTEXT DispatchContext
)
{
    auto irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_FRE_ASSERT(irpStack->MajorFunction == IRP_MJ_PNP);
    NT_FRE_ASSERT(
        irpStack->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE ||
        irpStack->MinorFunction == IRP_MN_REMOVE_DEVICE);

    auto nxDevice = GetNxDeviceFromHandle(Device);

    LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
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
EvtWdmPowerIrpPreprocessRoutine(
    WDFDEVICE Device,
    IRP *Irp,
    WDFCONTEXT DispatchContext
)
{
    auto irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_FRE_ASSERT(irpStack->MajorFunction == IRP_MJ_POWER);
    NT_FRE_ASSERT(irpStack->MinorFunction == IRP_MN_SET_POWER);

    auto nxDevice = GetNxDeviceFromHandle(Device);

    LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
        "EvtWdmPowerIrpPreprocessRoutine IRP_MJ_POWER. WDFDEVICE=%p, IRP=%p, irpStack->MinorFunction=0x%x",
        Device,
        Irp,
        irpStack->MinorFunction);

    nxDevice->PreSetPowerIrp(irpStack->Parameters.Power);

    IoCopyCurrentIrpStackLocationToNext(Irp);

    SetCompletionRoutineSmart(
        WdfDeviceWdmGetDeviceObject(Device),
        Irp,
        WdmIrpCompleteSetPower,
        nxDevice,
        TRUE,
        TRUE,
        TRUE);

    return WdfDeviceWdmDispatchIrp(
        Device,
        Irp,
        DispatchContext);
}

_Use_decl_annotations_
NTSTATUS
EvtWdmWmiIrpPreprocessRoutine(
    WDFDEVICE Device,
    IRP *Irp,
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
    nxDevice->EnqueueEvent(NxDevice::Event::WdfDeviceObjectCleanup);
    nxDevice->WdfDeviceObjectCleanupHandled.Wait();
}

RECORDER_LOG
NxDevice::GetRecorderLog(
    void
)
{
    return m_nxDriver->GetRecorderLog();
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
    UNREFERENCED_PARAMETER(TransitionType);

    TraceLoggingWrite(
        g_hNetAdapterCxEtwProvider,
        "NxDeviceStateTransition",
        TraceLoggingDescription("NxDevicePnPStateTransition"),
        TraceLoggingKeyword(NET_ADAPTER_CX_NXDEVICE_PNP_STATE_TRANSITION),
        TraceLoggingUInt32(GetDeviceBusAddress(), "DeviceBusAddress"),
        TraceLoggingHexInt32(static_cast<INT32>(SourceState), "DeviceStateTransitionFrom"),
        TraceLoggingHexInt32(static_cast<INT32>(ProcessedEvent), "DeviceStateTransitionEvent"),
        TraceLoggingHexInt32(static_cast<INT32>(TargetState), "DeviceStateTransitionTo"));
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
NxDevice::EvtMachineDestroyed()
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

bool
NxDevice::IsPowerPolicyOwner(
    void
) const
{
    return !!m_flags.IsPowerPolicyOwner;
}

_Use_decl_annotations_
void
NxDevice::PreSetPowerIrp(
    PowerIrpParameters const & PowerParameters
)
{
    if (PowerParameters.Type == SystemPowerState)
    {
        m_systemPowerAction = PowerParameters.ShutdownType;
        m_targetSystemPowerState = PowerParameters.State.SystemState;
    }
    else if (PowerParameters.Type == DevicePowerState)
    {
        m_targetDevicePowerState = PowerParameters.State.DeviceState;
    }
}

_Use_decl_annotations_
void
NxDevice::PostSetPowerIrp(
    PowerIrpParameters const & PowerParameters
)
{
    if (PowerParameters.Type == DevicePowerState)
    {
        m_systemPowerAction = PowerActionNone;
        m_targetSystemPowerState = PowerSystemUnspecified;
        m_targetDevicePowerState = PowerDeviceUnspecified;
    }
}

_Use_decl_annotations_
void
NxDevice::GetPowerList(
    NxPowerList * PowerList
)
{
    m_adapterCollection.ForEach(
        [this, PowerList](NxAdapter & Adapter)
        {
            auto powerPolicy = &Adapter.m_powerPolicy;

            powerPolicy->UpdatePowerList(
                IsDeviceInPowerTransition(),
                PowerList);
        });
}

_Use_decl_annotations_
NTSTATUS
NxDevice::PowerReference(
    bool WaitForD0,
    void * Tag
)
/*++

Routine description:

    This routine acquires a power reference on the WDFDEVICE.
    It also tracks failures in a way that enables callers to
    always issue Ref/Deref calls, regardless if the actual
    WDF call failed.

--*/
{
    NTSTATUS status = WdfDeviceStopIdleWithTag(
        GetFxObject(),
        WaitForD0,
        Tag);

    if (!NT_SUCCESS(status))
    {
        LogWarning(GetRecorderLog(), FLAG_DEVICE,
            "WdfDeviceStopIdle failed %!STATUS!. WDFDEVICE=%p", status, GetFxObject());

        //
        // Track WdfDeviceStopIdle failures so as to avoid imbalance of stop idle
        // and resume idle.
        //
        InterlockedIncrement(&m_powerRefFailureCount);

        return status;
    }

    return status;
}

_Use_decl_annotations_
void
NxDevice::PowerDereference(
    void * Tag
)
{
    //
    // -1 is returned if failure count was already 0 (floor). This denotes no
    // failed calls to _EvtNdisPowerReference need to be accounted for.
    // If any other value is returned the failure count was decremented
    // successfully so no need to call WdfDeviceResumeIdle
    //
    if (-1 == NxInterlockedDecrementFloor(&m_powerRefFailureCount, 0))
    {
        WdfDeviceResumeIdleWithTag(
            GetFxObject(),
            Tag);
    }
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
        LogWarning(GetRecorderLog(), FLAG_DEVICE,
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

        LogInfo(GetRecorderLog(), FLAG_DEVICE, "Performing Device Reset");
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
            LogWarning(GetRecorderLog(), FLAG_DEVICE,
                "Device Reset failed %!STATUS!. WDFDEVICE=%p", status, GetFxObject());

            // If Device Reset fails, perform WdfDeviceSetFailed.
            LogInfo(GetRecorderLog(), FLAG_DEVICE,
                "Performing WdfDeviceSetFailed with WdfDeviceFailedAttemptRestart WDFDEVICE=%p", GetFxObject());

            WdfDeviceSetFailed(GetFxObject(), WdfDeviceFailedAttemptRestart);

            LogInfo(GetRecorderLog(), FLAG_DEVICE,
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
        LogInfo(GetRecorderLog(), FLAG_DEVICE,
            "Performing WdfDeviceSetFailed with WdfDeviceFailedAttemptRestart WDFDEVICE=%p", GetFxObject());

        WdfDeviceSetFailed(GetFxObject(), WdfDeviceFailedAttemptRestart);

        LogInfo(GetRecorderLog(), FLAG_DEVICE,
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
    PFUNCTION_LEVEL_RESET_PARAMETERS Parameters;
    Parameters = (PFUNCTION_LEVEL_RESET_PARAMETERS)Context;
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
