// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include "NxDriver.tmh"
#include "NxDriver.hpp"

#include "NxAdapter.hpp"
#include "version.hpp"

static
EVT_WDF_OBJECT_CONTEXT_CLEANUP
    EvtDriverCleanup;

_Use_decl_annotations_
NxDriver::NxDriver(
    WDFDRIVER Driver,
    NX_PRIVATE_GLOBALS * NxPrivateGlobals
)
    : CFxObject(Driver)
    , m_PrivateGlobals(NxPrivateGlobals)
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
    //
    // Create a WPP Log buffer for the client.
    //

    RECORDER_LOG_CREATE_PARAMS recorderLogCreateParams;
    RECORDER_LOG_CREATE_PARAMS_INIT(&recorderLogCreateParams, nullptr);

    recorderLogCreateParams.TotalBufferSize = DEFAULT_WPP_TOTAL_BUFFER_SIZE;
    recorderLogCreateParams.ErrorPartitionSize = DEFAULT_WPP_ERROR_PARTITION_SIZE;

    //
    // ClientDriverGlobals->DriverName is a CHAR array that contains a NULL terminated string
    //
    auto clientDriverName = &m_PrivateGlobals->ClientDriverGlobals->DriverName[0];

    //
    // If clientDriverName length is larger than what LogIdentifier supports the string will
    // be truncated
    //
    RtlStringCchPrintfA(
        recorderLogCreateParams.LogIdentifier,
        sizeof(recorderLogCreateParams.LogIdentifier),
        "%s",
        clientDriverName);

    RECORDER_LOG recorderLog;
    NTSTATUS ntStatus = WppRecorderLogCreate(&recorderLogCreateParams, &recorderLog);

    if (NT_SUCCESS(ntStatus))
    {
        m_RecorderLog = recorderLog;
    }
    else
    {
        //
        // Non-fatal failure
        //
        LogInitMsg(TRACE_LEVEL_WARNING, "Could Not allocate Recorder Log for NetAdapterCxClient : %s", clientDriverName);
    }
}

NxDriver::~NxDriver(
    void
)
/*++
Routine Description:

    D'tor for the NxDriver object.

--*/
{
    if (m_RecorderLog != nullptr)
    {
        //
        // Delete the log buffer created in the constructor.
        //
        WppRecorderLogDelete(m_RecorderLog);
        m_RecorderLog = nullptr;
    }
}

_Use_decl_annotations_
NTSTATUS
NxDriver::_CreateIfNeeded(
    WDFDRIVER Driver,
    NX_PRIVATE_GLOBALS * NxPrivateGlobals
)
{
    if (NxPrivateGlobals->NxDriver != nullptr)
    {
        return STATUS_SUCCESS;
    }

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxDriver);

    //
    // Callin this ensures that our d'tor will be called on destory.
    //
    NxDriver::_SetObjectAttributes(&attributes);

    //
    // Allocate a context on the client WDFDRIVER object and create a
    // NxDriver object on it using an in-placement new call.
    //
    attributes.EvtCleanupCallback = EvtDriverCleanup;

    void * nxDriverAsContext;
    NTSTATUS ntStatus = WdfObjectAllocateContext(
        Driver,
        &attributes,
        &nxDriverAsContext);

    if (!NT_SUCCESS(ntStatus))
    {
        LogInitMsg(TRACE_LEVEL_ERROR, "WdfObjectAllocateContext Failed %!STATUS!", ntStatus);
        return ntStatus;
    }

    auto nxDriver = new (nxDriverAsContext) NxDriver(Driver, NxPrivateGlobals);

    __analysis_assume(nxDriver != NULL);

    NxPrivateGlobals->NxDriver = nxDriver;

    return ntStatus;
}

_Use_decl_annotations_
NTSTATUS
NxDriver::_CreateAndRegisterIfNeeded(
    NX_PRIVATE_GLOBALS * NxPrivateGlobals
)
/*++
Routine Description:
    This routine is called when a client driver calls NetAdapterDeviceInitConfig.

    In response this routine registers with Ndis.sys it it hasnt already
    registered.

Arguments:

    NxPrivateGlobals - The clients globals.

--*/

{
    auto wdfDriver = NxPrivateGlobals->ClientDriverGlobals->Driver;
    auto exclusive = wil::acquire_wdf_wait_lock(g_RegistrationLock);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        _CreateIfNeeded(wdfDriver, NxPrivateGlobals));

    auto nxDriver = NxPrivateGlobals->NxDriver;

    if (nxDriver->GetNdisMiniportDriverHandle() != nullptr)
    {
        // Already registered
        return STATUS_SUCCESS;
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(nxDriver->Register());

    return STATUS_SUCCESS;
}

NTSTATUS
NxDriver::Register(
    void
)
/*++
Routine Description:
    This routine is called when the client driver calls NetAdapterDriverRegister
    API.

    In response this routine registers with Ndis.sys on the behalf of the
    client driver.

--*/

{
    //
    // Register with Ndis.sys on behalf of the client. To to that :
    // get the WDM driver object, registry path, and prepare the
    // miniport driver chars structure, and then call
    // NdisWdfRegisterMiniportDriver
    //

    UNICODE_STRING registryPath = {};

    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlUnicodeStringInit(
            &registryPath,
            WdfDriverGetRegistryPath(GetFxObject())));

    NDIS_MINIPORT_DRIVER_CHARACTERISTICS ndisMPChars;
    NdisMiniportDriverCharacteristicsInitialize(ndisMPChars);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NdisWdfRegisterMiniportDriver(
            WdfDriverWdmGetDriverObject(GetFxObject()),
            &registryPath,
            NetAdapterCxDriverHandle,
            (NDIS_HANDLE)this,
            &ndisMPChars,
            &m_NdisMiniportDriverHandle),
        "NdisWdfRegisterMiniportDriver failed.");

    return STATUS_SUCCESS;
}

void
NxDriver::Deregister(
    void
)
{
    if (m_NdisMiniportDriverHandle != nullptr)
    {
        NdisMDeregisterMiniportDriver(m_NdisMiniportDriverHandle);
        m_NdisMiniportDriverHandle = nullptr;
    }
}

_Use_decl_annotations_
void
EvtDriverCleanup(
    WDFOBJECT Driver
)
/*++
Routine Description:

    A WDF Event Callback

    This event callback is called when the client's WDFDRIVER object
    is being deleted.

    NetAdapterCx.sys unregisters the client from Ndis.sys in this routine.

--*/
{
    auto nxDriver = GetNxDriverFromWdfDriver((WDFDRIVER)Driver);
    nxDriver->Deregister();
}

RECORDER_LOG
NxDriver::GetRecorderLog(
    void
) const
{
    return m_RecorderLog;
}

NDIS_HANDLE
NxDriver::GetNdisMiniportDriverHandle(
    void
) const
{
    return m_NdisMiniportDriverHandle;
}

NX_PRIVATE_GLOBALS *
NxDriver::GetPrivateGlobals(
    void
) const
{
    return m_PrivateGlobals;
}

