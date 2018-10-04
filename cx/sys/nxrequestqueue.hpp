// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxRequestQueue object.

--*/

#pragma once

#include <FxObjectBase.hpp>

//
// The NxRequestQueue is an object that represents a Request Queue
//

struct NX_PRIVATE_GLOBALS;

class NxAdapter;
class NxRequest;
class NxRequestQueue;

FORCEINLINE
NxRequestQueue *
GetNxRequestQueueFromHandle(
    _In_ NETREQUESTQUEUE RequestQueue
    );

class NxRequestQueue : public CFxObject<NETREQUESTQUEUE,
                                        NxRequestQueue,
                                        GetNxRequestQueueFromHandle,
                                        false>
{

private:
    //
    // Client's Private Driver Globals
    //
    NX_PRIVATE_GLOBALS *         m_NxPrivateGlobals;

    //
    // A copy of the config structure that client passed in
    //
    NET_REQUEST_QUEUE_CONFIG     m_Config;

    //
    // Spinlock to protect the m_RequestsListHead
    //
    KSPIN_LOCK                   m_RequestsListLock;

    //
    // List Entry to Track All Requests For this queue
    //
    LIST_ENTRY                   m_RequestsListHead;

public:
    //
    // Pointer to the corresponding NxAdapter object
    //
    NxAdapter *                  m_NxAdapter;

private:
    NxRequestQueue(
        _In_ NX_PRIVATE_GLOBALS *      NxPrivateGlobals,
        _In_ NETREQUESTQUEUE           RequestQueue,
        _In_ NxAdapter *               NxAdapter,
        _In_ PNET_REQUEST_QUEUE_CONFIG Config
        );

public:

    ~NxRequestQueue();

    static
    NTSTATUS
    _Create(
        _In_     NX_PRIVATE_GLOBALS *      PrivateGlobals,
        _In_     NxAdapter *               NxAdatper,
        _In_opt_ PWDF_OBJECT_ATTRIBUTES    ClientAttributes,
        _In_     PNET_REQUEST_QUEUE_CONFIG Config,
        _Out_    NxRequestQueue **         Queue
        );

    VOID
    ReferenceHandlers(
        VOID
        );

    static
    VOID
    _FreeHandlers(
        _In_ PNET_REQUEST_QUEUE_CONFIG RequestQueueConfig
        );

    RECORDER_LOG
    GetRecorderLog(
        void
        );

    VOID
    QueueRequest(
        _In_ NETREQUEST Request
        );

    VOID
    DispatchRequest(
        _In_ NxRequest * NxRequest
        );

    VOID
    DisconnectRequest(
        _In_ NxRequest * NxRequest
        );

};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxRequestQueue, _GetNxRequestQueueFromHandle);

FORCEINLINE
NxRequestQueue *
GetNxRequestQueueFromHandle(
    _In_ NETREQUESTQUEUE     RequestQueue
    )
/*++
Routine Description:

    This routine is just a wrapper around the _GetNxRequestQueueFromHandle function.
    To be able to define a the NxRequestQueue class above, we need a forward declaration of the
    accessor function. Since _GetNxRequestQueueFromHandle is defined by Wdf, we dont want to
    assume a prototype of that function for the foward declaration.

--*/

{
    return _GetNxRequestQueueFromHandle(RequestQueue);
}

