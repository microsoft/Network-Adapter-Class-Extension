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

#include "FxObjectBase.hpp"
#include "NxDeviceStateMachine.h"
#include "NxForward.hpp"

#if (FX_CORE_MODE==FX_CORE_KERNEL_MODE)
using CxRemoveLock = IO_REMOVE_LOCK;
#else
using CxRemoveLock = WUDF_IO_REMOVE_LOCK;
#endif // _FX_CORE_MODE

PNxDevice
FORCEINLINE
GetNxDeviceFromHandle(
    _In_ WDFDEVICE Device
    );

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
        // Used to identify if at least one NETADAPTER created during EvtDriverDeviceAdd
        // failed NDIS initialization
        //
        ULONG AnyAdapterFailedNdisInitialize : 1;

        //
        // Indicates the device is in the device started state. This is the only
        // state where drivers can call NetAdapterCreate other than from their
        // EvtDriverDeviceAdd callback
        //
        ULONG DeviceStarted : 1;

        //
        // Indicates the device has its self managed IO started
        //
        ULONG SelfManagedIoStarted;
    };
    ULONG Flags;
} DeviceFlags;

EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmIrpPreprocessRoutine;
EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmPnpPowerIrpPreprocessRoutine;

typedef class NxDevice *PNxDevice;
class NxDevice : public CFxObject<WDFDEVICE,
                                  NxDevice,
                                  GetNxDeviceFromHandle,
                                  false>,
                 private NxDeviceStateMachine<NxDevice>
{
private:
    PNxDriver                    m_NxDriver;

    //
    // NOTE: Once we implement a formal state machine for NetAdapterCx, the following
    // state variables may go way.
    //

    //
    // A boolean that tracks if the devnode was surprise removed.
    //
    BOOLEAN                      m_SurpriseRemoved;

    //
    // A boolean indicating that we have acquired a power ref
    // to keep the ndis stack in D0.
    //
    BOOLEAN                      m_PowerReferenceAcquired;

    //
    // Collection of adapters created on top of this device
    //
    NxAdapterCollection          m_AdapterCollection;

    //
    // Number of adapters that had NdisMiniportInitializeEx
    // callback completed
    //
    ULONG                        m_NdisInitializeCount;

    //
    // WDM Remove Lock
    //
    CxRemoveLock                 m_RemoveLock;

    //
    // The device state machine sets this to determine it is ok to return
    // from a PnP/power callback.
    //
    KAutoEvent                 m_StateChangeComplete;

    //
    // The device state machine sets this so the device state change callback
    // back (prepare hw, release hw) can return the appropriate status.
    //
    NTSTATUS                   m_StateChangeStatus;

    //
    // Device Bus Address, indicating the bus slot this device belongs to
    // This value is used for telemetry and test purpose.
    // This value is defaulted to 0xffffffff
    //
    UINT32                      m_DeviceBusAddress = 0xFFFFFFFF;

    DeviceFlags                 m_Flags;

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

private:
    friend class NxDeviceStateMachine<NxDevice>;

    NxDevice(
        _In_ PNX_PRIVATE_GLOBALS           NxPrivateGlobals,
        _In_ WDFDEVICE                     Device
        );

    // State machine events
    EvtLogTransitionFunc EvtLogTransition;
    EvtLogEventEnqueueFunc EvtLogEventEnqueue;
    EvtLogMachineExceptionFunc EvtLogMachineException;
    EvtMachineDestroyedFunc EvtMachineDestroyed;

    // State machine operations
    AsyncOperationDispatch PrepareForStart;
    SyncOperationDispatch StartingReportStartToNdis;
    SyncOperationDispatch ReleasingIsSurpriseRemoved;
    SyncOperationPassive ReleasingReportPreReleaseToNdis;
    SyncOperationDispatch ReleasingReportSurpriseRemoveToNdis;
    AsyncOperationDispatch StartProcessingComplete;
    AsyncOperationDispatch StartProcessingCompleteFailed;
    AsyncOperationDispatch ReleasingReleaseClient;
    AsyncOperationDispatch ReleasingReportPostReleaseToNdis;
    AsyncOperationDispatch StoppedPrepareForStart;
    AsyncOperationDispatch RemovedReportRemoveToNdis;
    AsyncOperationDispatch ReleasingReportDeviceAddFailureToNdis;

public:

    ~NxDevice();

    UINT32 GetDeviceBusAddress() { return m_DeviceBusAddress; }

    static
    NTSTATUS
    _Create(
        _In_  PNX_PRIVATE_GLOBALS           PrivateGlobals,
        _In_  WDFDEVICE                     Device,
        _Out_ PNxDevice*                    NxDeviceParam
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

    VOID
    SignalStateChange(
        _In_ NTSTATUS Status
    );

    static EVT_WDF_DEVICE_PREPARE_HARDWARE _EvtCxDevicePrePrepareHardware;

    static EVT_WDF_DEVICE_PREPARE_HARDWARE _EvtCxDevicePostPrepareHardware;

    static EVT_WDFCX_DEVICE_PRE_PREPARE_HARDWARE_FAILED_CLEANUP _EvtCxDevicePrePrepareHardwareFailedCleanup;

    static EVT_WDF_DEVICE_RELEASE_HARDWARE _EvtCxDevicePreReleaseHardware;

    static EVT_WDF_DEVICE_RELEASE_HARDWARE _EvtCxDevicePostReleaseHardware;

    NTSTATUS
    AdapterAdd(
        _In_  NxAdapter *Adapter
        );

    bool
    RemoveAdapter(
        _In_ NxAdapter *Adapter
        );

    VOID
    AdapterInitialized(
        _In_ NTSTATUS InitializationStatus
        );

    VOID
    AdapterHalting(
        VOID
        );

    bool
    IsStarted(
        void
        ) const;

    bool
    IsSelfManagedIoStarted(
        void
        ) const;

    NTSTATUS
    InitializeWdfDevicePowerReferences(
        _In_ WDFDEVICE Device
        );

    static EVT_WDF_DEVICE_D0_ENTRY _EvtCxDevicePostD0Entry;

    static EVT_WDF_DEVICE_D0_EXIT _EvtCxDevicePreD0Exit;

    static EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT _EvtCxDevicePostSelfManagedIoInit;

    static EVT_WDF_DEVICE_SELF_MANAGED_IO_RESTART _EvtCxDevicePostSelfManagedIoRestart;

    static EVT_WDF_DEVICE_SELF_MANAGED_IO_SUSPEND _EvtCxDevicePreSelfManagedIoSuspend;

    static EVT_WDF_DEVICE_SURPRISE_REMOVAL _EvtCxDevicePreSurpriseRemoval;

    NTSTATUS
    EvtPostD0Entry(
        _In_ WDF_POWER_DEVICE_STATE PreviousState
        );

    VOID
    InitializeSelfManagedIo(
        VOID
        );

    VOID
    RestartSelfManagedIo(
        VOID
        );

    VOID
    SuspendSelfManagedIo(
        VOID
        );

    VOID
    StartComplete(
        VOID
        );

    static
    NTSTATUS
    _WdmIrpCompleteFromNdis(
        PDEVICE_OBJECT DeviceObject,
        PIRP           Irp,
        PVOID          Context
        );

    static
    NTSTATUS
    _WdmIrpCompleteSetPower(
        PDEVICE_OBJECT DeviceObject,
        PIRP           Irp,
        PVOID          Context
        );

    static
    NTSTATUS
    _WdmIrpCompleteCreate(
        DEVICE_OBJECT *DeviceObject,
        IRP *Irp,
        VOID *Context
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

    static
    NTSTATUS
    _EvtWdfCxDeviceWdmIrpPreProcess(
        _In_    WDFDEVICE  Device,
        _Inout_ PIRP       Irp,
        _In_    WDFCONTEXT DispatchContext
        );

    static
    NTSTATUS
    _EvtWdfCxDeviceWdmPnpPowerIrpPreProcess(
        _In_    WDFDEVICE  Device,
        _Inout_ PIRP       Irp,
        _In_    WDFCONTEXT DispatchContext
        );

    NTSTATUS
    WaitForStateChangeResult(
        VOID
        );

    NTSTATUS
    NotifyNdisDevicePowerDown(
        VOID
    );

    BOOLEAN
    IsDeviceInPowerTransition(
        VOID
    );

};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxDevice, _GetNxDeviceFromHandle);

PNxDevice
FORCEINLINE
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
