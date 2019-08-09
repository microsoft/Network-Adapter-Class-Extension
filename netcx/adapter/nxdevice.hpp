// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef _KERNEL_MODE

#include <windows.h>

#include <wdf.h>
#include <wdfcx.h>
#include <wdfcxbase.h>

#include <WppRecorder.h>

#include "NdisUm.h"

#endif // _KERNEL_MODE

#include <KWaitEvent.h>
#include <mx.h>
#include <smfx.h>
#include <KPtr.h>

#include "NxDeviceStateMachine.h"
#include "NxAdapterCollection.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "NxUtility.hpp"
#include "netadaptercx_triage.h"

#if (FX_CORE_MODE==FX_CORE_KERNEL_MODE)
using CxRemoveLock = IO_REMOVE_LOCK;
#else
using CxRemoveLock = WUDF_IO_REMOVE_LOCK;
#endif // _FX_CORE_MODE

struct NX_PRIVATE_GLOBALS;
class NxDriver;

NTSTATUS
WdfCxDeviceInitAssignPreprocessorRoutines(
    _Inout_ WDFCXDEVICE_INIT *CxDeviceInit
    );

void
SetCxPnpPowerCallbacks(
    _Inout_ WDFCXDEVICE_INIT * CxDeviceInit
);

typedef struct _FUNCTION_LEVEL_RESET_PARAMETERS {
    FUNCTION_LEVEL_DEVICE_RESET_PARAMETERS CompletionParameters;
    KEVENT Event;
    NTSTATUS Status;
} FUNCTION_LEVEL_RESET_PARAMETERS, *PFUNCTION_LEVEL_RESET_PARAMETERS;

typedef union _DeviceFlags
{
#pragma warning(disable:4201)
    struct
    {
        //
        // Indicates the device is the stack's power policy owner
        //
        ULONG IsPowerPolicyOwner : 1;

        //
        // Tracks if the devnode was surprise removed.
        //
        ULONG SurpriseRemoved : 1;

    };
    ULONG Flags;
} DeviceFlags;

class NxDevice : public NxDeviceStateMachine<NxDevice>
{
    using PowerIrpParameters = decltype(IO_STACK_LOCATION::Parameters.Power);

private:

    //
    // The triage block contains the offsets to dynamically allocated class
    // members. Do not add a class member before this. If a class member has
    // to be added, make sure to update NETADAPTERCX_TRIAGE_INFO_OFFSET.
    //
    NETADAPTERCX_GLOBAL_TRIAGE_BLOCK *
        m_netAdapterCxTriageBlock = nullptr;

    WDFDEVICE
        m_device = nullptr;

    NxDriver *
        m_nxDriver = nullptr;

    void *
        m_plugPlayNotificationHandle = nullptr;

    //
    // Collection of adapters created on top of this device
    //
    NxAdapterCollection
        m_adapterCollection;

    //
    // Number of adapters that had NdisMiniportInitializeEx
    // callback completed
    //
    _Interlocked_ ULONG
        m_ndisInitializeCount = 0;

    CxRemoveLock
        m_removeLock = {};

    //
    // Device Bus Address, indicating the bus slot this device belongs to
    // This value is used for telemetry and test purpose.
    // This value is defaulted to 0xffffffff
    //
    UINT32
        m_deviceBusAddress = 0xFFFFFFFF;

    DeviceFlags
        m_flags = {};

    DeviceState
        m_state = DeviceState::Initialized;

    USHORT
        m_cGuidToOidMap = 0;

    KPoolPtrNP<NDIS_GUID>
        m_pGuidToOidMap;

    size_t
        m_oidListCount = 0;

    KPoolPtrNP<NDIS_OID>
        m_oidList;

#ifdef _KERNEL_MODE
    //
    // WdfDeviceGetSystemPowerAction relies on system power information stored
    // in the D-IRP. The power manager marks D-IRPs with a system sleep power
    // action as long as a system power broadcast is in progress. e.g. if
    // device goes into S0-Idle while system power broadcast is still in
    // progress then the API's returned action cannot be trusted. So we track
    // Sx IRPs ourselves to ensure the correct value is reported.
    //
    POWER_ACTION
        m_systemPowerAction = PowerActionNone;

    SYSTEM_POWER_STATE
        m_targetSystemPowerState = PowerSystemUnspecified;

    DEVICE_POWER_STATE
        m_targetDevicePowerState = PowerDeviceUnspecified;

#endif // _KERNEL_MODE

