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
#include "NxMacros.hpp"
#include "verifier.hpp"
#include "version.hpp"

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

static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmIrpPreprocessRoutine;
static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmPnpPowerIrpPreprocessRoutine;
static EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmWmiIrpPreprocessRoutine;

static IO_COMPLETION_ROUTINE WdmIrpCompleteFromNdis;
static IO_COMPLETION_ROUTINE WdmIrpCompleteSetPower;

static DRIVER_NOTIFICATION_CALLBACK_ROUTINE DeviceInterfaceChangeNotification;

// Note: 0x5 and 0x3 were chosen as the device interface signatures because
// ExAllocatePool will allocate memory aligned to at least 8-byte boundary
static void * const NETADAPTERCX_DEVICE_INTERFACE_SIGNATURE = (void *)(0x3);
static void * const SIDE_BAND_DEVICE_INTERFACE_SIGNATURE = (void *)(0x5);

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

enum class DeviceInterfaceType
{
    Ndis = 0,
    NetAdapterCx,
    SideBand,
};

static
void
_FunctionLevelResetCompletion(
    _In_ NTSTATUS Status,
    _Inout_opt_ PVOID Context
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

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
            CxDeviceInit,
            EvtWdmPnpPowerIrpPreprocessRoutine,
            IRP_MJ_PNP,
            nullptr,
            0),
        "WdfDeviceInitAssignWdmIrpPreprocessCallback failed for IRP_MJ_PNP");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
            CxDeviceInit,
            EvtWdmPnpPowerIrpPreprocessRoutine,
            IRP_MJ_POWER,
            nullptr,
            0),
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
    PWDFCXDEVICE_INIT CxDeviceInit
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

NTSTATUS
NxDevice::AdapterAdd(
    _In_ NxAdapter *Adapter
)
/*++
Routine Description:
    Invoked by adapter object to inform the device about adapter creation
    in response to a client calling NetAdapterCreate from it's device add
    callback. In response NxDevice will report NdisWdfPnPAddDevice to NDIS.
--*/
{
    NTSTATUS status;
    WDFDEVICE device;
    WDFDRIVER driver;

    device = GetFxObject();
    NT_ASSERT(device != NULL);
    driver = WdfDeviceGetDriver(device);

    status = NdisWdfPnPAddDevice(WdfDriverWdmGetDriverObject(driver),
                            WdfDeviceWdmGetPhysicalDevice(device),
                            &Adapter->m_NdisAdapterHandle,
                            reinterpret_cast<NDIS_HANDLE>(Adapter));
    if (!NT_SUCCESS(status)) {
        //
        // NOTE: In case of failure, WDF object cleanup for WDFDEVICE will post
        // the cleanup event to the device state machine.
        //
        LogError(GetRecorderLog(), FLAG_DEVICE,
            "NdisWdfPnpAddDevice failed %!STATUS!", status);
        return status;
    }

    m_AdapterCollection.AddAdapter(Adapter);

    return status;
}

