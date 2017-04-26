/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxDriver.cpp

Abstract:

    This is the main NetAdapterCx driver framework.





Environment:

    kernel mode only

Revision History:

--*/

#include "Nx.hpp"

// Tracing support
extern "C" {
#include "NxDriver.tmh"
}

NxDriver::NxDriver(
    _In_ WDFDRIVER                Driver,
    _In_ PNX_PRIVATE_GLOBALS      NxPrivateGlobals
    ) : CFxObject(Driver), 
    m_Driver(Driver)
/*++
Routine Description: 
 
    Constructor for the NxDriver Class.
    NxDriver is an object created on top of the context of the client's
    WDFDRIVER object. 
 
Arguments: 
    Driver - The WDFDRIVER object of the client.
    NxPrivateGlobals - Clients Globals. 
 
--*/
{
    FuncEntry(FLAG_DRIVER);

    RECORDER_LOG_CREATE_PARAMS  recorderLogCreateParams;
    NTSTATUS                    status;
    RECORDER_LOG                recorderLog;
   
    UNREFERENCED_PARAMETER(NxPrivateGlobals);

    //
    // Create a WPP Log buffer for the client. 
    //

    RECORDER_LOG_CREATE_PARAMS_INIT(&recorderLogCreateParams, NULL);

    recorderLogCreateParams.TotalBufferSize = DEFAULT_WPP_TOTAL_BUFFER_SIZE;
    recorderLogCreateParams.ErrorPartitionSize = DEFAULT_WPP_ERROR_PARTITION_SIZE;
















    RtlStringCchPrintfA(recorderLogCreateParams.LogIdentifier,
                        sizeof(recorderLogCreateParams.LogIdentifier),
                        "NxClient");

    status = WppRecorderLogCreate(&recorderLogCreateParams, &recorderLog);
    if (!NT_SUCCESS(status)) {

        LogInitMsg(TRACE_LEVEL_WARNING, "Could Not allocate Recorder Log for NetAdapterCxClient : <FILL HERE> ");
        recorderLog = NULL;

        //
        // Non fatal failure
        //

        status = STATUS_SUCCESS;
    }

    m_RecorderLog = recorderLog;
    FuncExit(FLAG_DRIVER);

}

NxDriver::~NxDriver()
/*++
Routine Description: 
 
    D'tor for the NxDriver object.
 
--*/
{
    FuncEntry(FLAG_DRIVER);
    if (m_RecorderLog) {
        //
        // Delete the log buffer created in the constructor.
        //
        WppRecorderLogDelete(m_RecorderLog);
        m_RecorderLog = NULL;
    }
    FuncExit(FLAG_DRIVER);
}

NTSTATUS
NxDriver::_CreateIfNeeded(
    _In_ WDFDRIVER           Driver,
    _In_ PNX_PRIVATE_GLOBALS NxPrivateGlobals
    )
{
    FuncEntry(FLAG_DRIVER);

    WDF_OBJECT_ATTRIBUTES                   attributes;
    PNxDriver                               nxDriver;
    PVOID                                   nxDriverAsContext;
    NTSTATUS                                status;

    if (NxPrivateGlobals->NxDriver != NULL) {
        FuncExit(FLAG_DRIVER);
        return STATUS_SUCCESS;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxDriver);
    
    //
    // Callin this ensures that our d'tor will be called on destory.
    //
    NxDriver::_SetObjectAttributes(&attributes);

    //
    // Allocate a context on the client WDFDRIVER object and create a
    // NxDriver object on it using an in-placement new call.
    //
    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxDriver::_EvtWdfCleanup;

    status = WdfObjectAllocateContext(Driver,
                                      &attributes,
                                      &nxDriverAsContext);
    
    if (!NT_SUCCESS(status)) {
        LogInitMsg(TRACE_LEVEL_ERROR, "WdfObjectAllocateContext Failed %!STATUS!", status);
        FuncExit(FLAG_DRIVER);
        return status;
    }
    
    nxDriver = new (nxDriverAsContext) NxDriver(Driver, NxPrivateGlobals);

    __analysis_assume(nxDriver != NULL);

    NT_ASSERT(nxDriver);
    
    NxPrivateGlobals->NxDriver = nxDriver;
        
    FuncExit(FLAG_DRIVER);
    return status;
    
}

NTSTATUS
NxDriver::_CreateAndRegisterIfNeeded(
    _In_ WDFDRIVER                      Driver,
    _In_ NET_ADAPTER_DRIVER_TYPE        DriverType,
    _In_ PNX_PRIVATE_GLOBALS            NxPrivateGlobals
    )
