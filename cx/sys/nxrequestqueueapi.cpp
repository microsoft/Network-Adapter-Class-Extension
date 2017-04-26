/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxRequestQueueApi.cpp

Abstract:

    This module contains the "C" interface for the NxRequestQueue object.





Environment:

    kernel mode only

Revision History:

--*/

#include "Nx.hpp"

// Tracing support
extern "C" {
#include "NxRequestQueueApi.tmh"
}

//
// extern the whole file
//
extern "C" {

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRequestQueueCreate)(
    _In_      PNET_DRIVER_GLOBALS       Globals,
    _In_      PNET_REQUEST_QUEUE_CONFIG Configuration,
    _In_opt_  PWDF_OBJECT_ATTRIBUTES    QueueAttributes,
    _Out_opt_ NETREQUESTQUEUE*          Queue
    )
/*++
Routine Description:

    This method is called by the clients create a NetRequestQueue
 
    The client driver typically calls this from its EvtDriverDeviceAdd routine after
    the client has successfully create a NETADAPTER object. 
 
Arguments:
 
    Adapter : A handle to a NETADAPTER object that represents the network
        adapter
 
    QueueAttributes : Pointer to the WDF_OBJECT_ATTRIBUTES structure. This is
        the attributes structure that a Wdf client can pass at the time of
        creating any wdf object.
 
        The ParentObject of this structure should be NULL as NETREQUESTQUEUE object
        is parented to the the NETADAPTER passed in.
 
    Configuration : A Pointer to a structure containing the configuration
        parameters. This contains client callbacks, queue type, custom etc
 
    Queue : Output NETREQUESTQUEUE object
 
Return Value:

    STATUS_SUCCESS upon success.
    Returns other NTSTATUS values.

--*/
{
    FuncEntry(FLAG_REQUEST_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals;
    NTSTATUS            status;
    PNxRequestQueue     nxRequestQueue;
    PNxAdapter          nxAdapter;

    if (Queue) { *Queue = NULL; }

    //
    // Parameter Validation.
    //
    pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize<NET_REQUEST_QUEUE_CONFIG>(pNxPrivateGlobals, Configuration);

    status = Verifier_VerifyQueueConfiguration(pNxPrivateGlobals, Configuration);

    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    nxAdapter = GetNxAdapterFromHandle(Configuration->Adapter);

    //
    // Create the NxRequestQueue
    //
    status = NxRequestQueue::_Create(pNxPrivateGlobals, 
                                 nxAdapter, 
                                 QueueAttributes, 
                                 Configuration, 
                                 &nxRequestQueue);

    if (!NT_SUCCESS(status)) {
        if (Queue) {
            *Queue = NULL;
        }
        goto Exit;
    }

    if (Queue) {
        *Queue = nxRequestQueue->GetFxObject();
    }

Exit:

    if (!NT_SUCCESS(status)) {
        //
        // We must free the memory for the handlers
        //
        NxRequestQueue::_FreeHandlers(Configuration);
    }

    FuncExit(FLAG_REQUEST_QUEUE);
    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NETADAPTER
NETEXPORT(NetRequestQueueGetAdapter)(
    _In_      PNET_DRIVER_GLOBALS    Globals,
    _In_      NETREQUESTQUEUE            NetRequestQueue
    )
/*++
Routine Description:

    This method is called by the clients to retrieve the NETADAPTER
    object corresponding to NETREQUESTQUEUE
    
Arguments: 
 
    NetRequestQueue - The NETREQUESTQUEUE handler 
 
Returns: 
    A handle to the corresponding NETADAPTER
 
--*/
{
    FuncEntry(FLAG_REQUEST_QUEUE);

    PNxRequestQueue              nxRequestQueue;
    NETADAPTER               adapter;

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    nxRequestQueue = GetNxRequestQueueFromHandle(NetRequestQueue);
    adapter = nxRequestQueue->m_NxAdapter->GetFxObject();

    FuncExit(FLAG_REQUEST_QUEUE);
    return adapter;
}

}