_Use_decl_annotations_
bool
NxDevice::RemoveAdapter(
    NxAdapter *Adapter
    )
{
    return m_AdapterCollection.RemoveAdapter(Adapter);
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::AreAllAdaptersHalted(
    void
    )
{
    if(InterlockedCompareExchange((LONG *)&m_NdisInitializeCount, 0, 0) == 0)
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
    InterlockedIncrement((LONG *)&m_NdisInitializeCount);
}

void
NxDevice::AdapterHalted(
    void
    )
{
    InterlockedDecrement((LONG *)&m_NdisInitializeCount);

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
    m_State = DeviceState::ReleasingPhase2Pending;

    if (m_Flags.SurpriseRemoved)
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
    m_AdapterCollection.ForEachAdapterLocked(
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
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        adapter.FullStop(m_State);
    });

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReleasingReportPreReleaseToNdis()
{
    m_AdapterCollection.ForEachAdapterLocked(
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
    m_State = DeviceState::Released;

    // The adapter collection lock should not be acquired here, otherwise
    // we might deadlock with a thread trying to cleanup an user mode handle.
    //
    // It is safe to not acquire the lock because this is always executed after
    // EvtDeviceReleaseHardware has returned, which is the last point a client
    //driver is allowed to delete a NETADAPTER
    m_AdapterCollection.ForEachAdapterUnlocked(
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
    m_Flags.IsPowerPolicyOwner = GetPowerPolicyOwnership();
    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::RemovedReportRemoveToNdis()
{
    m_State = DeviceState::Removed;

    (void)IoWMIRegistrationControl(WdfDeviceWdmGetDeviceObject(GetFxObject()),
        WMIREG_ACTION_DEREGISTER);

    Verifier_VerifyDeviceAdapterCollectionIsEmpty(
        m_NxDriver->GetPrivateGlobals(),
        this,
        &m_AdapterCollection);

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
    m_State = DeviceState::Initialized;

    m_AdapterCollection.ForEachAdapterLocked(
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
    m_State = DeviceState::Started;

    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        if (adapter.GetCurrentState() == AdapterState::Started)
        {
            NdisWdfMiniportStarted(adapter.GetNdisHandle());
        }
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
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        adapter.FullStop(m_State);
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
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        adapter.Refresh(m_State);
    });
}

NTSTATUS
NxDevice::_Create(
    _In_  NX_PRIVATE_GLOBALS *          PrivateGlobals,
    _In_  WDFDEVICE                     Device,
    _Out_ NxDevice **                   NxDeviceParam
)
/*++
Routine Description:
    Static method that creates the NXDEVICE object, corresponding to the
    WDFDEVICE.

    Currently NxDevice creation is closely tied to the creation of the
    default NxAdapter object.
--*/
{
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // First Apply a context on the device
    //

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxDevice);

    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxDevice::_EvtCleanup;

    //
    // Ensure that the destructor would be called when this object is distroyed.
    //
    NxDevice::_SetObjectAttributes(&attributes);

    void * nxDeviceMemory;
    status = WdfObjectAllocateContext(Device,
                                      &attributes,
                                      &nxDeviceMemory);

    if (!NT_SUCCESS(status)) {
        LogError(PrivateGlobals->NxDriver->GetRecorderLog(), FLAG_DEVICE,
                 "WdfObjectAllocateContext failed %!STATUS!", status);
        return status;
    }

    //
    // Use the inplacement new and invoke the constructor on the
    // NxAdapter's memory
    //
    auto nxDevice = new (nxDeviceMemory) NxDevice(PrivateGlobals,
                                             Device);

    __analysis_assume(nxDevice != NULL);

    NT_ASSERT(nxDevice);

    status = nxDevice->Init();

    if (!NT_SUCCESS(status)) {
        LogError(PrivateGlobals->NxDriver->GetRecorderLog(), FLAG_DEVICE,
            "Failed to initialize NxDevice, nxDevice=%p, status=%!STATUS!", nxDevice, status);
        return status;
    }

    *NxDeviceParam = nxDevice;

    return status;
}

NTSTATUS
NxDevice::Init(
    void
    )
{
    #ifdef _KERNEL_MODE
    StateMachineEngineConfig smConfig(WdfDeviceWdmGetDeviceObject(GetFxObject()), NETADAPTERCX_TAG);
    #else
    StateMachineEngineConfig smConfig;
    #endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        Initialize(smConfig),
        "StateMachineEngine_Init failed");

    CX_RETURN_IF_NOT_NT_SUCCESS(m_AdapterCollection.Initialize());

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxDevice::ReleaseRemoveLock(
    IRP *Irp
    )
{
    Mx::MxReleaseRemoveLock(&m_RemoveLock, Irp);
}

void
NxDevice::SurpriseRemoved(
    void
    )
{
    NT_ASSERT(!m_Flags.SurpriseRemoved);
    m_Flags.SurpriseRemoved = true;
}

NxDriver *
NxDevice::GetNxDriver(
    void
    ) const
{
    return m_NxDriver;
}

NxAdapter *
NxDevice::GetDefaultNxAdapter(
    VOID
    ) const
{
    return m_AdapterCollection.GetDefaultAdapter();
}

NxDevice::NxDevice(
    _In_ NX_PRIVATE_GLOBALS *          NxPrivateGlobals,
    _In_ WDFDEVICE                     Device
    ):
    CFxObject(Device),
    m_NxDriver(NxPrivateGlobals->NxDriver),
    m_SystemPowerAction(PowerActionNone),
    m_LastReportedDxAction(NdisWdfActionPowerNone)
{
    Mx::MxInitializeRemoveLock(&m_RemoveLock, NETADAPTERCX_TAG, 0, 0);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    WDF_DEVICE_PROPERTY_DATA data;
    WDF_OBJECT_ATTRIBUTES objAttributes;
    DEVPROPTYPE devPropType;
    WDFMEMORY memory = nullptr;

    WDF_DEVICE_PROPERTY_DATA_INIT(&data, &DEVPKEY_Device_Address);
    WDF_OBJECT_ATTRIBUTES_INIT(&objAttributes);
    objAttributes.ParentObject = Device;

    status = WdfDeviceAllocAndQueryPropertyEx(
        Device,
        &data,
        PagedPool,
        &objAttributes,
        &memory,
        &devPropType);

    if ((NT_SUCCESS(status)) &&
        (devPropType == DEVPROP_TYPE_UINT32))
    {
        size_t bufferSize;
        PVOID buffer = WdfMemoryGetBuffer(memory, &bufferSize);
        if (bufferSize == sizeof(UINT32))
        {
            m_DeviceBusAddress = *((PUINT32)buffer);
        }
    }

    if (memory != nullptr)
    {
        WdfObjectDelete(memory);
    }

    m_Flags.InDxPowerTransition = FALSE;






    m_NetAdapterCxTriageBlock = &g_NetAdapterCxTriageBlock;
}

NxDevice::~NxDevice()
{
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
    auto  nxDevice = GetNxDeviceFromHandle(Device);

    if (nxDevice == nullptr)
    {
        // If there is no NxDevice context create it
        auto driver = WdfDeviceGetDriver(Device);
        auto nxDriver = GetNxDriverFromWdfDriver(driver);

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NxDevice::_Create(
                nxDriver->GetPrivateGlobals(),
                Device,
                &nxDevice),
            "Failed to create NxDevice context. WDFDEVICE=%p", Device);
    }

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
NxDevice::IsStarted(
    void
    ) const
{
    return m_State == DeviceState::Started;
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
    m_State = DeviceState::SelfManagedIoInitialized;

    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        if (adapter.GetCurrentState() == AdapterState::Started)
        {
            adapter.InitializeSelfManagedIO();
        }
    });

    auto ntStatus = IoRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        0,
        (void *)&GUID_DEVINTERFACE_NETCX,
        WdfDriverWdmGetDriverObject(WdfGetDriver()),
        DeviceInterfaceChangeNotification,
        GetFxObject(),
        &m_PlugPlayNotificationHandle);

    CxPostSelfManagedIoInitHandled.Set(ntStatus);

    return NxDevice::Event::SyncSuccess;
}

