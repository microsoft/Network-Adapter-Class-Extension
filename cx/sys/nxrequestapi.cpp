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














_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestCompleteWithoutInformation)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);
    Verifier_VerifyNetRequestCompletionStatusNotPending(pNxPrivateGlobals, Request, CompletionStatus);

    GetNxRequestFromHandle(Request)->Complete(CompletionStatus);

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestSetDataComplete)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);
    Verifier_VerifyNetRequestCompletionStatusNotPending(pNxPrivateGlobals, Request, CompletionStatus);

    auto nxRequest = GetNxRequestFromHandle(Request);

    Verifier_VerifyNetRequestType(pNxPrivateGlobals, nxRequest, NdisRequestSetInformation);

    nxRequest->m_NdisOidRequest->DATA.SET_INFORMATION.BytesRead = BytesRead;
    nxRequest->Complete(CompletionStatus);

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestQueryDataComplete)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);
    Verifier_VerifyNetRequestCompletionStatusNotPending(pNxPrivateGlobals, Request, CompletionStatus);

    auto nxRequest = GetNxRequestFromHandle(Request);

    Verifier_VerifyNetRequestIsQuery(pNxPrivateGlobals, nxRequest);

    nxRequest->m_NdisOidRequest->DATA.QUERY_INFORMATION.BytesWritten = BytesWritten;
    nxRequest->Complete(CompletionStatus);

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestMethodComplete)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);
    Verifier_VerifyNetRequestCompletionStatusNotPending(pNxPrivateGlobals, Request, CompletionStatus);

    auto nxRequest = GetNxRequestFromHandle(Request);

    Verifier_VerifyNetRequestType(pNxPrivateGlobals, nxRequest, NdisRequestMethod);

    nxRequest->m_NdisOidRequest->DATA.METHOD_INFORMATION.BytesWritten = BytesWritten;
    nxRequest->m_NdisOidRequest->DATA.METHOD_INFORMATION.BytesRead = BytesRead;

    nxRequest->Complete(CompletionStatus);

    return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestSetBytesNeeded)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    auto nxRequest = GetNxRequestFromHandle(Request);

    Verifier_VerifyNetRequest(pNxPrivateGlobals, nxRequest);

    switch (nxRequest->m_NdisOidRequest->RequestType) {
    case NdisRequestSetInformation:
        nxRequest->m_NdisOidRequest->DATA.SET_INFORMATION.BytesNeeded = BytesNeeded;
        break;
    case NdisRequestQueryInformation:
    case NdisRequestQueryStatistics:
        nxRequest->m_NdisOidRequest->DATA.QUERY_INFORMATION.BytesNeeded = BytesNeeded;
        break;
    case NdisRequestMethod:
        nxRequest->m_NdisOidRequest->DATA.METHOD_INFORMATION.BytesNeeded = BytesNeeded;
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
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_Oid;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRequestRetrieveInputOutputBuffer)(
    _In_      PNET_DRIVER_GLOBALS Globals,
    _In_      NETREQUEST          Request,
    _In_      UINT                MininumInputLengthRequired,
    _In_      UINT                MininumOutputLengthRequired,
    _Outptr_result_bytebuffer_(max(*InputBufferLength,*OutputBufferLength))
              PVOID*              InputOutputBuffer,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

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

    if (nxRequest->m_NdisOidRequest == NULL) {





        LogError(nxRequest->GetRecorderLog(), FLAG_REQUEST,
                 "The Request probably has already been completed / deferred completed, STATUS_INVALID_DEVICE_STATE");
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (MininumInputLengthRequired > nxRequest->m_InputBufferLength) {
        LogError(nxRequest->GetRecorderLog(), FLAG_REQUEST,
                 "Required Input Length %d, Actual %d, STATUS_BUFFER_TOO_SMALL",
                 MininumInputLengthRequired, nxRequest->m_InputBufferLength);
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (MininumOutputLengthRequired > nxRequest->m_OutputBufferLength) {
        LogError(nxRequest->GetRecorderLog(), FLAG_REQUEST,
                 "Required Output Length %d, Actual %d, STATUS_BUFFER_TOO_SMALL",
                 MininumOutputLengthRequired, nxRequest->m_OutputBufferLength);
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (InputBufferLength != NULL) {
        *InputBufferLength = nxRequest->m_InputBufferLength;
    }

    if (OutputBufferLength != NULL) {
        *OutputBufferLength = nxRequest->m_OutputBufferLength;
    }

    *InputOutputBuffer = nxRequest->m_InputOutputBuffer;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PNDIS_OID_REQUEST
NETEXPORT(NetRequestWdmGetNdisOidRequest)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_NdisOidRequest;
}


_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_PORT_NUMBER
NETEXPORT(NetRequestGetPortNumber)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_NdisOidRequest->PortNumber;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_NIC_SWITCH_ID
NETEXPORT(NetRequestGetSwitchId)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_NdisOidRequest->SwitchId;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_NIC_SWITCH_VPORT_ID
NETEXPORT(NetRequestGetVPortId)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_NdisOidRequest->VPortId;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_REQUEST_TYPE
NETEXPORT(NetRequestGetType)(
    _In_ PNET_DRIVER_GLOBALS Globals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->m_NdisOidRequest->RequestType;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NETADAPTER
NETEXPORT(NetRequestGetAdapter)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
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
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return GetNxRequestFromHandle(Request)->GetNxAdapter()->GetFxObject();
}

