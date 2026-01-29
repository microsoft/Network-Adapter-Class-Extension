// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <ntddndis_p.h>
#define _NTDEF_
#include <ntstrsafe.h>
#ifdef _KERNEL_MODE
#include <ndis/lwminiporttypes.h>
#endif

#include "ExecutionContext.h"
#include "NxDriver.hpp"
#include "ndisminiport/NdisMiniportCharacteristics.hpp"
#include "NxPrivateGlobals.hpp"

#include "NxDriver.tmh"

extern
WDFWAITLOCK
    g_RegistrationLock;

extern
NDIS_WDF_CX_DRIVER
    NetAdapterCxDriverHandle;

static
EVT_WDF_OBJECT_CONTEXT_CLEANUP
    EvtDriverCleanup;

EXECUTION_CONTEXT_RUNTIME_KNOBS NxDriver::DevicelessEcRuntimeKnobs
{
    sizeof(EXECUTION_CONTEXT_RUNTIME_KNOBS), // Size
    ExecutionContextFlagRunDpcForFirstLoop, // Flags
    0, // MaxTimeAtDispatch
    0, // DispatchTimeWarning
    15 * 60 * 1000, // DispatchTimeWarningInterval
    80, // DpcWatchdogTimerThreshold
    10, // WorkerThreadPriority
    {
        64, // MaxPacketsSend.AtPassive
        64, // MaxPacketsSend.AtDispatch
    },
    {
        64, // MaxPacketsSendComplete.AtPassive
        64, // MaxPacketsSendComplete.AtDispatch
    },
    {
        64, // MaxPacketsReceive.AtPassive
        64, // MaxPacketsReceive.AtDispatch
    },
    {
        64, // MaxPacketsReceiveComplete.AtPassive
        64, // MaxPacketsReceiveComplete.AtDispatch
    },
};

_Use_decl_annotations_
NxDriver::NxDriver(
    WDFDRIVER Driver,
    NX_PRIVATE_GLOBALS * NxPrivateGlobals
)
    : CFxObject(Driver)
    , m_privateGlobals(NxPrivateGlobals)
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
        LogError(FLAG_DRIVER, "WdfObjectAllocateContext Failed %!STATUS!", ntStatus);
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
    NX_PRIVATE_GLOBALS * NxPrivateGlobals,
    NetCxDriverType DriverType
)
/*++
Routine Description:
    This routine is called when a client driver calls NetDeviceInitConfig.

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

    switch (DriverType)
    {
        case NetCxDriverType::PnpDeviceDriver:
        {
            if (nxDriver->GetNdisMiniportDriverHandle() != nullptr)
            {
                // Already registered
                return STATUS_SUCCESS;
            }

            CX_RETURN_IF_NOT_NT_SUCCESS(nxDriver->Register());
        }
        break;

        case NetCxDriverType::DevicelessDriver:
        {
            if (nxDriver->GetNdisLwmDriverHandle() != nullptr)
            {
                // Already registered
                return STATUS_SUCCESS;
            }

            CX_RETURN_IF_NOT_NT_SUCCESS(nxDriver->RegisterLwmDriver());
        }
        break;

        default:
            CX_RETURN_IF_NOT_NT_SUCCESS_MSG(STATUS_INVALID_PARAMETER, "Unknown driver type");
    }

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
#ifdef NETCX_KMDF
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
            &m_ndisMiniportDriverHandle),
        "NdisWdfRegisterMiniportDriver failed.");
#endif

    return STATUS_SUCCESS;
}

void
NxDriver::Deregister(
    void
)
{
    // In UMDF version this is done by RdDriver
#ifdef NETCX_KMDF
    if (m_ndisMiniportDriverHandle != nullptr)
    {
        NdisMDeregisterMiniportDriver(m_ndisMiniportDriverHandle);
        m_ndisMiniportDriverHandle = nullptr;
    }
#endif
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
    auto nxDriver = GetNxDriverFromWdfDriver(static_cast<WDFDRIVER>(Driver));
    nxDriver->Deregister();
}

NDIS_HANDLE
NxDriver::GetNdisMiniportDriverHandle(
    void
) const
{
    return m_ndisMiniportDriverHandle;
}

NX_PRIVATE_GLOBALS *
NxDriver::GetPrivateGlobals(
    void
) const
{
    return m_privateGlobals;
}

NDIS_HANDLE
NxDriver::GetNdisLwmDriverHandle(
    void
) const
{
    return m_ndisLwmDriverHandle.get();
}

NTSTATUS
NxDriver::RegisterLwmDriver(
    void
)
{
#ifdef NETCX_KMDF
    NDIS_LWM_DRIVER_CHARACTERISTICS ndisLWMChars;

    NdisLWMiniportDriverCharacteristicsInitialize(ndisLWMChars);

    NDIS_STATUS ndisStatus = NdisLWMRegisterMiniportDriver(
        static_cast<NDIS_HANDLE>(this),
        &ndisLWMChars,
        &m_ndisLwmDriverHandle);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NdisConvertNdisStatusToNtStatus(ndisStatus),
        "NdisLWMRegisterMiniportDriver failed.");
#endif
    return STATUS_SUCCESS;
}
