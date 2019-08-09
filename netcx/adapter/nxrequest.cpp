// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include "NxRequest.tmh"
#include "NxRequest.hpp"

#include "NxAdapter.hpp"
#include "NxMacros.hpp"
#include "NxRequestQueue.hpp"

NxRequest::NxRequest(
    _In_ NX_PRIVATE_GLOBALS *     NxPrivateGlobals,
    _In_ NETREQUEST               NetRequest,
    _In_ NxAdapter *              NxAdapter,
    _In_ PNDIS_OID_REQUEST        NdisOidRequest,
    _In_ NDIS_OID                 Oid,
    _In_ UINT                     InputBufferLength,
    _In_ UINT                     OutputBufferLength,
    _In_ void *                    InputOutputBuffer
) :
    CFxObject(NetRequest),
    m_NdisOidRequest(NdisOidRequest),
    m_NxAdapter(NxAdapter),
    m_Oid(Oid),
    m_InputBufferLength(InputBufferLength),
    m_OutputBufferLength(OutputBufferLength),
    m_InputOutputBuffer(InputOutputBuffer),
    m_CancellationStarted(FALSE)
/*++
Routine Description:
    Constructor for the NxRequest object. NxRequest needs to be a cancelable object
    thus it derives from the CFxCancelableObject class.
--*/
{
    UNREFERENCED_PARAMETER(NxPrivateGlobals);

    InitializeListEntry(&m_CancelTempListEntry);
    InitializeListEntry(&m_QueueListEntry);
}

NxRequest::~NxRequest(
    void
)
/*++
Routine Description:
    D'tor for the NxRequest object.
--*/
{




}

NTSTATUS
NxRequest::_Create(
    _In_     NX_PRIVATE_GLOBALS *     PrivateGlobals,
    _In_     NxAdapter *              NxAdapter,
    _In_     NDIS_OID_REQUEST *       NdisOidRequest,
    _Out_    NxRequest **             Request
)
/*++
Routine Description:
    Static method that creates the NETREQUEST object.

Arguments:
    NxAdapter - Pointer to the NxAdapter object to which this NetRequest is sent.

    NdisOidRequest - A pointer to the traditional ndis oid request structure

    Request - Ouput - Address to the location that accepts a pointer to the created
        NxRequest (on success)

Returns:
    NTSTATUS

Remarks:
    We only support the following ndis oid requests:
        NdisRequestSetInformation
        NdisRequestQueryInformation, NdisRequestQueryStatistics,
        NdisRequestMethod

    Other types are not supported.
--*/
{
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES attributes;
    NETREQUEST            netRequest;
    NDIS_OID              oid;
    UINT                  inputBufferLength;
    UINT                  outputBufferLength;
    void *                 inputOutputBuffer;

    switch (NdisOidRequest->RequestType) {
    case NdisRequestSetInformation:

        oid = NdisOidRequest->DATA.SET_INFORMATION.Oid;
        inputBufferLength = NdisOidRequest->DATA.SET_INFORMATION.InformationBufferLength;
        inputOutputBuffer = NdisOidRequest->DATA.SET_INFORMATION.InformationBuffer;

        //
        // Currently the NdisRequestSetInformation does not support
        // any output buffer.
        //
        outputBufferLength = 0;

        break;

    case NdisRequestQueryInformation:
    case NdisRequestQueryStatistics:

        oid = NdisOidRequest->DATA.QUERY_INFORMATION.Oid;
        outputBufferLength = NdisOidRequest->DATA.QUERY_INFORMATION.InformationBufferLength;
        inputOutputBuffer = NdisOidRequest->DATA.QUERY_INFORMATION.InformationBuffer;

        //
        // Currently the NdisRequestQueryStatistics, NdisRequestQueryInformation
        // does not support any input buffer.
        //
        inputBufferLength = 0;

        break;

    case NdisRequestMethod:

        oid = NdisOidRequest->DATA.METHOD_INFORMATION.Oid;
        outputBufferLength = NdisOidRequest->DATA.METHOD_INFORMATION.OutputBufferLength;
        inputBufferLength = NdisOidRequest->DATA.METHOD_INFORMATION.InputBufferLength;
        inputOutputBuffer = NdisOidRequest->DATA.METHOD_INFORMATION.InformationBuffer;

        break;

    default:

        LogError(NxAdapter->GetRecorderLog(), FLAG_REQUEST_QUEUE,
                 "Type %!NDIS_REQUEST_TYPE!, STATUS_NOT_SUPPORTED",
                 NdisOidRequest->RequestType);
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Create a WDFOBJECT for the NxRequest
    //

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxRequest);
    attributes.ParentObject = NxAdapter->GetFxObject();

    //
    // Ensure that the destructor would be called when this object is distroyed.
    //
    NxRequest::_SetObjectAttributes(&attributes);

    status = WdfObjectCreate(&attributes, (WDFOBJECT*)&netRequest);
    if (!NT_SUCCESS(status)) {
        LogError(NxAdapter->GetRecorderLog(), FLAG_REQUEST,
                 "WdfObjectCreate for NetRequest failed %!STATUS!", status);
        return status;
    }

    //
    // Since we just created the netRequest, the NxRequest object has
    // yet not been constructed. Get the NxRequest's memory.
    //
    void * nxRequestMemory = GetNxRequestFromHandle(netRequest);

    //
    // Use the inplacement new and invoke the constructor on the
    // NxRequest's memory
    //
    auto nxRequest = new (nxRequestMemory) NxRequest(PrivateGlobals,
                                                netRequest,
                                                NxAdapter,
                                                NdisOidRequest,
                                                oid,
                                                inputBufferLength,
                                                outputBufferLength,
                                                inputOutputBuffer);

    __analysis_assume(nxRequest != NULL);

    NT_ASSERT(nxRequest);

    if (NxAdapter->m_NetRequestObjectAttributes.Size != 0) {
        status = WdfObjectAllocateContext(netRequest, &NxAdapter->m_NetRequestObjectAttributes, NULL);
        if (!NT_SUCCESS(status)) {
            LogError(nxRequest->GetRecorderLog(), FLAG_REQUEST,
                     "WdfObjectAllocateContext for client's NetRequest attributes failed %!STATUS!", status);
            WdfObjectDelete(netRequest);
            return status;
        }
    }

    //
    // Dont Fail after this point or else the client's Cleanup / Destroy
    // callbacks can get called.
    //
    *Request = nxRequest;

    return status;
}

RECORDER_LOG
NxRequest::GetRecorderLog(
    void
)
{
    return m_NxAdapter->GetRecorderLog();
}

void
NxRequest::Complete(
    _In_ NTSTATUS CompletionStatus
)
/*++
Routine Description:
    This member function completes an oid request

Arguments:
    CompletionStatus - The status that to be used to complete the OID request.
--*/
{
    NDIS_STATUS oidCompletionStatus;

    if (CompletionStatus == STATUS_CANCELLED)
    {
        oidCompletionStatus = NDIS_STATUS_REQUEST_ABORTED;
    }
    else
    {
        oidCompletionStatus = NdisConvertNtStatusToNdisStatus(CompletionStatus);
    }

    // If this request is being completed from a NetAdapter
    // extension it is not attached to a request queue
    if (m_NxQueue != nullptr)
    {
        m_NxQueue->DisconnectRequest(this);
    }

    NdisMOidRequestComplete(m_NxAdapter->GetNdisHandle(),
                            m_NdisOidRequest,
                            oidCompletionStatus);

    WdfObjectDelete(GetFxObject());
}

NxAdapter *
NxRequest::GetNxAdapter() const
{
    return m_NxAdapter;
}

