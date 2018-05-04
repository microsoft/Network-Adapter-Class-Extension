// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxRequest object.

--*/

#pragma once

//
// The NxRequest is an object that represents a Ndis Oid Request
//

PNxRequest
FORCEINLINE
GetNxRequestFromHandle(
    _In_ NETREQUEST Request
    );

typedef class NxRequest *PNxRequest;
class NxRequest : public CFxCancelableObject<NETREQUEST,
                                             NxRequest,
                                             GetNxRequestFromHandle,
                                             false>
{
    friend class NxRequestQueue;

private:
    //
    // Pointer to the corresponding NxAdapter object
    //
    PNxAdapter                   m_NxAdapter;

    //
    // Pointer to the Oid Queue that is tracking this Oid
    //
    PNxRequestQueue              m_NxQueue;

    //
    // List Entry for the Queue level list of oids
    //
    LIST_ENTRY                   m_QueueListEntry;

    //
    // List Entry for a temporary cancel list
    //
    LIST_ENTRY                   m_CancelTempListEntry;

public:

    //
    // Pointer to the Ndis Oid Request
    //
    PNDIS_OID_REQUEST            m_NdisOidRequest;

    //
    // Copy of the OidId, input, output buffer lengths and the buffer pointer
    //
    NDIS_OID                     m_Oid;
    UINT                         m_InputBufferLength;
    UINT                         m_OutputBufferLength;
    PVOID                        m_InputOutputBuffer;

    BOOLEAN                      m_CancellationStarted;


private:
    NxRequest(
        _In_ PNX_PRIVATE_GLOBALS      NxPrivateGlobals,
        _In_ NETREQUEST               NetRequest,
        _In_ PNxAdapter               NxAdapter,
        _In_ PNDIS_OID_REQUEST        NdisOidRequest,
        _In_ NDIS_OID                 OidId,
        _In_ UINT                     InputBufferLength,
        _In_ UINT                     OutputBufferLength,
        _In_ PVOID                    InputOutputBuffer
        );

public:

    ~NxRequest();

    static
    NTSTATUS
    _Create(
        _In_     PNX_PRIVATE_GLOBALS      PrivateGlobals,
        _In_     PNxAdapter               NxAdatper,
        _In_     PNDIS_OID_REQUEST        NdisOidRequest,
        _Out_    PNxRequest*              Request
        );

    RECORDER_LOG
    GetRecorderLog() {
        return m_NxAdapter->GetRecorderLog();
    }

    VOID
    Complete(
        _In_ NTSTATUS CompletionStatus
        );

    VOID
    SetDataRequestSetInformation(
        _In_ ULONG BytesRead,
        _In_ ULONG BytesNeeded
        );

    VOID
    QueryDataRequestSetInformation(
        _In_ ULONG BytesWritten,
        _In_ ULONG BytesNeeded
        );

    VOID
    MethodRequestSetInformation(
        _In_ ULONG BytesRead,
        _In_ ULONG BytesWritten,
        _In_ ULONG BytesNeeded
        );

    PNxAdapter
    GetNxAdapter() const;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxRequest, _GetNxRequestFromHandle);

PNxRequest
FORCEINLINE
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
