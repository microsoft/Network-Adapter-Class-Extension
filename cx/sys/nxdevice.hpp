// Copyright (C) Microsoft Corporation. All rights reserved.
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
    };
    ULONG Flags;
} DeviceFlags;

EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmIrpPreprocessRoutine;
EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtWdmPnpPowerIrpPreprocessRoutine;

typedef class NxDevice *PNxDevice;
class NxDevice : public CFxObject<WDFDEVICE,
                                  NxDevice,
                                  GetNxDeviceFromHandle,
                                  false> 
{
private:
    PNxDriver                    m_NxDriver;






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
    // Current Implemenetation requies 1 NxAdapter Object associated with the
    // NxDevice object. 
    //
    PNxAdapter                   m_DefaultNxAdapter;

    //
    // WDM Remove Lock
    //
    IO_REMOVE_LOCK               m_RemoveLock;

    //
    // Device state management via the state machine engine
    //
    SM_ENGINE_CONTEXT           m_SmEngineContext;

    //
    // The device state machine sets this to determine it is ok to return
    // from a PnP/power callback.
    // 
    KEVENT                     m_StateChangeComplete;

    //
    // The device state machine sets this so the device state change callback
    // back (prepare hw, release hw) can return the appropriate status.
    //
    NTSTATUS                   m_StateChangeStatus;








    DeviceFlags                 m_Flags;

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
private:
    NxDevice(
        _In_ PNX_PRIVATE_GLOBALS           NxPrivateGlobals,
        _In_ WDFDEVICE                     Device
        );

public:

    ~NxDevice();



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

    RECORDER_LOG
    GetRecorderLog() { 
        return m_NxDriver->GetRecorderLog();
    }

    VOID
    SetDefaultNxAdapter(
        PNxAdapter NxAdapter
    );

    PNxAdapter
    GetDefaultNxAdapter() {
        return m_DefaultNxAdapter;
    }

    VOID
    SignalStateChange(_In_ NTSTATUS Status) {
        m_StateChangeStatus = Status;
        KeSetEvent(&m_StateChangeComplete, IO_NO_INCREMENT, FALSE);
    }
    static
    NTSTATUS
    _EvtCxDevicePrePrepareHardware(
        _In_ WDFDEVICE    Device,
        _In_ WDFCMRESLIST ResourcesRaw,
        _In_ WDFCMRESLIST ResourcesTranslated
        );

    static
    NTSTATUS
    _EvtCxDevicePostPrepareHardware(
        _In_ WDFDEVICE    Device,
        _In_ WDFCMRESLIST ResourcesRaw,
        _In_ WDFCMRESLIST ResourcesTranslated
        );

    static
    VOID
    _EvtCxDevicePrePrepareHardwareFailedCleanup(
        _In_ WDFDEVICE    Device,
        _In_ WDFCMRESLIST ResourcesRaw,
        _In_ WDFCMRESLIST ResourcesTranslated
        );

    static
    NTSTATUS
    _EvtCxDevicePreReleaseHardware(
        _In_ WDFDEVICE    Device,
        _In_ WDFCMRESLIST ResourcesTranslated
        );

    static
    NTSTATUS
    _EvtCxDevicePostReleaseHardware(
        _In_ WDFDEVICE    Device,
        _In_ WDFCMRESLIST ResourcesTranslated
        );

    NTSTATUS
    AdapterAdd(
        _In_  NDIS_HANDLE  MiniportAdapterContext,
        _Out_ NDIS_HANDLE* NdisAdapterHandle
        );

    VOID
    AdapterInitialized(
        _In_ NTSTATUS InitializationStatus
        );

    VOID
    AdapterHalting(
        VOID
        );

    NTSTATUS 
    InitializeWdfDevicePowerReferences(
        _In_ WDFDEVICE Device
        );

    static
    NTSTATUS
    _EvtCxDevicePostD0Entry(
        _In_ WDFDEVICE              Device,
        _In_ WDF_POWER_DEVICE_STATE PreviousState
        );

    static
    NTSTATUS
    _EvtCxDevicePreD0Exit(
        _In_ WDFDEVICE              Device,
        _In_ WDF_POWER_DEVICE_STATE TargetState
        );

    static
    NTSTATUS
    _EvtCxDevicePostSelfManagedIoInit(
        _In_  WDFDEVICE Device
        );

    static
    NTSTATUS
    _EvtCxDevicePostSelfManagedIoRestart(
        _In_  WDFDEVICE Device
        );

    static
    NTSTATUS
    _EvtCxDevicePreSelfManagedIoSuspend(
        _In_  WDFDEVICE Device
        );

    static
    VOID 
    _EvtCxDevicePreSurpriseRemoval(
        _In_  WDFDEVICE Device
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
    )
    {
        return (m_LastReportedDxAction != NdisWdfActionPowerNone);
    }

    GENERATED_DECLARATIONS_FOR_NXDEVICE_STATE_ENTRY_FUNCTIONS();

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

SM_ENGINE_ETW_WRITE_STATE_TRANSITION NxDeviceStateTransitionTracing;
