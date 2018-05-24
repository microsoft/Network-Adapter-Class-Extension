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

#include <FxObjectBase.hpp>
#include <KWaitEvent.h>
#include <mx.h>
#include <smfx.h>

#include "NxDeviceStateMachine.h"
#include "NxAdapterCollection.hpp"
#include "NxUtility.hpp"

#if (FX_CORE_MODE==FX_CORE_KERNEL_MODE)
using CxRemoveLock = IO_REMOVE_LOCK;
#else
using CxRemoveLock = WUDF_IO_REMOVE_LOCK;
#endif // _FX_CORE_MODE

struct NX_PRIVATE_GLOBALS;

class NxDevice;
class NxDriver;

void
SetCxPnpPowerCallbacks(
    _Inout_ PWDFCXDEVICE_INIT CxDeviceInit
    );

FORCEINLINE
NxDevice *
GetNxDeviceFromHandle(
    _In_ WDFDEVICE Device
    );

ULONG const MAX_DEVICE_RESET_ATTEMPTS = 5;

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
        // Used to identify if device is being powered down. Set while
        // pre-processing Dx IRP and cleared after Dx transition is reported to
        // NDIS. Also cleared during Dx IRP completion, in case failure occurs
        // prior to reporting transition to NDIS.
        //
        ULONG InDxPowerTransition : 1;

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

