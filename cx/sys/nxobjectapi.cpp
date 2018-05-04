// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the "C" interface for a general Nx object.

--*/

#include "Nx.hpp"

#include "NxObjectApi.tmh"

//
// extern the whole file
//
extern "C" {

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetObjectMarkCancelableEx)(
    _In_      PNET_DRIVER_GLOBALS    Globals,
    _In_      WDFOBJECT              NetObject,
    _In_      PFN_NET_OBJECT_CANCEL  EvtCancel
    )
/*++
Routine Description:

    This routine sets a cancel routine on a cancelable WDFOBJECT.

    Following are the WDFOBJECTs that support this API:
        NETREQUEST
        NETPACKET *PENDING*

Arguments:

    NetObject - A handle to a cancelable WDFOBJECT.

    EvtCancel - A pointer to a cancel routine that is run if the object is
        canceled.

Return:
    STATUS_SUCCESS - If the cancel routine has been set successfully.
    STATUS_CANCELED - The the object has already been canceled. In that case
        the cancel routine will not be called.
--*/
{
    NTSTATUS status;
    PNxRequest nxRequest;
    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(Globals);

    FuncEntry(FLAG_NET_OBJECT);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);
    Verifier_VerifyNotNull(pNxPrivateGlobals, EvtCancel);
    //
    // First check if this is an NETOBJECT for which we support cancellation.
    // Currently supported objects are:
    //     NETREQUEST
    //
    Verifier_VerifyObjectSupportsCancellation(pNxPrivateGlobals, NetObject);

    nxRequest = GetNxRequestFromHandle((NETREQUEST)NetObject);

    status = nxRequest->MarkCancelable(EvtCancel);

    FuncExit(FLAG_NET_OBJECT);
    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetObjectUnmarkCancelable)(
    _In_      PNET_DRIVER_GLOBALS    Globals,
    _In_      WDFOBJECT              NetObject
    )
/*++
Routine Description:
    This routine clears the cancel routine that was previously set on a
    cancelable WDFOBJECT using the NetObjectMarkCancelableEx API

Arguments:

    NetObject - A handle to a cancelable WDFOBJECT.

Returns:

    STATUS_CANCELED - If the Object has already been canceled. In that case a
        previously set completion routine would be run or is runnning or has
        already run.

    STATUS_SUCCESS - The Object is no longer marked cancelable

--*/
{
    NTSTATUS status;
    PNxRequest nxRequest;
    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(Globals);

    FuncEntry(FLAG_NET_OBJECT);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    //
    // First check if this is an NETOBJECT for which we support cancellation.
    // Currently supported objects are:
    //     NETREQUEST
    //
    Verifier_VerifyObjectSupportsCancellation(pNxPrivateGlobals, NetObject);

    nxRequest = GetNxRequestFromHandle((NETREQUEST)NetObject);

    status = nxRequest->UnmarkCancelable();

    FuncExit(FLAG_NET_OBJECT);

    return status;
}

}
