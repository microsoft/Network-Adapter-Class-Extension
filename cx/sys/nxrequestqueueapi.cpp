// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the "C" interface for the NxRequestQueue object.

--*/

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxRequestQueueApi.tmh"

#include "NxAdapter.hpp"
#include "NxRequestQueue.hpp"
#include "verifier.hpp"
#include "version.hpp"

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRequestQueueCreate)(
    _In_      NET_DRIVER_GLOBALS *      Globals,
    _In_      NET_REQUEST_QUEUE_CONFIG * Configuration,
    _In_opt_  WDF_OBJECT_ATTRIBUTES *   QueueAttributes,
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
    NTSTATUS            status;

    if (Queue) { *Queue = NULL; }

    //
    // Parameter Validation.
    //
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize<NET_REQUEST_QUEUE_CONFIG>(pNxPrivateGlobals, Configuration);

    status = Verifier_VerifyQueueConfiguration(pNxPrivateGlobals, Configuration);

    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    //
    // Create the NxRequestQueue
    //
    NxRequestQueue * nxRequestQueue;
    status = NxRequestQueue::_Create(pNxPrivateGlobals,
                                 GetNxAdapterFromHandle(Configuration->Adapter),
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

    return status;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NETADAPTER
NETEXPORT(NetRequestQueueGetAdapter)(
    _In_      NET_DRIVER_GLOBALS *   Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxRequestQueueFromHandle(NetRequestQueue)->m_NxAdapter->GetFxObject();
}