    //
    // Track WdfDeviceStopIdle failures so as to avoid imbalance of stop idle
    // and resume idle. This relieves the caller (NDIS) from needing to keep
    // track of failures.
    //
    _Interlocked_ LONG
        m_powerRefFailureCount = 0;

    //
    // Used to track the device reset interface
    //
    DEVICE_RESET_INTERFACE_STANDARD
        m_resetInterface = {};

    //
    // Track whether we are the failing device that will be used
    // to provide hint to ACPI by returning STATUS_DEVICE_HUNG
    //
    bool
        m_failingDeviceRequestingReset = false;

    //
    //  Used to track the number of device reset attempts requested on this device
    //
    ULONG
        m_resetAttempts = 0;

    PFN_NET_DEVICE_RESET
        m_evtNetDeviceReset = nullptr;

    NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS
        m_powerPolicyEventCallbacks = {};

    // Declare the base device state machine class as friend so that we
    // can declare the state machine operations as private
    friend class
        NxDeviceStateMachine<NxDevice>;

    //
    // State machine events
    //
    EvtLogTransitionFunc
        EvtLogTransition;

    EvtLogEventEnqueueFunc
        EvtLogEventEnqueue;

    EvtLogMachineExceptionFunc
        EvtLogMachineException;

    EvtMachineDestroyedFunc
        EvtMachineDestroyed;

    //
    // State machine operations
    //
    SyncOperationDispatch
        ReleasingIsSurpriseRemoved;

    SyncOperationPassive
        ReleasingReportPreReleaseToNdis;

    SyncOperationDispatch
        ReleasingReportSurpriseRemoveToNdis;

    SyncOperationDispatch
        ReleasingReportDeviceAddFailureToNdis;

    SyncOperationDispatch
        RemovedReportRemoveToNdis;

    SyncOperationDispatch
        ReleasingReportPostReleaseToNdis;

    SyncOperationDispatch
        CheckPowerPolicyOwnership;

    SyncOperationDispatch
        InitializeSelfManagedIo;

    SyncOperationDispatch
        ReinitializeSelfManagedIo;

    SyncOperationDispatch
        SuspendSelfManagedIo;

    SyncOperationDispatch
        RestartSelfManagedIo;

    SyncOperationDispatch
        PrepareForRebalance;

    SyncOperationPassive
        Started;

    SyncOperationDispatch
        PrepareHardware;

    SyncOperationDispatch
        PrepareHardwareFailedCleanup;

    SyncOperationDispatch
        SelfManagedIoCleanup;

    SyncOperationDispatch
        AreAllAdaptersHalted;

    AsyncOperationDispatch
        RefreshAdapterList;

    NTSTATUS
    Initialize(
        _In_ WDFDEVICE Device
    );

