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
#include <wmistr.h>
#include <smfx.h>
#include <KPtr.h>

#include "NxDeviceStateMachine.h"
#include "DevicePnpPower.hpp"
#include "NxAdapterCollection.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "idle/IdleStateMachine.hpp"
#include "NxUtility.hpp"
#include "NxQueue.hpp"
#include "ExecutionContext.hpp"
#include "netadaptercx_triage.h"

struct NX_PRIVATE_GLOBALS;
class NxDriver;

NTSTATUS
WdfCxDeviceInitAssignPreprocessorRoutines(
    _Inout_ WDFCXDEVICE_INIT * CxDeviceInit
    );

void
SetCxPnpPowerCallbacks(
    _Inout_ WDFCXDEVICE_INIT * CxDeviceInit
);

void
SetCxPowerPolicyCallbacks(
    _Inout_ WDFCXDEVICE_INIT * CxDeviceInit
);

typedef struct _FUNCTION_LEVEL_RESET_PARAMETERS
{
    FUNCTION_LEVEL_DEVICE_RESET_PARAMETERS
        CompletionParameters;

    KEVENT
        Event;

    NTSTATUS
        Status;

} FUNCTION_LEVEL_RESET_PARAMETERS;

typedef union _DeviceFlags
{
#pragma warning(disable:4201)
    struct
    {
        ULONG
            InPowerTransition : 1;

        ULONG
            WmiPowerEventEnabled : 1;

    };
    ULONG Flags;
} DeviceFlags;

class NxDevice
    : public NxDeviceStateMachine<NxDevice>
{

public:

    //
    // The triage block contains the offsets to dynamically allocated class
    // members. Do not add a class member before this. If a class member has
    // to be added or removed, make sure to update NETADAPTERCX_TRIAGE_INFO_OFFSET.
    // Make sure to update this if an inherited member is added/removed as well.
    //
    NETADAPTERCX_GLOBAL_TRIAGE_BLOCK *
        m_netAdapterCxTriageBlock = nullptr;

    DevicePnpPower
        PnpPower;

    IdleStateMachine
        IdleStateMachine;

    LIST_ENTRY
        m_linkage = {};

private:

    WDFDEVICE
        m_device = nullptr;

    NxDriver *
        m_driver = nullptr;

    //
    // Collection of adapters created on top of this device
    //
    NxAdapterCollection
        m_adapterCollection;

    //
    // Device Bus Address, indicating the bus slot this device belongs to
    // This value is used for telemetry and test purpose.
    // This value is defaulted to 0xffffffff
    //
    UINT32
        m_deviceBusAddress = 0xFFFFFFFF;

    DeviceFlags
        m_flags = {};

    USHORT
        m_cGuidToOidMap = 0;

    KPoolPtrNP<NDIS_GUID>
        m_pGuidToOidMap;

    size_t
        m_oidListCount = 0;

    KPoolPtrNP<NDIS_OID>
        m_oidList;

    bool
        m_isWakeInProgress = false;

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

    //
    // Used to collect diagnostics from device driver
    //
    NET_DEVICE_RESET_CAPABILITIES
        m_resetCapabilities = {};

    //
    // Perform noops for duplicated PLDR requests
    //
    bool
        m_pldrRequested = false;

    //
    // Prevent unloading driver after PLDR is requested
    //
    KAutoEvent
        m_pldrComplete;

    //
    // Only valid when client driver is handling
    // EVT_NET_DEVICE_COLLECT_RESET_DIAGNOSTICS
    //
    PKTHREAD
        m_collectResetDiagnosticsThread = nullptr;

    //
    // Copy diagnostics from IHV drivers
    //
    WDFMEMORY
        m_resetDiagnosticsBuffer = WDF_NO_HANDLE;

    NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS
        m_powerPolicyEventCallbacks = {};

    wchar_t const *
        m_pnpInstanceId = nullptr;

    size_t
        m_cbPnpInstanceId = 0;

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

    SyncOperationPassive
        CollectDiagnostics;

    AsyncOperationDispatch
        TriggerPlatformLevelDeviceReset;

    NTSTATUS
    Initialize(
        _In_ WDFDEVICE Device
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    QueryDeviceResetInterface(
        void
    );

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
        _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
        _In_ WDFDEVICE_INIT * DeviceInit
    );

    NTSTATUS
    EnsureInitialized(
        _In_ WDFDEVICE Device
    );

    WDFDEVICE
    GetFxObject(
        void
    ) const;

    NxDriver *
    GetNxDriver(
        void
    ) const;

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
    WmiProcessSingleInstanceDefault(
        _In_ WNODE_SINGLE_INSTANCE * Wnode,
        _In_ NDIS_GUID * Guid,
        _In_ ULONG BufferSize,
        _In_ UCHAR Method,
        _Out_ ULONG * ReturnSize
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    WmiProcessSingleInstanceMagicPacket(
        _In_ WNODE_SINGLE_INSTANCE * Wnode,
        _In_ NDIS_GUID * Guid,
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

    bool
    IsWakeInProgress(
        void
    );

    void
    EnableWakeReasonReporting(
        void
    );

    void
    DisableWakeReasonReporting(
        void
    );

    void
    GetPowerOffloadList(
        _Inout_ NxPowerOffloadList * PowerList
    );

    void
    GetWakeSourceList(
        _Inout_ NxWakeSourceList * PowerList
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

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    SetResetCapabilities(
        NET_DEVICE_RESET_CAPABILITIES const * ResetCapabilities
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    bool
    IsCollectingResetDiagnostics(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    StoreResetDiagnostics(
        _In_ size_t ResetDiagnosticsSize,
        _In_reads_bytes_(ResetDiagnosticsSize) const UINT8 * ResetDiagnosticsBuffer
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    ReportResetDiagnostics(
        _In_ HANDLE ReportHandle,
        _In_ PDBGK_LIVEDUMP_ADDSECONDARYDATA_ROUTINE DbgkLiveDumpAddSecondaryDataRoutine
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    PlatformLevelDeviceResetWithDiagnostics(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    WaitDeviceResetFinishIfRequested(
        void
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

    NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS const &
    GetPowerPolicyEventCallbacks(
        void
    ) const;

    void
    BeginPowerTransition(
        void
    );

    void
    EndPowerTransition(
        void
    );

    void
    UpdatePowerPolicyParameters(
        void
    );

    void
    WritePowerStateChangeEvent(
        _In_ WDF_POWER_DEVICE_STATE TargetState
    ) const;

    _IRQL_requires_max_(PASSIVE_LEVEL)
    NxAdapterCollection &
    GetAdapterCollection(
        void
    );

    NTSTATUS
    PostD0Entry(
        WDF_POWER_DEVICE_STATE PreviousState
    );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxDevice, GetNxDeviceFromHandle);

#ifdef _WIN64
static_assert(FIELD_OFFSET(NxDevice, m_netAdapterCxTriageBlock) == 8);
#else
static_assert(FIELD_OFFSET(NxDevice, m_netAdapterCxTriageBlock) == 4);
#endif
