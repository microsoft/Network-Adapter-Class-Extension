// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the "C" interface for the NxRequest object.

--*/

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxRequestApi.tmh"

#include "NxAdapter.hpp"
#include "NxRequest.hpp"
#include "verifier.hpp"
#include "version.hpp"

//
// TODO: Discuss with Jeffrey if we can come up with better prototype here.
// how about NetRequestCompleteQueryWithInforamtion / NetRequestCompleteSetWithInformation
// Name collision : NetRequestSetInformation (WdfRequestSetInformation)
//
// TODO: Discuss with Jeffrey:
// In case of Set, we have BytesRead, BytesNeeded
//           Query, BytesWritten, BytesNeeded
// In case of Method - We have BytesRead, BytesWritten, BytesNeeded
//     Should we design the APIs to have BytesNeededForRead, ForWrite?
//


_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
void
NETEXPORT(NetRequestCompleteWithoutInformation)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request,
    _In_ NTSTATUS            CompletionStatus
)
/*++
Routine Description:

    This method is called by the clients to complete an NETREQUEST

Arguments:

    Request - A handle to a NETREQUEST object

    CompletionStatus - Completion Status of the Request.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);
    Verifier_VerifyNetRequestCompletionStatusNotPending(nxPrivateGlobals, Request, CompletionStatus);

    GetNxRequestFromHandle(Request)->Complete(CompletionStatus);

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
void
NETEXPORT(NetRequestSetDataComplete)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request,
    _In_ NTSTATUS            CompletionStatus,
    _In_ UINT                BytesRead
)
/*++
Routine Description:

    This method is called by the clients to complete an Set Data Request

Arguments:

    Request - A handle to a NETREQUEST object

    CompletionStatus - Completion Status of the Request.

    BytesRead - The number of bytes that were read by the client from the
        InputOutputBuffer

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);
    Verifier_VerifyNetRequestCompletionStatusNotPending(nxPrivateGlobals, Request, CompletionStatus);

    auto nxRequest = GetNxRequestFromHandle(Request);

    Verifier_VerifyNetRequestType(nxPrivateGlobals, nxRequest, NdisRequestSetInformation);

    nxRequest->m_ndisOidRequest->DATA.SET_INFORMATION.BytesRead = BytesRead;
    nxRequest->Complete(CompletionStatus);

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
void
NETEXPORT(NetRequestQueryDataComplete)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request,
    _In_ NTSTATUS            CompletionStatus,
    _In_ UINT                BytesWritten
)
/*++
Routine Description:

    This method is called by the clients to complete a Query Request

Arguments:

    Request - A handle to a NETREQUEST object

    CompletionStatus - Completion Status of the Request.

    BytesWritten - Number of bytes written by the client to the
        InputOutputBuffer

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);
    Verifier_VerifyNetRequestCompletionStatusNotPending(nxPrivateGlobals, Request, CompletionStatus);

    auto nxRequest = GetNxRequestFromHandle(Request);

    Verifier_VerifyNetRequestIsQuery(nxPrivateGlobals, nxRequest);

    nxRequest->m_ndisOidRequest->DATA.QUERY_INFORMATION.BytesWritten = BytesWritten;
    nxRequest->Complete(CompletionStatus);

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
void
NETEXPORT(NetRequestMethodComplete)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request,
    _In_ NTSTATUS            CompletionStatus,
    _In_ UINT                BytesRead,
    _In_ UINT                BytesWritten
)
/*++
Routine Description:

    This method is called by the clients to complete a Method Request

Arguments:

    Request - A handle to a NETREQUEST object

    CompletionStatus - Completion Status of the Request.

    BytesRead - The number of bytes that were read by the client from the
        InputOutputBuffer

    BytesWritten - Number of bytes written by the client to the
        InputOutputBuffer
--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);
    Verifier_VerifyNetRequestCompletionStatusNotPending(nxPrivateGlobals, Request, CompletionStatus);

    auto nxRequest = GetNxRequestFromHandle(Request);

    Verifier_VerifyNetRequestType(nxPrivateGlobals, nxRequest, NdisRequestMethod);

    nxRequest->m_ndisOidRequest->DATA.METHOD_INFORMATION.BytesWritten = BytesWritten;
    nxRequest->m_ndisOidRequest->DATA.METHOD_INFORMATION.BytesRead = BytesRead;

    nxRequest->Complete(CompletionStatus);

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
void
NETEXPORT(NetRequestSetBytesNeeded)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request,
    _In_ UINT                BytesNeeded
)
/*++
Routine Description:

    This method is called by the clients to set bytes needed for an
    NETREQUEST.

    This is useful only if the client is failing the Request due a smaller than
    expected InputOutputBuffer size.

    Depending on the request type, the BytesNeeded may mean BytesNeeded to be read,
    or BytesNeeded to be written.

Arguments:

    Request - A handle to a NETREQUEST object

    BytesNeeded - Number of bytes needed to be read / written

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    auto nxRequest = GetNxRequestFromHandle(Request);

    Verifier_VerifyNetRequest(nxPrivateGlobals, nxRequest);

    switch (nxRequest->m_ndisOidRequest->RequestType) {
    case NdisRequestSetInformation:
        nxRequest->m_ndisOidRequest->DATA.SET_INFORMATION.BytesNeeded = BytesNeeded;
        break;
    case NdisRequestQueryInformation:
    case NdisRequestQueryStatistics:
        nxRequest->m_ndisOidRequest->DATA.QUERY_INFORMATION.BytesNeeded = BytesNeeded;
        break;
    case NdisRequestMethod:
        nxRequest->m_ndisOidRequest->DATA.METHOD_INFORMATION.BytesNeeded = BytesNeeded;
        break;
    default:
        break;
    }

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_OID
NETEXPORT(NetRequestGetId)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request
)
/*++
Routine Description:

    This method is called by the clients to get the NDIS_OID id of the
    NETREQUEST

Arguments:

    Request - A handle to a NETREQUEST object

Returns:

    The NDIS_OID Id of the NETREQUEST.
--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_oid;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRequestRetrieveInputOutputBuffer)(
    _In_      NET_DRIVER_GLOBALS * Globals,
    _In_      NETREQUEST          Request,
    _In_      UINT                MininumInputLengthRequired,
    _In_      UINT                MininumOutputLengthRequired,
    _Outptr_result_bytebuffer_(max(*InputBufferLength,*OutputBufferLength))
              void **              InputOutputBuffer,
    _Out_opt_ PUINT               InputBufferLength,
    _Out_opt_ PUINT               OutputBufferLength

)
/*++
Routine Description:

    This method is called by the clients to retrive the input/output buffer
    of the request

Arguments:

    Request - A handle to a NETREQUEST object

    MininumInputLengthRequired - The minimum input length neede for the
        InputOutputBuffer. If the Request's buffer's InputLength is less than
        the minimum required, this routine returs a STATUS_BUFFER_TOO_SMALL
        error.

    MininumOutputLengthRequired - The minimum Output length neede for the
        InputOutputBuffer. If the Request's buffer's OutputLength is less than
        the minimum required, this routine returs a failure.

    InputOutputBuffer - Address to a location that recieves the pointer to the
        InputOutput buffer.

    InputBufferLength - Address to a location that recieves the actual input
        length of the InputOutputBuffer

    OutputBufferLength - Address to a location that recieves the actual output
        length of the InputOutputBuffer
--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    auto nxRequest = GetNxRequestFromHandle(Request);

    if (InputBufferLength  != NULL)
    {
        *InputBufferLength  = 0;
    }

    if (OutputBufferLength != NULL)
    {
        *OutputBufferLength = 0;
    }

    *InputOutputBuffer = NULL;

    if (nxRequest->m_ndisOidRequest == NULL) {
        //
        // The request may have already been comleted.
        // TODO: In case of a deferred completion requiest see if we
        // want to modify the logic to return a cloned InputOutputBuffer
        //
        LogError(nxRequest->GetRecorderLog(), FLAG_REQUEST,
                 "The Request probably has already been completed / deferred completed, STATUS_INVALID_DEVICE_STATE");
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (MininumInputLengthRequired > nxRequest->m_inputBufferLength) {
        LogError(nxRequest->GetRecorderLog(), FLAG_REQUEST,
                 "Required Input Length %d, Actual %d, STATUS_BUFFER_TOO_SMALL",
                 MininumInputLengthRequired, nxRequest->m_inputBufferLength);
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (MininumOutputLengthRequired > nxRequest->m_outputBufferLength) {
        LogError(nxRequest->GetRecorderLog(), FLAG_REQUEST,
                 "Required Output Length %d, Actual %d, STATUS_BUFFER_TOO_SMALL",
                 MininumOutputLengthRequired, nxRequest->m_outputBufferLength);
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (InputBufferLength != NULL) {
        *InputBufferLength = nxRequest->m_inputBufferLength;
    }

    if (OutputBufferLength != NULL) {
        *OutputBufferLength = nxRequest->m_outputBufferLength;
    }

    *InputOutputBuffer = nxRequest->m_inputOutputBuffer;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PNDIS_OID_REQUEST
NETEXPORT(NetRequestWdmGetNdisOidRequest)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request
)
/*++
Routine Description:

    This method is retrives the traditional WDM NDIS_OID_REQUEST structure
    for the NETREQUEST.

Arguments:

    Request - A handle to a NETREQUEST object

Returns:
    Pointer to the NDIS_OID_REQUEST structure

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_ndisOidRequest;
}


_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_PORT_NUMBER
NETEXPORT(NetRequestGetPortNumber)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request
)
/*++
Routine Description:

    This method is retrives the port nubmer for the NETREQUEST.

Arguments:

    Request - A handle to a NETREQUEST object

Returns:
    The port number corresponding to the NETREQUEST

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_ndisOidRequest->PortNumber;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_NIC_SWITCH_ID
NETEXPORT(NetRequestGetSwitchId)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request
)
/*++
Routine Description:

    This method is retrives the SwitchId for the NETREQUEST.

Arguments:

    Request - A handle to a NETREQUEST object

Returns:
    The SwitchId corresponding to the NETREQUEST

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_ndisOidRequest->SwitchId;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_NIC_SWITCH_VPORT_ID
NETEXPORT(NetRequestGetVPortId)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request
)
/*++
Routine Description:

    This method is retrives the VPortId for the NETREQUEST.

Arguments:

    Request - A handle to a NETREQUEST object

Returns:
    The VPortId corresponding to the NETREQUEST

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_ndisOidRequest->VPortId;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_REQUEST_TYPE
NETEXPORT(NetRequestGetType)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETREQUEST          Request
)
/*++
Routine Description:

    This method is retrives the Type for the NETREQUEST.

Arguments:

    Request - A handle to a NETREQUEST object

Returns:
    The Type corresponding to the NETREQUEST

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_ndisOidRequest->RequestType;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NETADAPTER
NETEXPORT(NetRequestGetAdapter)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETREQUEST Request
)
/*++
Routine Description:
    This method is called by the clients to retrieve the NETADAPTER
    object corresponding to NETREQUEST

Arguments:
    Request - The NETREQUEST handle

Returns:
    A handle to the corresponding NETADAPTER

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->GetNxAdapter()->GetFxObject();
}