class NxDevice : public CFxObject<WDFDEVICE,
                                  NxDevice,
                                  GetNxDeviceFromHandle,
                                  false>,
                 public NxDeviceStateMachine<NxDevice>
{
private:
    NxDriver *                   m_NxDriver;

    void * m_PlugPlayNotificationHandle;

    //
    // Collection of adapters created on top of this device
    //
    NxAdapterCollection          m_AdapterCollection;

    //
    // Number of adapters that had NdisMiniportInitializeEx
    // callback completed
    //
    _Interlocked_ ULONG          m_NdisInitializeCount;

    //
    // WDM Remove Lock
    //
    CxRemoveLock                 m_RemoveLock;

    //
    // Device Bus Address, indicating the bus slot this device belongs to
    // This value is used for telemetry and test purpose.
    // This value is defaulted to 0xffffffff
    //
    UINT32                      m_DeviceBusAddress = 0xFFFFFFFF;

    DeviceFlags                 m_Flags;

    DeviceState m_State = DeviceState::Initialized;

#ifdef _KERNEL_MODE
    //
    // WdfDeviceGetSystemPowerAction relies on system power information stored
    // in the D-IRP. The power manager marks D-IRPs with a system sleep power
    // action as long as a system power broadcast is in progress. e.g. if
    // device goes into S0-Idle while system power broadcast is still in
    // progress then the API's returned action cannot be trusted. So we track
    // Sx IRPs ourselves to ensure the correct value is reported.
    //
    POWER_ACTION                m_SystemPowerAction;

    NdisWdfPnpPowerAction       m_LastReportedDxAction;
#endif // _KERNEL_MODE

    //
    // Track WdfDeviceStopIdle failures so as to avoid imbalance of stop idle
    // and resume idle. This relieves the caller (NDIS) from needing to keep
    // track of failures.
    //
    _Interlocked_ LONG m_PowerRefFailureCount = 0;

    //
    // Used to track the device reset interface
    //
    DEVICE_RESET_INTERFACE_STANDARD m_ResetInterface = {};

    //
    // BOOLEAN to track whether we are the failing device that will be used
    // to provide hint to ACPI by returning STATUS_DEVICE_HUNG
    //
    BOOLEAN                     m_FailingDeviceRequestingReset = FALSE;

    //
    //  Used to track the number of device reset attempts requested on this device
    //
    ULONG                       m_ResetAttempts = 0;


    PFN_NET_DEVICE_RESET        m_EvtNetDeviceReset = nullptr;

private:
    friend class NxDeviceStateMachine<NxDevice>;

    NxDevice(
        _In_ NX_PRIVATE_GLOBALS *          NxPrivateGlobals,
        _In_ WDFDEVICE                     Device
        );

    bool
    GetPowerPolicyOwnership(
        void
        );

    void
    QueryDeviceResetInterface(
        void
        );

    // State machine events
    EvtLogTransitionFunc EvtLogTransition;
    EvtLogEventEnqueueFunc EvtLogEventEnqueue;
    EvtLogMachineExceptionFunc EvtLogMachineException;
    EvtMachineDestroyedFunc EvtMachineDestroyed;

    // State machine operations
    SyncOperationDispatch ReleasingIsSurpriseRemoved;
    SyncOperationPassive ReleasingReportPreReleaseToNdis;
    SyncOperationDispatch ReleasingReportSurpriseRemoveToNdis;
    SyncOperationDispatch ReleasingReportDeviceAddFailureToNdis;
    SyncOperationDispatch RemovedReportRemoveToNdis;
    SyncOperationDispatch ReleasingReportPostReleaseToNdis;
    SyncOperationDispatch CheckPowerPolicyOwnership;
    SyncOperationDispatch InitializeSelfManagedIo;
    SyncOperationDispatch ReinitializeSelfManagedIo;
    SyncOperationDispatch SuspendSelfManagedIo;
    SyncOperationDispatch RestartSelfManagedIo;
    SyncOperationDispatch PrepareForRebalance;
    SyncOperationPassive Started;
    SyncOperationDispatch PrepareHardware;
    SyncOperationDispatch PrepareHardwareFailedCleanup;
    SyncOperationDispatch SelfManagedIoCleanup;
    SyncOperationDispatch AreAllAdaptersHalted;

    AsyncOperationDispatch RefreshAdapterList;

public:

    AsyncResult CxPrePrepareHardwareHandled;
    AsyncResult CxPrePrepareHardwareFailedCleanupHandled;
    AsyncResult CxPostSelfManagedIoInitHandled;
    AsyncResult CxPreSelfManagedIoSuspendHandled;

    KAutoEvent CxPostSelfManagedIoRestartHandled;
    KAutoEvent CxPostSelfManagedIoCleanupHandled;
    KAutoEvent CxPreReleaseHardwareHandled;
    KAutoEvent CxPostReleaseHardwareHandled;
    KAutoEvent WdfDeviceObjectCleanupHandled;

public:

    ~NxDevice();

    UINT32 GetDeviceBusAddress() { return m_DeviceBusAddress; }

    static
    NTSTATUS
    _Create(
        _In_  NX_PRIVATE_GLOBALS *          PrivateGlobals,
        _In_  WDFDEVICE                     Device,
        _Out_ NxDevice **                   NxDeviceParam
        );

    static
    VOID
    _EvtCleanup(
        _In_  WDFOBJECT NetAdatper
        );

    NTSTATUS
    Init(
        VOID
        );

    void
    ReleaseRemoveLock(
        _In_ IRP *Irp
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
        VOID
    );

    NxDriver *
    GetNxDriver(
        void
        ) const;

    NxAdapter *
    GetDefaultNxAdapter(
        VOID
        ) const;

    NTSTATUS
    AdapterAdd(
        _In_  NxAdapter *Adapter
        );

    bool
    RemoveAdapter(
        _In_ NxAdapter *Adapter
        );

    bool
    IsStarted(
        void
        ) const;

    VOID
    StartComplete(
        VOID
        );

    NTSTATUS
    NxDevice::WdmCreateIrpPreProcess(
        _Inout_ PIRP       Irp,
        _In_    WDFCONTEXT DispatchContext
        );

    NTSTATUS
    NxDevice::WdmCloseIrpPreProcess(
        _Inout_ PIRP       Irp,
        _In_    WDFCONTEXT DispatchContext
        );

    NTSTATUS
    NxDevice::WdmIoIrpPreProcess(
        _Inout_ PIRP       Irp,
        _In_    WDFCONTEXT DispatchContext
        );

    NTSTATUS
    NxDevice::WdmSystemControlIrpPreProcess(
        _Inout_ PIRP       Irp,
        _In_    WDFCONTEXT DispatchContext
        );

    NTSTATUS
        NxDevice::WdmPnPIrpPreProcess(
        _Inout_ PIRP       Irp
        );

    BOOLEAN
    IsDeviceInPowerTransition(
        VOID
    );

    NTSTATUS
    PowerReference(
        _In_ bool WaitForD0,
        _In_ void *Tag
        );

    void
    PowerDereference(
        _In_ void *Tag
        );

    bool
    IsPowerPolicyOwner(
        void
        ) const;

    void
    SetSystemPowerAction(
        _In_ POWER_ACTION PowerAction
        );

    void
    SetInDxPowerTranstion(
        _In_ bool InDxPowerTransition
        );

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
        _Inout_ PULONG SupportedDeviceResetTypes
        ) const;

    NTSTATUS
    DispatchDeviceReset(
        _In_ DEVICE_RESET_TYPE ResetType
        );

    void
    SetEvtDeviceResetCallback(
        PFN_NET_DEVICE_RESET NetDeviceReset
        );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxDevice, _GetNxDeviceFromHandle);

FORCEINLINE
NxDevice *
GetNxDeviceFromHandle(
    _In_ WDFDEVICE     Device
    )
/*++
Routine Description:

    This routine is just a wrapper around the _GetNxDeviceFromHandle function.
    To be able to define a the NxDevice class above, we need a forward declaration of the
    accessor function. Since _GetNxDeviceFromHandle is defined by Wdf, we dont want to
    assume a prototype of that function for the foward declaration.

--*/

{
    return _GetNxDeviceFromHandle(Device);
}

NTSTATUS
WdfCxDeviceInitAssignPreprocessorRoutines(
    _Inout_ WDFCXDEVICE_INIT *CxDeviceInit
    );