/*++
Routine Description: 
    This routine is called when a new instance of the NxAdapter is getting
    created.
 
    In response this routine registers with Ndis.sys it it hasnt already
    registered. 
 
Arguments: 
 
    Driver - The NxDriver Object
 
    NxPrivateGlobals - The clients globals.
 
--*/

{
    NTSTATUS status;
    PNxDriver nxDriver;

    FuncEntry(FLAG_DRIVER);

    WdfWaitLockAcquire(g_RegistrationLock, NULL);

    status = _CreateIfNeeded(Driver, NxPrivateGlobals);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    nxDriver = NxPrivateGlobals->NxDriver;

    if (DriverType == NET_ADAPTER_DRIVER_TYPE_MINIPORT) {
        if (nxDriver->m_NdisMiniportDriverHandle != NULL) {
            // Already Registerd
            goto Exit;
        }
    } else {
        status = STATUS_INVALID_PARAMETER;
        LogInitMsg(TRACE_LEVEL_ERROR, "Unexpected Driver Type %!NET_ADAPTER_DRIVER_TYPE!", DriverType);
        goto Exit;
    }

    status = nxDriver->Register(DriverType);
    if (!NT_SUCCESS(status)) { 
        goto Exit;
    }

Exit:

    WdfWaitLockRelease(g_RegistrationLock);
    FuncExit(FLAG_DRIVER);
    return status;
}

NTSTATUS
NxDriver::Register(
    _In_ NET_ADAPTER_DRIVER_TYPE        DriverType
    )
/*++
Routine Description: 
    This routine is called when the client driver calls NetAdapterDriverRegister
    API.
 
    In response this routine registers with Ndis.sys on the behalf of the
    client driver.
 
Arguments: 
 
    DriverType - Type of the miniport driver (regular or intermediate) 
 
--*/

{
    FuncEntry(FLAG_DRIVER);

    UNREFERENCED_PARAMETER(DriverType);

    NTSTATUS                                status;
    PDRIVER_OBJECT                          driverObject;
    UNICODE_STRING                          registryPath;
    NDIS_HANDLE                             ndisMiniportDriverHandle;
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS    ndisMPChars;

    //
    // Register with Ndis.sys on behalf of the client. To to that : 
    // get the WDM driver object, registry path, and prepare the 
    // miniport driver chars structure, and then call 
    // NdisWdfRegisterMiniportDriver
    //

    driverObject = WdfDriverWdmGetDriverObject(GetFxObject());
    
    status = RtlUnicodeStringInit(&registryPath, WdfDriverGetRegistryPath(GetFxObject()));

    if(!WIN_VERIFY(NT_SUCCESS(status))) {
        FuncExit(FLAG_DRIVER);
        return status;
    }

    RtlZeroMemory(&ndisMPChars, sizeof(ndisMPChars));

    ndisMPChars.Header.Type        = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    ndisMPChars.Header.Size        = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
    ndisMPChars.Header.Revision    = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
    ndisMPChars.MajorNdisVersion   = NDIS_MINIPORT_MAJOR_VERSION;
    ndisMPChars.MinorNdisVersion   = NDIS_MINIPORT_MINOR_VERSION;

    //
    // NDIS should not hold driver version information of WDF managed miniports
    //
    ndisMPChars.MajorDriverVersion = 0;
    ndisMPChars.MinorDriverVersion = 0;

    ndisMPChars.Flags =  0;
    ndisMPChars.Flags |= NDIS_WDF_PNP_POWER_HANDLING; // prevent NDIS from owning the driver dispatch table
    ndisMPChars.Flags |= NDIS_DRIVER_POWERMGMT_PROXY; // this is a virtual device so need updated capabilities

    #pragma prefast(push)
    #pragma prefast(disable:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")

    ndisMPChars.SetOptionsHandler           =  NxDriver::_EvtNdisSetOptions;

    ndisMPChars.InitializeHandlerEx         =  NxAdapter::_EvtNdisInitializeEx;
    ndisMPChars.HaltHandlerEx               =  NxAdapter::_EvtNdisHaltEx;
    ndisMPChars.PauseHandler                =  NxAdapter::_EvtNdisPause;
    ndisMPChars.RestartHandler              =  NxAdapter::_EvtNdisRestart;
    ndisMPChars.OidRequestHandler           =  NxAdapter::_EvtNdisOidRequest;
    ndisMPChars.DirectOidRequestHandler     =  NxAdapter::_EvtNdisDirectOidRequest;
    ndisMPChars.SendNetBufferListsHandler   =  NxAdapter::_EvtNdisSendNetBufferLists;
    ndisMPChars.ReturnNetBufferListsHandler =  NxAdapter::_EvtNdisReturnNetBufferLists;
    ndisMPChars.CancelSendHandler           =  NxAdapter::_EvtNdisCancelSend;
    //For the WDF model, Check For Hang handler keeps waking up the miniport if Selective Suspend
    //is enabled. Instead of trying to address it in ndis, just dont register for
    //CheckForHang callback.
    //ndisMPChars.CheckForHangHandlerEx       =  NxAdapter::_EvtNdisCheckForHangEx;
    //ndisMPChars.ResetHandlerEx              =  NxAdapter::_EvtNdisResetEx;
    ndisMPChars.DevicePnPEventNotifyHandler =  NxAdapter::_EvtNdisDevicePnpEventNotify;
    ndisMPChars.ShutdownHandlerEx           =  NxAdapter::_EvtNdisShutdownEx;
    ndisMPChars.CancelOidRequestHandler     =  NxAdapter::_EvtNdisCancelOidRequest;
    ndisMPChars.CancelDirectOidRequestHandler = NxAdapter::_EvtNdisCancelDirectOidRequest;
    #pragma prefast(pop)

    LogVerbose(GetRecorderLog(), FLAG_DRIVER, "Calling NdisWdfRegisterMiniportDriver");
    status = NdisWdfRegisterMiniportDriver(driverObject,
                                           &registryPath,
                                           NetAdapterCxDriverHandle,
                                           (NDIS_HANDLE)this,
                                           &ndisMPChars,
                                           &ndisMiniportDriverHandle);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_DRIVER, "NdisWdfRegisterMiniportDriver Failed %!STATUS!", status);
        FuncExit(FLAG_DRIVER);
        return status;
    }

    //
    // Store the NDIS's miniport driver handle in NxDriver object for 
    // future reference. 
    //
    NT_ASSERTMSG("Unexpected Driver Type", DriverType == NET_ADAPTER_DRIVER_TYPE_MINIPORT);
    NT_ASSERT(m_NdisMiniportDriverHandle == NULL);
    m_NdisMiniportDriverHandle = ndisMiniportDriverHandle;
    
    FuncExit(FLAG_DRIVER);
    return status;
    
}

