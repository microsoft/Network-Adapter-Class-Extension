/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxAdapterDriverApi.cpp

Abstract:

    This module contains the "C" interface for the NxAdapter object.





Environment:

    kernel mode only

Revision History:

--*/

#include "Nx.hpp"

// Tracing support
extern "C" {
#include "NxAdapterDriverApi.tmh"
}

//
// extern the whole file
//
extern "C" {

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterDriverRegister)(
    _In_     PNET_DRIVER_GLOBALS         Globals,
    _In_     WDFDRIVER                   Driver,
    _In_     NET_ADAPTER_DRIVER_TYPE     DriverType
    )
/*++
Routine Description:
    The client driver calls this method to register itself as a NetAdapterDriver
    At the backend we register with NDIS. 
    The registration is optional as we would auto-register (if needed) at the
    time the client creates a NetAdapter object
 
Arguments:
   
    Driver : A handle to a WDFDRIVER object that represents the cleint driver.
 
    DriverType : A type of the driver
        NET_ADAPTER_DRIVER_TYPE_MINIPORT
 
Return Value:

    STATUS_SUCCESS upon success.
    Returns other NTSTATUS values.

--*/
{
    FuncEntry(FLAG_ADAPTER);
    PNX_PRIVATE_GLOBALS      NxPrivateGlobals;
    NTSTATUS                 status;
    PNxDriver nxDriver;

    FuncEntry(FLAG_DRIVER);

    NxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(NxPrivateGlobals);
    Verifier_VerifyIrqlPassive(NxPrivateGlobals);

    WdfWaitLockAcquire(g_RegistrationLock, NULL);

    //
    // First see if we need create a NxDriver. 
    //
    status = NxDriver::_CreateIfNeeded(Driver, NxPrivateGlobals);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    nxDriver = GetNxDriverFromWdfDriver(Driver);

    if (DriverType == NET_ADAPTER_DRIVER_TYPE_MINIPORT) {
        if (nxDriver->GetNdisMiniportDriverHandle() != NULL) {
            status = STATUS_INVALID_DEVICE_STATE;
            LogError(NULL, FLAG_DRIVER, "Driver Type %!NET_ADAPTER_DRIVER_TYPE! already registered", DriverType);
            goto Exit;
        }
    } else {
        status = STATUS_INVALID_PARAMETER;
        LogError(NULL, FLAG_DRIVER, "Unexpected Driver Type %!NET_ADAPTER_DRIVER_TYPE!", DriverType);
        goto Exit;
    }

    status = nxDriver->Register(DriverType);
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

Exit:

    WdfWaitLockRelease(g_RegistrationLock);

    FuncExit(FLAG_ADAPTER);
    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_HANDLE
NETEXPORT(NetAdapterDriverWdmGetHandle)(
    _In_     PNET_DRIVER_GLOBALS         Globals,
    _In_     WDFDRIVER                   Driver,
    _In_     NET_ADAPTER_DRIVER_TYPE     DriverType
    )
/*++
Routine Description:
    The client driver calls this method to retrieve a NDIS_HANDLE corresponding
    to the WDFDRIVER and input DriverType
 
    This routine can only be called after the client has either:
 
    - explictly successfully registered itself as a NetAdapterDriver using
      NetAdapterDriverRegister
    OR
    - implicilty registered itself as a NetAdapterDriver since it successfuly
      created a NETADAPTER object
 
Arguments:
   
    Driver : A handle to a WDFDRIVER object that represents the cleint driver.
 
    DriverType : A type of the driver
        NET_ADAPTER_DRIVER_TYPE_MINIPORT

Return Value:

    NDIS_HANDLE
--*/
{
    PNX_PRIVATE_GLOBALS pNxPrivateGlobals;
    PNxDriver           nxDriver;

    UNREFERENCED_PARAMETER(Driver);

    pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    nxDriver = pNxPrivateGlobals->NxDriver;

    if (DriverType == NET_ADAPTER_DRIVER_TYPE_MINIPORT) {
        return nxDriver->GetNdisMiniportDriverHandle();
    } else {
        LogError(NULL, FLAG_DRIVER, "Unexpected Driver Type %!NET_ADAPTER_DRIVER_TYPE!", DriverType);
        NT_ASSERTMSG("Unexpected Driver Type", FALSE);
    }
    return NULL;
}

} // extern "C"