    bool
    GetPowerPolicyOwnership(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    QueryDeviceResetInterface(
        void
    );

public:

    //
    // Events used to synchronize our state machine with WDF's own
    // state machine
    //
    AsyncResult
        CxPrePrepareHardwareHandled;

    AsyncResult
        CxPostSelfManagedIoInitHandled;

    KAutoEvent
        CxPrePrepareHardwareFailedCleanupHandled;

    KAutoEvent
        CxPostSelfManagedIoRestartHandled;

    KAutoEvent
        CxPreSelfManagedIoSuspendHandled;

    KAutoEvent
        CxPostSelfManagedIoCleanupHandled;

    KAutoEvent
        CxPreReleaseHardwareHandled;

    KAutoEvent
        CxPostReleaseHardwareHandled;

    KAutoEvent
        WdfDeviceObjectCleanupHandled;

public:

    NxDevice(
        _In_ NX_PRIVATE_GLOBALS * NxPrivateGlobals
    );

    UINT32
    GetDeviceBusAddress(
        void
    ) const;

    static
    NTSTATUS
    _Create(
        _In_  NX_PRIVATE_GLOBALS * PrivateGlobals,
        _In_  WDFDEVICE_INIT * DeviceInit
    );

    NTSTATUS
    EnsureInitialized(
        _In_ WDFDEVICE Device
    );

    WDFDEVICE
    GetFxObject(
        void
    ) const;

    void
    ReleaseRemoveLock(
        _In_ IRP * Irp
    );

    void
    AdapterInitialized(
        void
    );

    void
    AdapterHalted(
        void
    );

    void
    SurpriseRemoved(
        void
    );

    RECORDER_LOG
    GetRecorderLog(
        void
    );

    NxDriver *
    GetNxDriver(
        void
    ) const;

    void
    AdapterCreated(
        _In_ NxAdapter * Adapter
    );

    void
    AdapterDestroyed(
        _In_ NxAdapter * Adapter
    );

    void
    StartComplete(
        void
    );

    NTSTATUS
    WdmCreateIrpPreProcess(
        _Inout_ IRP * Irp,
        _In_ WDFCONTEXT DispatchContext
    );

    NTSTATUS
    WdmCloseIrpPreProcess(
        _Inout_ IRP * Irp,
        _In_ WDFCONTEXT DispatchContext
    );

    NTSTATUS
    WdmIoIrpPreProcess(
        _Inout_ IRP * Irp,
        _In_ WDFCONTEXT DispatchContext
    );

    NTSTATUS
    WdmSystemControlIrpPreProcess(
        _Inout_ IRP * Irp,
        _In_ WDFCONTEXT DispatchContext
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiGetGuid(
        _In_ GUID const & Guid,
        _Out_ NDIS_GUID ** NdisGuid
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiGetEventGuid(
        _In_ NTSTATUS GuidStatus,
        _Out_ NDIS_GUID ** Guid
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiIrpDispatch(
        _Inout_ IRP * Irp,
        _In_ WDFCONTEXT DispatchContext
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiRegister(
        _In_ ULONG_PTR RegistrationType,
        _In_ WMIREGINFO * WmiRegInfo,
        _In_ ULONG WmiRegInfoSize,
        _In_ bool ShouldReferenceDriver,
        _Out_ ULONG * ReturnSize
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiQueryAllData(
        _In_ GUID * Guid,
        _In_ WNODE_ALL_DATA * Wnode,
        _In_ ULONG BufferSize,
        _Out_ ULONG * ReturnSize
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiWnodeAllDataMerge(
        _Inout_ WNODE_ALL_DATA * AllData,
        _In_ WNODE_ALL_DATA * SingleData,
        _In_ ULONG BufferSize,
        _In_ USHORT MiniportCount,
        _Out_ ULONG * ReturnSize
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiProcessSingleInstance(
        _In_ WNODE_SINGLE_INSTANCE * Wnode,
        _In_ ULONG BufferSize,
        _In_ UCHAR Method,
        _Out_ ULONG * ReturnSize
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiEnableEvents(
        _In_ GUID const & Guid
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiDisableEvents(
        _In_ GUID const & Guid
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiExecuteMethod(
        _In_ WNODE_METHOD_ITEM * Wnode,
        _In_ ULONG BufferSize,
        _Out_ ULONG * PReturnSize
    );

    NTSTATUS
    WdmPnPIrpPreProcess(
        _Inout_ IRP * Irp
    );

    bool
    IsDeviceInPowerTransition(
        void
    );

    NTSTATUS
    PowerReference(
        _In_ bool WaitForD0,
        _In_ void * Tag
    );

    void
    PowerDereference(
        _In_ void * Tag
    );

    bool
    IsPowerPolicyOwner(
        void
    ) const;

    void
    PreSetPowerIrp(
        _In_ PowerIrpParameters const & PowerParameters
    );

    void
    PostSetPowerIrp(
        _In_ PowerIrpParameters const & PowerParameters
    );

    void
    GetPowerList(
        _Inout_ NxPowerList * PowerList
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetFailingDeviceRequestingResetFlag(
        void
    );

    bool
    DeviceResetTypeSupported(
        _In_ DEVICE_RESET_TYPE ResetType
    ) const;

    bool
    GetSupportedDeviceResetType(
        _Inout_ ULONG * SupportedDeviceResetTypes
    ) const;

    NTSTATUS
    DispatchDeviceReset(
        _In_ DEVICE_RESET_TYPE ResetType
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetEvtDeviceResetCallback(
        PFN_NET_DEVICE_RESET NetDeviceReset
    );

    void
    SetPowerPolicyEventCallbacks(
        _In_ NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS const * Callbacks
    );

    static
    void
    GetTriageInfo(
        void
    );

    NTSTATUS
    AssignSupportedOidList(
        _In_reads_(SupportedOidsCount) NDIS_OID const * SupportedOids,
        _In_ size_t SupportedOidsCount
    );

    NDIS_OID const *
    GetOidList(
        void
    ) const;

    size_t
    GetOidListCount(
        void
    ) const;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxDevice, GetNxDeviceFromHandle);
