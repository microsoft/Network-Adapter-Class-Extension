// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxRequest object.

--*/

#pragma once

#include <FxObjectBase.hpp>

#include <preview/netrequest.h>

static const ULONG OidSignature = 'SOid';
static const ULONG DirectOidSignature = 'DOid';

struct DispatchContext
{
    size_t NextHandlerIndex = 0;
    ULONG Signature;
};

static_assert(sizeof(DispatchContext) <= sizeof(NDIS_OID_REQUEST::MiniportReserved));

//
// The NxRequest is an object that represents a Ndis Oid Request
//

struct NX_PRIVATE_GLOBALS;

class NxAdapter;
class NxRequest;

FORCEINLINE
NxRequest *
GetNxRequestFromHandle(
    _In_ NETREQUEST Request
);

class NxRequest
    : public CFxObject<NETREQUEST, NxRequest, GetNxRequestFromHandle, false>
{
    friend class NxRequestQueue;

private:
    //
    // Pointer to the corresponding NxAdapter object
    //
    NxAdapter *
        m_adapter;

    //
    // Pointer to the Oid Queue that is tracking this Oid
    //
    NxRequestQueue *
        m_queue;

    //
    // List Entry for the Queue level list of oids
    //
    LIST_ENTRY
        m_queueListEntry;

    //
    // List Entry for a temporary cancel list
    //
    LIST_ENTRY
        m_cancelTempListEntry;

public:

    //
    // Pointer to the Ndis Oid Request
    //
    PNDIS_OID_REQUEST
        m_ndisOidRequest;

    //
    // Copy of the OidId, input, output buffer lengths and the buffer pointer
    //
    NDIS_OID
        m_oid;

    UINT
        m_inputBufferLength;

    UINT
        m_outputBufferLength;

    void *
        m_inputOutputBuffer;

    BOOLEAN
        m_cancellationStarted;

private:
    NxRequest(
        _In_ NX_PRIVATE_GLOBALS * NxPrivateGlobals,
        _In_ NETREQUEST NetRequest,
        _In_ NxAdapter * NxAdapter,
        _In_ NDIS_OID_REQUEST * NdisOidRequest,
        _In_ NDIS_OID OidId,
        _In_ UINT InputBufferLength,
        _In_ UINT OutputBufferLength,
        _In_ void * InputOutputBuffer
    );

public:

    ~NxRequest(
        void
    );

    static
    NTSTATUS
    _Create(
        _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
        _In_ NxAdapter * NxAdatper,
        _In_ PNDIS_OID_REQUEST NdisOidRequest,
        _Out_ NxRequest ** Request
    );

    RECORDER_LOG
    GetRecorderLog(
        void
    );

    void
    Complete(
        _In_ NTSTATUS CompletionStatus
    );

    void
    SetDataRequestSetInformation(
        _In_ ULONG BytesRead,
        _In_ ULONG BytesNeeded
    );

    void
    QueryDataRequestSetInformation(
        _In_ ULONG BytesWritten,
        _In_ ULONG BytesNeeded
    );

    void
    MethodRequestSetInformation(
        _In_ ULONG BytesRead,
        _In_ ULONG BytesWritten,
        _In_ ULONG BytesNeeded
    );

    NxAdapter *
    GetNxAdapter(
        void
    ) const;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxRequest, _GetNxRequestFromHandle);

FORCEINLINE
NxRequest *
GetNxRequestFromHandle(
    _In_ NETREQUEST     Request
    )
/*++
Routine Description:

    This routine is just a wrapper around the _GetNxRequestFromHandle function.
    To be able to define a the NxRequest class above, we need a forward declaration of the
    accessor function. Since _GetNxRequestFromHandle is defined by Wdf, we dont want to
    assume a prototype of that function for the foward declaration.

--*/

{
    return _GetNxRequestFromHandle(Request);
}