_Use_decl_annotations_
NxDevice::Event
NxDevice::ReinitializeSelfManagedIo(
    void
    )
{
    m_State = DeviceState::SelfManagedIoInitialized;

    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        if (adapter.GetCurrentState() == AdapterState::Started)
        {
            adapter.InitializeSelfManagedIO();
        }
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
    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        if (adapter.GetCurrentState() == AdapterState::Started)
        {
            //
            // Adapter will ensure it triggers restart of the data path
            //
            adapter.RestartSelfManagedIO();

            NT_ASSERT(m_LastReportedDxAction != NdisWdfActionPowerNone);

            //
            // Inform NDIS we're returning from a low power state.
            //



            CX_LOG_IF_NOT_NT_SUCCESS_MSG(
                NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                    NdisWdfActionPowerD0,
                    m_LastReportedDxAction),
                "NdisWdfActionPowerD0 failed. adapter=%p, device=%p", &adapter, this);
        }
    });

    m_LastReportedDxAction = NdisWdfActionPowerNone;

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
    IoUnregisterPlugPlayNotificationEx(m_PlugPlayNotificationHandle);
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
    VOID
)
/*++
Routine Description:
    If the device is in the process of being powered down then it notifies
    NDIS of the power transition along with the reason for the device power
    transition.

--*/
{
    if (m_Flags.InDxPowerTransition)
    {
        auto systemPowerAction = WdfDeviceGetSystemPowerAction(GetFxObject());

        if (m_SystemPowerAction != PowerActionNone
            &&
            (systemPowerAction == PowerActionSleep || systemPowerAction == PowerActionHibernate))
        {
            m_LastReportedDxAction = NdisWdfActionPowerDxOnSystemSx;
        }
        else if (m_SystemPowerAction != PowerActionNone &&
            (systemPowerAction == PowerActionShutdown || systemPowerAction == PowerActionShutdownReset ||
            systemPowerAction == PowerActionShutdownOff))
        {
            m_LastReportedDxAction = NdisWdfActionPowerDxOnSystemShutdown;
        }
        else
        {
            m_LastReportedDxAction = NdisWdfActionPowerDx;
        }
    }
    else
    {
        m_State = DeviceState::ReleasingPhase1Pending;
    }

    m_AdapterCollection.ForEachAdapterLocked(
        [this](NxAdapter & adapter)
    {
        if (adapter.GetCurrentState() == AdapterState::Started)
        {
            if (m_Flags.InDxPowerTransition)
            {




                CX_LOG_IF_NOT_NT_SUCCESS_MSG(
                    NdisWdfPnpPowerEventHandler(adapter.GetNdisHandle(),
                        m_LastReportedDxAction, NdisWdfActionPowerNone),
                    "NdisWdfPnpPowerEventHandler failed. adapter=%p, device=%p", &adapter, this);
            }

            adapter.SuspendSelfManagedIo(m_Flags.InDxPowerTransition);
        }
    });

    m_Flags.InDxPowerTransition = FALSE;

    CxPreSelfManagedIoSuspendHandled.Set();

    return NxDevice::Event::SyncSuccess;
}