NDIS_STATUS
NxDriver::_EvtNdisSetOptions(
    _In_  NDIS_HANDLE  NdisDriverHandle,
    _In_  NDIS_HANDLE  NxDriverAsContext
    )
/*++
Routine Description:
 
    An Ndis Event Callback
 
    The MiniportSetOptions function registers optional handlers.  For each
    optional handler that should be registered, this function makes a call
    to NdisSetOptionalHandlers.

    MiniportSetOptions runs at IRQL = PASSIVE_LEVEL.

Arguments:

    DriverContext  The context handle

Return Value:

    NDIS_STATUS_xxx code

--*/
{
    FuncEntry(FLAG_DRIVER);
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PNxDriver nxDriver = (PNxDriver)NxDriverAsContext;

    UNREFERENCED_PARAMETER(NdisDriverHandle);

    LogVerbose(nxDriver->GetRecorderLog(), FLAG_DRIVER, "Called NxDriver::_SetOptions");

    FuncExit(FLAG_DRIVER);
    return Status;

}

VOID
NxDriver::_EvtWdfCleanup(
    _In_  WDFOBJECT Driver
    )
/*++
Routine Description: 
 
    A WDF Event Callback
 
    This event callback is called when the client's WDFDRIVER object
    is being deleted.
 
    NetAdapterCx.sys unregisters the client from Ndis.sys in this routine.
 
--*/
{
    FuncEntry(FLAG_DRIVER);

    PNxDriver nxDriver = GetNxDriverFromWdfDriver((WDFDRIVER)Driver);

    if (nxDriver->m_NdisMiniportDriverHandle) {
        LogVerbose(nxDriver->GetRecorderLog(), FLAG_DRIVER, "Calling NdisMDeregisterMiniportDriver");
        NdisMDeregisterMiniportDriver(nxDriver->m_NdisMiniportDriverHandle);
        nxDriver->m_NdisMiniportDriverHandle = NULL;
    } else {
        LogVerbose(nxDriver->GetRecorderLog(), FLAG_DRIVER, "Did not call NdisMDeregisterMiniportDriver since m_NdisMiniportDriverHandle was NULL\n");
    }

    FuncExit(FLAG_DRIVER);
    return;
}
