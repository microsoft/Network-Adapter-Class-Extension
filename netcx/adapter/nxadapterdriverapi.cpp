// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxDriver.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include "NxAdapterDriverApi.tmh"

extern
WDFWAITLOCK
    g_RegistrationLock;

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterDriverRegister)(
    _In_     NET_DRIVER_GLOBALS *        Globals,
    _In_     WDFDRIVER                   Driver
)
/*++
Routine Description:
    The client driver calls this method to register itself as a NetAdapterDriver
    At the backend we register with NDIS.
    The registration is optional as we would auto-register (if needed) at the
    time the client creates a NetAdapter object

Arguments:

    Driver : A handle to a WDFDRIVER object that represents the cleint driver.

Return Value:

    STATUS_SUCCESS upon success.
    Returns other NTSTATUS values.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    {
        auto exclusive = wil::acquire_wdf_wait_lock(g_RegistrationLock);

        //
        // First see if we need create a NxDriver.
        //
        CX_RETURN_IF_NOT_NT_SUCCESS(
            NxDriver::_CreateIfNeeded(Driver, nxPrivateGlobals));

        NxDriver *nxDriver = GetNxDriverFromWdfDriver(Driver);

        if (nxDriver->GetNdisMiniportDriverHandle() != NULL) {
            LogError(FLAG_DRIVER, "Driver already registered");
            CX_RETURN_IF_NOT_NT_SUCCESS(STATUS_INVALID_DEVICE_STATE);
        }

        CX_RETURN_IF_NOT_NT_SUCCESS(nxDriver->Register());
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_HANDLE
NETEXPORT(NetAdapterDriverWdmGetHandle)(
    _In_     NET_DRIVER_GLOBALS *        Globals,
    _In_     WDFDRIVER                   Driver
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

Return Value:

    NDIS_HANDLE
--*/
{
    UNREFERENCED_PARAMETER(Driver);

    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    return nxPrivateGlobals->NxDriver->GetNdisMiniportDriverHandle();
}