BOOLEAN
NxDevice::IsDeviceInPowerTransition(
    VOID
    )
{
    return (m_LastReportedDxAction != NdisWdfActionPowerNone);
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
    PIO_STACK_LOCATION irpStack;
    DEVICE_POWER_STATE devicePowerState;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(irpStack->Parameters.Power.Type == DevicePowerState);
    devicePowerState = irpStack->Parameters.Power.State.DeviceState;

    if (devicePowerState != PowerDeviceD0)
    {
        auto device = static_cast<NxDevice *>(Context);
        device->SetInDxPowerTranstion(false);
    }

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
DeviceInterfaceType
DeviceInterfaceTypeFromFileName(
    _In_opt_ NxAdapter *Adapter,
    _In_ IO_STACK_LOCATION const &StackLocation
    )
/*++

Routine Description:

    This is called by WdmCreateIrpPreProcess to evaluate what kind of
    device interface is being used to open a handle to this device.
    Unfortunately there is no way to determine which device interface
    was used in the open call, so we require the client to create their
    private device interface with a ref string, otherwise NDIS will
    intercept all the IO.

    The reference string 'NetAdapterCx' is reserved for system use, as
    well as the network interface GUIDs of the adapters created on top
    of this device.

Arguments:

    Adapter - Optional, pointer to the default NxAdapter object, if one exists
    StackLocation - Pointer to the current I/O stack location

Return Value:

    The device interface type.

--*/
{
    NT_ASSERT(StackLocation.MajorFunction == IRP_MJ_CREATE);

    // If there is no FileObject, the handle is not being opened
    // on a device interface
    if (StackLocation.FileObject == nullptr)
    {
        return DeviceInterfaceType::Ndis;
    }

    // If the FileName is empty this is not a private device interface
    // nor a NetAdapterCx one
    if (StackLocation.FileObject->FileName.Length == 0)
    {
        return DeviceInterfaceType::Ndis;
    }

    if (StackLocation.FileObject->FileName.Length > sizeof(WCHAR))
    {
        // Make an unicode string without the initial backslash
        UNICODE_STRING refString = {};
        refString.Buffer = &StackLocation.FileObject->FileName.Buffer[1];
        refString.Length = StackLocation.FileObject->FileName.Length - sizeof(OBJ_NAME_PATH_SEPARATOR);
        refString.MaximumLength = StackLocation.FileObject->FileName.MaximumLength - sizeof(OBJ_NAME_PATH_SEPARATOR);

        NT_ASSERT(
            refString.Length % sizeof(WCHAR) == 0 &&
            refString.MaximumLength % sizeof(WCHAR) == 0);

        // Check if the reference string is the network interface GUID
        // of the default adapter
        if (Adapter != nullptr)
        {
            auto const & baseName = Adapter->GetBaseName();

            if (RtlEqualUnicodeString(
                &refString,
                &baseName,
                TRUE))
            {
                return DeviceInterfaceType::Ndis;
            }
        }

        if (RtlEqualUnicodeString(
            &refString,
            &DeviceInterface_RefString,
            TRUE))
        {
            return DeviceInterfaceType::NetAdapterCx;
        }
    }

    return DeviceInterfaceType::SideBand;
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
        return DeviceInterfaceType::Ndis;
    }

    if (StackLocation.FileObject->FsContext == SIDE_BAND_DEVICE_INTERFACE_SIGNATURE)
    {
        return DeviceInterfaceType::SideBand;
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

    From this routine NetAdapterCx hands the IRP to NDIS in order
    to do some checks and initialize the handle context. Then it
    checks to see if the handle is marked as being on a private
    interface, if yes it hands the request to the client.
--*/
{
    auto device = GetFxObject();
    auto irpStack = IoGetCurrentIrpStackLocation(Irp);

    auto adapter = GetDefaultNxAdapter();

    // If this handle is being opened on a side band device interface or
    // on the NetAdapterCx device interface mark the FsContext so that in
    // future calls for the same handle we know where to dispatch the IRP

    auto deviceInterfaceType = DeviceInterfaceTypeFromFileName(adapter, *irpStack);

    if (deviceInterfaceType == DeviceInterfaceType::NetAdapterCx)
    {
        irpStack->FileObject->FsContext = NETADAPTERCX_DEVICE_INTERFACE_SIGNATURE;

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
        return STATUS_SUCCESS;
    }
    else if (deviceInterfaceType == DeviceInterfaceType::SideBand)
    {
        irpStack->FileObject->FsContext = SIDE_BAND_DEVICE_INTERFACE_SIGNATURE;

        IoSkipCurrentIrpStackLocation(Irp);
        return WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);
    }

    NT_ASSERT(deviceInterfaceType == DeviceInterfaceType::Ndis);

    // If there is no default adapter yet we should just complete the IRP
    if (adapter == nullptr)
    {
        Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
        return STATUS_DEVICE_NOT_READY;
    }

    BOOLEAN unused;
    return NdisWdfCreateIrpHandler(adapter->GetNdisHandle(), Irp, &unused);
}

NTSTATUS
NxDevice::WdmCloseIrpPreProcess(
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
    )
/*++
Routine Description:

    This is a pre-processor routine for IRP_MJ_CLOSE

    From this routine NetAdapterCx hands the IRP to NDIS so it
    can perform cleanup associated with the handle being closed.

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
    else if (deviceInterfaceType == DeviceInterfaceType::SideBand)
    {
        IoSkipCurrentIrpStackLocation(Irp);
        return WdfDeviceWdmDispatchIrp(device, Irp, DispatchContext);
    }

    NT_ASSERT(deviceInterfaceType == DeviceInterfaceType::Ndis);

    BOOLEAN unused;
    return NdisWdfCloseIrpHandler(GetDefaultNxAdapter()->GetNdisHandle(), Irp, &unused);
}

NTSTATUS
NxDevice::WdmIoIrpPreProcess(
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
    )
{
    NTSTATUS                       status;
    PIO_STACK_LOCATION             irpStack;
    WDFDEVICE                      device;

    device = GetFxObject();
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_ASSERT(
        irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ||
        irpStack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL ||
        irpStack->MajorFunction == IRP_MJ_WRITE ||
        irpStack->MajorFunction == IRP_MJ_READ ||
        irpStack->MajorFunction == IRP_MJ_CLEANUP
        );

    auto deviceInterfaceType = DeviceInterfaceTypeFromFsContext(*irpStack);

    if(deviceInterfaceType == DeviceInterfaceType::SideBand)
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

    auto nxAdapter = GetDefaultNxAdapter();

    if (nxAdapter == nullptr)
    {
        Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
        return STATUS_DEVICE_NOT_READY;
    }

    switch (irpStack->MajorFunction)
    {
    case IRP_MJ_DEVICE_CONTROL:
        status = NdisWdfDeviceControlIrpHandler(nxAdapter->m_NdisAdapterHandle, Irp);
        break;
    case IRP_MJ_INTERNAL_DEVICE_CONTROL:
        status = NdisWdfDeviceInternalControlIrpHandler(nxAdapter->m_NdisAdapterHandle, Irp);
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
    _Inout_ PIRP       Irp,
    _In_    WDFCONTEXT DispatchContext
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

    auto defaultNxAdapter = GetDefaultNxAdapter();
    if (defaultNxAdapter == nullptr)
    {
        Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_NOT_READY;
    }

    //
    // Acquire a wdm remove lock, the lock is released in the
    // completion routine: WdmIrpCompleteFromNdis
    //
    NTSTATUS status = Mx::MxAcquireRemoveLock(&m_RemoveLock, Irp);

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
        (PVOID)this,
        TRUE,
        TRUE,
        TRUE);

    //
    // We need to explicitly adjust the IrpStackLocation since we do not call
    // IoCallDriver to send this Irp to NDIS.sys. Rather we hand this irp to
    // Ndis.sys in function call.
    //
    IoSetNextIrpStackLocation(Irp);

    status = NdisWdfDeviceWmiHandler(defaultNxAdapter->GetNdisHandle(), Irp);

    return status;
}


NTSTATUS
NxDevice::WdmPnPIrpPreProcess(
    _Inout_ PIRP       Irp
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
        status = Mx::MxAcquireRemoveLock(&m_RemoveLock, Irp);

        NT_ASSERT(NT_SUCCESS(status));

        Mx::MxReleaseRemoveLockAndWait(&m_RemoveLock, Irp);
        break;
    case IRP_MN_QUERY_REMOVE_DEVICE:
        if (m_FailingDeviceRequestingReset)
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
EvtWdmPnpPowerIrpPreprocessRoutine(
    WDFDEVICE Device,
    IRP *Irp,
    WDFCONTEXT DispatchContext
    )
/*++
Routine Description:

    This is a pre-processor routine for PNP and Power IRPs.

    In case this is a IRP_MN_REMOVE_DEVICE irp, we want to call
    Mx::MxReleaseRemoveLockAndWait to ensure that all existing remove locks have
    been release and no new remove locks can be acquired.

    In case this is an IRP_MN_SET_POWER we inspect the IRP to mark
    the adapter associated with this device as going in or out of a
    power transition
--*/
{
    PIO_STACK_LOCATION             irpStack;
    PDEVICE_OBJECT                 deviceObject;
    BOOLEAN                        skipCurrentStackLocation = TRUE;
    NTSTATUS                       status = STATUS_SUCCESS;
    SYSTEM_POWER_STATE_CONTEXT     systemPowerStateContext;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    NT_ASSERT(irpStack->MajorFunction == IRP_MJ_PNP ||
              irpStack->MajorFunction == IRP_MJ_POWER);

    auto nxDevice = GetNxDeviceFromHandle(Device);
    deviceObject = WdfDeviceWdmGetDeviceObject(Device);

    switch (irpStack->MajorFunction)
    {
        case IRP_MJ_PNP:
            LogVerbose(nxDevice->GetRecorderLog(), FLAG_DEVICE,
                "EvtWdfCxDeviceWdmPnpPowerIrpPreProcess IRP_MJ_PNP Device 0x%p, IRP 0x%p\n", deviceObject, Irp);

            status = nxDevice->WdmPnPIrpPreProcess(Irp);
            break;
        case IRP_MJ_POWER:
            if (irpStack->MinorFunction == IRP_MN_SET_POWER) {
                if (irpStack->Parameters.Power.Type == SystemPowerState) {
                    systemPowerStateContext = irpStack->Parameters.Power.SystemPowerStateContext;

                    if (systemPowerStateContext.TargetSystemState != PowerSystemWorking) {
                        nxDevice->SetSystemPowerAction(irpStack->Parameters.Power.ShutdownType);
                    }
                    else {
                        nxDevice->SetSystemPowerAction(PowerActionNone);
                    }

                    break;
                }

                if (irpStack->Parameters.Power.Type == DevicePowerState) {
                    if (irpStack->Parameters.Power.State.DeviceState == PowerDeviceD3 ||
                        irpStack->Parameters.Power.State.DeviceState == PowerDeviceD2 ||
                        irpStack->Parameters.Power.State.DeviceState == PowerDeviceD1) {
                        nxDevice->SetInDxPowerTranstion(true);
                    }

                    IoCopyCurrentIrpStackLocationToNext(Irp);

                    //
                    // Set completion routine to clear the InDxPowerTransition flag
                    //
                    SetCompletionRoutineSmart(
                        deviceObject,
                        Irp,
                        WdmIrpCompleteSetPower,
                        nxDevice,
                        TRUE,
                        TRUE,
                        TRUE);

                    skipCurrentStackLocation = FALSE;
                }
            }
            break;
        default:
            NT_ASSERTMSG("Should never hit", FALSE);
            break;
    }

    if (status == STATUS_DEVICE_HUNG)
    {
        // special case this since the IRP has been already completed in WdmPnPIrpPreProcess
        // which is the only place where this error code is set, so just return here.
        return status;
    }
    else
    {
        //
        // Allow WDF to process the Pnp Irp normally.
        //
        if (skipCurrentStackLocation) {
            IoSkipCurrentIrpStackLocation(Irp);
        }

        status = WdfDeviceWdmDispatchIrp(Device, Irp, DispatchContext);

        return status;
    }
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

VOID
NxDevice::_EvtCleanup(
    _In_  WDFOBJECT Device
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
    VOID
    )
{
    return m_NxDriver->GetRecorderLog();
}

_Use_decl_annotations_
VOID
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
        TraceLoggingHexInt32(static_cast<INT32>(TargetState), "DeviceStateTransitionTo")
        );
}

_Use_decl_annotations_
VOID
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
VOID
NxDevice::EvtMachineDestroyed()
{
}

_Use_decl_annotations_
VOID
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
    return !!m_Flags.IsPowerPolicyOwner;
}

_Use_decl_annotations_
void
NxDevice::SetSystemPowerAction(
    POWER_ACTION PowerAction
    )
{
    m_SystemPowerAction = PowerAction;
}

_Use_decl_annotations_
void
NxDevice::SetInDxPowerTranstion(
    bool InDxPowerTransition
    )
{
    m_Flags.InDxPowerTransition = InDxPowerTransition;
}

_Use_decl_annotations_
NTSTATUS
NxDevice::PowerReference(
    bool WaitForD0,
    void *Tag
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
        InterlockedIncrement(&m_PowerRefFailureCount);

        return status;
    }

    return status;
}

_Use_decl_annotations_
void
NxDevice::PowerDereference(
    void *Tag
    )
{
    //
    // -1 is returned if failure count was already 0 (floor). This denotes no
    // failed calls to _EvtNdisPowerReference need to be accounted for.
    // If any other value is returned the failure count was decremented
    // successfully so no need to call WdfDeviceResumeIdle
    //
    if (-1 == NxInterlockedDecrementFloor(&m_PowerRefFailureCount, 0))
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
    m_FailingDeviceRequestingReset = TRUE;
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

    RtlZeroMemory(&m_ResetInterface, sizeof(m_ResetInterface));
    m_ResetInterface.Size = sizeof(m_ResetInterface);
    m_ResetInterface.Version = 1;

    status = WdfFdoQueryForInterface(
        GetFxObject(),
        &GUID_DEVICE_RESET_INTERFACE_STANDARD,
        (PINTERFACE)&m_ResetInterface,
        sizeof(m_ResetInterface),
        1,
        NULL);
    if (!NT_SUCCESS(status))
    {
        LogWarning(GetRecorderLog(), FLAG_DEVICE,
            "QueryDeviceResetInterface failed %!STATUS!. WDFDEVICE=%p", status, GetFxObject());
        RtlZeroMemory(&m_ResetInterface, sizeof(m_ResetInterface));
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
    if ((m_ResetInterface.DeviceReset != NULL) &&
        ((m_ResetInterface.SupportedResetTypes & (1 << ResetType)) != 0))
    {
        return TRUE;
    }

    return FALSE;
}

_Use_decl_annotations_
bool
NxDevice::GetSupportedDeviceResetType(
    _Inout_ PULONG SupportedDeviceResetTypes
) const
/*++

Routine description:

    This routine returns the support Device Reset Types

--*/
{
    bool fDeviceResetSupported = FALSE;

    if (m_ResetInterface.DeviceReset != NULL)
    {
        *SupportedDeviceResetTypes = m_ResetInterface.SupportedResetTypes;
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
    if (m_EvtNetDeviceReset != NULL)
    {
        m_EvtNetDeviceReset(GetFxObject());
    }
    // This else block calls into the reset interface directly. Mostly applicable to PCIe based
    // devices.
    else if((m_ResetInterface.DeviceReset != NULL) && DeviceResetTypeSupported(ResetType))
    {
        ++m_ResetAttempts;  // Currently this is not used. Will use it later.

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
        status = m_ResetInterface.DeviceReset(m_ResetInterface.Context, ResetType, 0, NULL);

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
    PVOID Context
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
    m_EvtNetDeviceReset = NetDeviceReset;
}

void
NxDevice::GetTriageInfo(
    void
    )
{

    g_NetAdapterCxTriageBlock.NxDeviceAdapterCollectionOffset = FIELD_OFFSET(NxDevice, m_AdapterCollection);

    CFxObject::GetTriageInfo();
    NxAdapter::GetTriageInfo();
    NxAdapterCollection::GetTriageInfo();
}

_Use_decl_annotations_
void
NxDevice::SetMaximumNumberOfWakePatterns(
    NET_ADAPTER_POWER_CAPABILITIES const & PowerCapabilities
    )
{
    m_wakePatternMax = PowerCapabilities.NumTotalWakePatterns;
}

bool
NxDevice::IncreaseWakePatternReference(
    void
    )
{
    if ((ULONG)InterlockedIncrement((LONG *)&m_wakePatternCount) > m_wakePatternMax)
    {
        // Note: m_wakePatternCount may temporarily go over the limit,
        // so this variable should not be used to query how many wake
        // patterns are currently programmed on this device. For that
        // use m_WakeListCount of each NxWake object associated with
        // this device
        InterlockedDecrement((LONG *)&m_wakePatternCount);
        return false;
    }

    return true;
}

void
NxDevice::DecreaseWakePatternReference(
    void
    )
{
    NT_FRE_ASSERT(m_wakePatternCount > 0);
    InterlockedDecrement((LONG *)&m_wakePatternCount);
}
