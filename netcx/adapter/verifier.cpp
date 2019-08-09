// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:
    Contains verifier function's implementations

--*/

#include "Nx.hpp"

#include <preview/netrequest.h>
#include <preview/netrequestqueue.h>

#include "verifier.tmh"
#include "verifier.hpp"

#include <net/checksumtypes_p.h>
#include <net/logicaladdresstypes_p.h>
#include <net/lsotypes_p.h>
#include <net/mdltypes_p.h>
#include <net/returncontexttypes_p.h>
#include <net/rsctypes_p.h>
#include <net/virtualaddresstypes_p.h>

#include "NxAdapter.hpp"
#include "NxDevice.hpp"
#include "NxDriver.hpp"
#include "NxMacros.hpp"
#include "NxQueue.hpp"
#include "NxRequest.hpp"
#include "powerpolicy/NxPowerList.hpp"
#include "version.hpp"

//
// If you change this behavior, or add new ones, make sure to update these comments.
//
// If you plan to add new Verifier functions, make sure to follow the pattern described bellow.
//
// Verifier_Verify* functions that use only VerifierAction_BugcheckAlways should not return any value.
// Verifier_Verify* functions that use VerifierAction_DbgBreakIfDebuggerPresent at least one time should return NTSTATUS, and the caller should deal with the status
//

void
Verifier_ReportViolation(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ VerifierAction      Action,
    _In_ FailureCode         FailureCode,
    _In_ ULONG_PTR           Parameter2,
    _In_ ULONG_PTR           Parameter3
)
/*++
Routine Description:
    This function takes failure parameters, and based on Action decides how to report it.
--*/
{
    switch (Action)
    {
        case VerifierAction_BugcheckAlways:
            NetAdapterCxBugCheck(
                PrivateGlobals,
                FailureCode,
                Parameter2,
                Parameter3);
            break;
        case VerifierAction_DbgBreakIfDebuggerPresent:
            if (!KdRefreshDebuggerNotPresent())
            {

                DbgBreakPoint();
            }
            break;
        default:
            break;
    }
}

void
Verifier_VerifyPrivateGlobals(
    NX_PRIVATE_GLOBALS const * PrivateGlobals
)
{
    if (PrivateGlobals->Signature != NX_PRIVATE_GLOBALS_SIG)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_CorruptedPrivateGlobals,
            0,
            0);
    }
}

void
NetAdapterCxBugCheck(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ FailureCode         FailureCode,
    _In_ ULONG_PTR           Parameter2,
    _In_ ULONG_PTR           Parameter3
)
/*++
Routine Description:
    This function will bugcheck the system using NetAdapterCx bugcode. The first parameter
    is the failure code, followed by parameters 2 and 3 of the bugcheck. Parameter 4 will
    stay reserved in case we decide to create some sort of triage block.
--*/
{
    WdfCxVerifierKeBugCheck(
        PrivateGlobals->NxDriver->GetFxObject(),
        BUGCODE_NETADAPTER_DRIVER,
        FailureCode,
        Parameter2,
        Parameter3,
        0); // Reserved
}

void
Verifier_VerifyIrqlPassive(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals
)
{
    KIRQL currentIrql = KeGetCurrentIrql();

    if (currentIrql != PASSIVE_LEVEL)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_IrqlIsNotPassive,
            currentIrql,
            0);
    }
}

void
Verifier_VerifyIrqlLessThanOrEqualDispatch(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals
)
{
    KIRQL currentIrql = KeGetCurrentIrql();

    if (currentIrql > DISPATCH_LEVEL)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_IrqlNotLessOrEqualDispatch,
            currentIrql,
            0);
    }
}

void
Verifier_VerifyAdapterNotStarted(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NxAdapter *         pNxAdapter
)
{
    if (pNxAdapter->StartCalled())
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_AdapterAlreadyStarted,
            0,
            0);
    }
}

_Use_decl_annotations_
void
Verifier_VerifyNetPowerTransition(
    NX_PRIVATE_GLOBALS const * PrivateGlobals,
    NxDevice *           Device
)
{
    if (!Device->IsDeviceInPowerTransition())
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_PowerNotInTransition,
            0,
            0);
    }
}

void
Verifier_VerifyNetRequestCompletionStatusNotPending(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NETREQUEST NetRequest,
    _In_ NTSTATUS   CompletionStatus
)
{
    if (CompletionStatus == STATUS_PENDING)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_CompletingNetRequestWithPendingStatus,
            (ULONG_PTR)NetRequest,
            0);
    }
}

void
Verifier_VerifyNetRequestType(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NxRequest * NxRequest,
    _In_ NDIS_REQUEST_TYPE Type
)
{
    NT_ASSERT(Type != NdisRequestQueryInformation && Type != NdisRequestQueryStatistics);

    if (NxRequest->m_NdisOidRequest->RequestType != Type)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetRequestType,
            NxRequest->m_NdisOidRequest->RequestType,
            0);
    }
}

void
Verifier_VerifyNetRequestIsQuery(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NxRequest * NxRequest
)
{
    if (NxRequest->m_NdisOidRequest->RequestType != NdisRequestQueryInformation &&
        NxRequest->m_NdisOidRequest->RequestType != NdisRequestQueryStatistics)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetRequestType,
            NxRequest->m_NdisOidRequest->RequestType,
            0);
    }
}

void
Verifier_VerifyNetRequest(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NxRequest * pNxRequest
)
{
    switch (pNxRequest->m_NdisOidRequest->RequestType)
    {
        case NdisRequestSetInformation:
            __fallthrough;
        case NdisRequestQueryInformation:
            __fallthrough;
        case NdisRequestQueryStatistics:
            __fallthrough;
        case NdisRequestMethod:
            break;
        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidNetRequestType,
                pNxRequest->m_NdisOidRequest->RequestType,
                0);
            break;
    }
}

void
Verifier_VerifyNotNull(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ void const * const Ptr
)
{
    if (!Ptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_ParameterCantBeNull,
            0,
            0);
    }
}

NTSTATUS
Verifier_VerifyQueueConfiguration(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_REQUEST_QUEUE_CONFIG * QueueConfig
)
{
    NTSTATUS status = STATUS_SUCCESS;

    //
    // If this checks fail we always bugcheck the system
    //

    if (QueueConfig->Adapter == NULL)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueConfiguration,
            0,
            0);
    }

    auto nxAdapter = GetNxAdapterFromHandle(QueueConfig->Adapter);

    if (QueueConfig->SizeOfSetDataHandler != sizeof(NET_REQUEST_QUEUE_SET_DATA_HANDLER))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueConfiguration,
            QueueConfig->SizeOfSetDataHandler,
            sizeof(NET_REQUEST_QUEUE_SET_DATA_HANDLER));
    }

    if (QueueConfig->SizeOfQueryDataHandler != sizeof(NET_REQUEST_QUEUE_QUERY_DATA_HANDLER))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueConfiguration,
            QueueConfig->SizeOfQueryDataHandler,
            sizeof(NET_REQUEST_QUEUE_QUERY_DATA_HANDLER));
    }

    if (QueueConfig->SizeOfMethodHandler != sizeof(NET_REQUEST_QUEUE_METHOD_HANDLER))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueConfiguration,
            QueueConfig->SizeOfMethodHandler,
            sizeof(NET_REQUEST_QUEUE_METHOD_HANDLER));
    }

    //
    // Currently only DefaultSequential and DefaultParallel Queues can
    // be created, and only one of each can be created per NxAdapter
    // Verify that client isnt creating a duplicate queue.
    //

    switch (QueueConfig->Type)
    {
        case NetRequestQueueDefaultSequential:
            if (nxAdapter->m_DefaultRequestQueue != NULL)
            {
                Verifier_ReportViolation(
                    PrivateGlobals,
                    VerifierAction_BugcheckAlways,
                    FailureCode_DefaultRequestQueueAlreadyExists,
                    NetRequestQueueDefaultSequential,
                    0);
            }
            break;

        case NetRequestQueueDefaultParallel:
            if (nxAdapter->m_DefaultDirectRequestQueue != NULL)
            {
                Verifier_ReportViolation(
                    PrivateGlobals,
                    VerifierAction_BugcheckAlways,
                    FailureCode_DefaultRequestQueueAlreadyExists,
                    NetRequestQueueDefaultParallel,
                    0);
            }
            break;

        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRequestQueueType,
                QueueConfig->Type,
                0);
            break;
    }

    //
    // If these checks fail we return a NTSTATUS code
    //

    //
    // The Request queues allow the client to register handlers for varous requests
    // using the NET_REQUEST_QUEUE_CONFIG_ADD_XXX_HANDLER
    // The registration requires memory allocation and thus can fail. To keep the
    // client API simple, we don't return a failure in those APIs, we just
    // make a note of that error in the AddHandlerError bit field.
    // This routine fails the Queue Creation in case any bit of the
    // AddHandlerError is set.
    //
    if (QueueConfig->AddHandlerError.AsUchar != 0)
    {
        NET_REQUEST_QUEUE_ADD_HANDLER_ERROR error;

        error = QueueConfig->AddHandlerError;

        status = STATUS_UNSUCCESSFUL;

        if (error.AllocationFailed)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_DbgBreakIfDebuggerPresent,
                FailureCode_QueueConfigurationHasError,
                0, // Allocation failed
                0);

            status = STATUS_INSUFFICIENT_RESOURCES;
            error.AllocationFailed = 0;
        }

        if (error.CallbackNull)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_DbgBreakIfDebuggerPresent,
                FailureCode_QueueConfigurationHasError,
                1, // CallbackNull
                0);

            status = STATUS_INVALID_PARAMETER;
            error.CallbackNull = 0;
        }

        if (error.AsUchar != 0)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_DbgBreakIfDebuggerPresent,
                FailureCode_QueueConfigurationHasError,
                2, // Other error
                0);

            status = STATUS_UNSUCCESSFUL;
        }
    }

    return status;
}

VOID
Verifier_VerifyPacketFilter(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_ADAPTER_PACKET_FILTER_CAPABILITIES const * PacketFilter
)
{
    if (PacketFilter->SupportedPacketFilters > 0 && PacketFilter->EvtSetPacketFilter == NULL)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidSetPacketFilterCallBack,
            0,
            (ULONG_PTR)(PacketFilter->EvtSetPacketFilter));
    }

    if (!VERIFIER_CHECK_FLAGS(PacketFilter->SupportedPacketFilters, NET_PACKET_FILTER_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPacketFilterCapabilities,
            0,
            PacketFilter->SupportedPacketFilters);
    }
}

void
Verifier_VerifyLinkLayerAddress(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS *LinkLayerAddress
)
{
    if (LinkLayerAddress->Length > NDIS_MAX_PHYS_ADDRESS_LENGTH)
    {
        // The NIC driver tried to set a link layer address with a length larger
        // than the maximum allowed by NDIS
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_MacAddressLengthTooLong,
            LinkLayerAddress->Length,
            NDIS_MAX_PHYS_ADDRESS_LENGTH);
    }
}

void
Verifier_VerifyCurrentLinkState(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_ADAPTER_LINK_STATE * LinkState
)
{
    switch (LinkState->MediaConnectState)
    {
        case MediaConnectStateUnknown:
            __fallthrough;
        case MediaConnectStateConnected:
            __fallthrough;
        case MediaConnectStateDisconnected:
            break;
        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLinkState,
                0, // What field has invalid value
                (ULONG_PTR)LinkState->MediaConnectState);
            break;
    }

    switch (LinkState->MediaDuplexState)
    {
        case MediaDuplexStateUnknown:
            __fallthrough;
        case MediaDuplexStateHalf:
            __fallthrough;
        case MediaDuplexStateFull:
            break;
        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLinkState,
                1, // What field has invalid value
                (ULONG_PTR)LinkState->MediaDuplexState);
            break;
    }

    switch (LinkState->SupportedPauseFunctions)
    {
        case NetAdapterPauseFunctionTypeUnsupported:
            __fallthrough;
        case NetAdapterPauseFunctionTypeSendOnly:
            __fallthrough;
        case NetAdapterPauseFunctionTypeReceiveOnly:
            __fallthrough;
        case NetAdapterPauseFunctionTypeSendAndReceive:
            __fallthrough;
        case NetAdapterPauseFunctionTypeUnknown:
            break;
        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLinkState,
                2, // What field has invalid value
                (ULONG_PTR)LinkState->SupportedPauseFunctions);
            break;
    }

    if (!VERIFIER_CHECK_FLAGS(LinkState->AutoNegotiationFlags, NDIS_AUTO_NEGOTIATION_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidLinkState,
            3, // What field has invalid value
            (ULONG_PTR)LinkState->AutoNegotiationFlags);
    }
}

void
Verifier_VerifyQueryAsUlongFlags(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_CONFIGURATION_QUERY_ULONG_FLAGS Flags
)
{
    if (!VERIFIER_CHECK_FLAGS(Flags, NET_CONFIGURATION_QUERY_ULONG_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueryUlongFlag,
            Flags,
            0);
    }
}

void
Verifier_VerifyMtuSize(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
         ULONG MtuSize
)
{
    if (MtuSize == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_MtuMustBeGreaterThanZero,
            0,
            0);
    }
}

void
Verifier_VerifyQueueInitContext(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ QUEUE_CREATION_CONTEXT *NetQueueInit
)
{
    if (NetQueueInit->Signature != QUEUE_CREATION_CONTEXT_SIGNATURE)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_BadQueueInitContext,
            0,
            0);
    }

    if (NetQueueInit->CurrentThread != KeGetCurrentThread())
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_CreatingNetQueueFromWrongThread,
            0,
            0);
    }

    if (NetQueueInit->CreatedQueueObject)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_QueueAlreadyCreated,
            (ULONG_PTR)NetQueueInit->CreatedQueueObject.get(),
            0);
    }
}

void
Verifier_VerifyNetPacketQueueConfiguration(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_PACKET_QUEUE_CONFIG const * Configuration
)
{
    // All Evt callbacks are required
    if (Configuration->EvtCancel == nullptr ||
        Configuration->EvtAdvance == nullptr ||
        Configuration->EvtSetNotificationEnabled == nullptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NetQueueInvalidConfiguration,
            0,
            0);
    }
}

void
Verifier_VerifyObjectAttributesParentIsNull(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ WDF_OBJECT_ATTRIBUTES * ObjectAttributes
)
{
    if (ObjectAttributes != nullptr && ObjectAttributes->ParentObject != nullptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_ParentObjectNotNull,
            0,
            0);
    }
}

void
Verifier_VerifyObjectAttributesContextSize(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * ObjectAttributes,
    _In_ SIZE_T MaximumContextSize
)
{
    // FxValidateObjectAttributes checks most problems; we only add a maximum size check.

    size_t requestedSize = 0;

    if (ObjectAttributes != nullptr)
    {
        if (ObjectAttributes->ContextSizeOverride > 0)
        {
            requestedSize = ObjectAttributes->ContextSizeOverride;
        }
        else if (ObjectAttributes->ContextTypeInfo != nullptr)
        {
            requestedSize = ObjectAttributes->ContextTypeInfo->ContextSize;
        }
    }

    if (requestedSize > MaximumContextSize)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_ObjectAttributesContextSizeTooLarge,
            requestedSize,
            MaximumContextSize);
    }
}

void
Verifier_VerifyDatapathCallbacks(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_ADAPTER_DATAPATH_CALLBACKS const *DatapathCallbacks
)
{
    if (DatapathCallbacks->EvtAdapterCreateRxQueue == nullptr ||
        DatapathCallbacks->EvtAdapterCreateTxQueue == nullptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidDatapathCallbacks,
            (ULONG_PTR)DatapathCallbacks,
            0);
    }
}

void
Verifier_VerifyAdapterInitSignature(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ AdapterInit const *AdapterInit
)
{
    if (AdapterInit->InitSignature != ADAPTER_INIT_SIGNATURE)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetAdapterInitSignature,
            (ULONG_PTR)AdapterInit,
            0);
    }
}

void
Verifier_VerifyAdapterExtensionInitSignature(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ AdapterExtensionInit const *AdapterExtensionInit
)
{
    if (AdapterExtensionInit->InitSignature != ADAPTER_EXTENSION_INIT_SIGNATURE)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetAdapterExtensionInitSignature,
            (ULONG_PTR)AdapterExtensionInit,
            0);
    }
}

void
Verifier_VerifyAdapterInitNotUsed(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ AdapterInit const *AdapterInit
)
{
    if (AdapterInit->CreatedAdapter != nullptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NetAdapterInitAlreadyUsed,
            (ULONG_PTR)AdapterInit,
            0);
    }
}

void
Verifier_VerifyReceiveScalingCapabilities(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const * Capabilities
)
{
    if (! RTL_IS_POWER_OF_TWO(Capabilities->IndirectionTableSize))
    {
        Verifier_ReportViolation(PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            Capabilities->IndirectionTableSize,
            0);
    }

    if (! RTL_IS_POWER_OF_TWO(Capabilities->NumberOfQueues))
    {
        Verifier_ReportViolation(PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            Capabilities->NumberOfQueues,
            0);
    }

    if (Capabilities->ReceiveScalingHashTypes >=
        (NetAdapterReceiveScalingHashTypeToeplitz << 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidReceiveScalingHashType,
            Capabilities->ReceiveScalingHashTypes,
            0);
    }

    if (Capabilities->ReceiveScalingProtocolTypes >=
        (NetAdapterReceiveScalingProtocolTypeUdp << 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidReceiveScalingProtocolType,
            Capabilities->ReceiveScalingProtocolTypes,
            0);
    }

    if (Capabilities->ReceiveScalingEncapsulationTypes.Outer >=
        (NetAdapterReceiveScalingEncapsulationTypeVXLan << 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidReceiveScalingEncapsulationType,
            Capabilities->ReceiveScalingEncapsulationTypes.Outer,
            0);
    }

    if (Capabilities->ReceiveScalingEncapsulationTypes.Inner >=
        (NetAdapterReceiveScalingEncapsulationTypeVXLan << 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidReceiveScalingEncapsulationType,
            Capabilities->ReceiveScalingEncapsulationTypes.Inner,
            0);
    }
}

void
Verifier_VerifyNetExtensionName(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ PCWSTR ExtensionName
)
{
    if ((wcsstr(ExtensionName, L"ms_") == ExtensionName))
    {
        auto const isBuiltin =
            wcscmp(ExtensionName, NET_PACKET_EXTENSION_CHECKSUM_NAME) == 0 ||
            wcscmp(ExtensionName, NET_PACKET_EXTENSION_LSO_NAME) == 0 ||
            wcscmp(ExtensionName, NET_PACKET_EXTENSION_RSC_NAME) == 0 ||
            wcscmp(ExtensionName, NET_PACKET_EXTENSION_RSC_TIMESTAMP_NAME) == 0 ||
            wcscmp(ExtensionName, NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME) == 0 ||
            wcscmp(ExtensionName, NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME) == 0 ||
            wcscmp(ExtensionName, NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_NAME) == 0 ||
            wcscmp(ExtensionName, NET_FRAGMENT_EXTENSION_MDL_NAME) == 0;

        if (! isBuiltin)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidNetExtensionName,
                (ULONG_PTR)ExtensionName,
                0);
        }
    }
}

void
Verifier_VerifyNetExtensionQuery(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_EXTENSION_QUERY const * Query
)
{
    if ((Query->Name == nullptr) ||
        (Query->Name[0] == L'\0'))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetExtensionName,
            (ULONG_PTR)Query,
            0);
    }

    Verifier_VerifyNetExtensionName(PrivateGlobals, Query->Name);

    if (Query->Version == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetExtensionVersion,
            (ULONG_PTR)Query,
            0);
    }

    switch (Query->Type)
    {

    case NetExtensionTypePacket:
        break;

    case NetExtensionTypeFragment:
        break;

    default:
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetExtensionType,
            reinterpret_cast<ULONG_PTR>(Query),
            0);
        break;
    }
}

void
Verifier_VerifyNetAdapterTxCapabilities(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ const NET_ADAPTER_TX_CAPABILITIES *TxCapabilities
)
{
    // If the mapping requirement is DMA, the adapter has to provide the DMA capabilities
    if (TxCapabilities->MappingRequirement == NetMemoryMappingRequirementDmaMapped &&
        ((TxCapabilities->DmaCapabilities == nullptr) || (TxCapabilities->DmaCapabilities->DmaEnabler == nullptr)))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterTxCapabilities,
            (ULONG_PTR)TxCapabilities,
            1);
    }

    // If the mapping requirement is DMA, the adapter has to provide the DMA v3 inteface
    if (TxCapabilities->MappingRequirement == NetMemoryMappingRequirementDmaMapped)
    {
        PDMA_ADAPTER dmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(TxCapabilities->DmaCapabilities->DmaEnabler,
                                          WdfDmaDirectionWriteToDevice);

        if (dmaAdapter->DmaOperations->GetDmaAdapterInfo == nullptr)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidAdapterTxCapabilities,
                (ULONG_PTR) TxCapabilities,
                2);
        }
    }

    if (TxCapabilities->MaximumNumberOfQueues == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterTxCapabilities,
            (ULONG_PTR)TxCapabilities,
            3);
    }

    // A valid fragment alignment requirement is either the default value or in the format (2^n)
    if (TxCapabilities->FragmentBufferAlignment != NET_ADAPTER_FRAGMENT_DEFAULT_ALIGNMENT &&
        !RTL_IS_POWER_OF_TWO(TxCapabilities->FragmentBufferAlignment))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterTxCapabilities,
            (ULONG_PTR)TxCapabilities,
            4);
    }

    if ((TxCapabilities->FragmentRingNumberOfElementsHint > 0) &&
        !RTL_IS_POWER_OF_TWO(TxCapabilities->FragmentRingNumberOfElementsHint))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            TxCapabilities->FragmentRingNumberOfElementsHint,
            5);
    }
}

void
Verifier_VerifyNetAdapterRxCapabilities(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ const NET_ADAPTER_RX_CAPABILITIES *RxCapabilities
)
{
    // If the adapter does not ask the OS to allocate receive fragments, then
    // EvtAdapterReturnRxBuffer must be set
    if (RxCapabilities->AllocationMode == NetRxFragmentBufferAllocationModeDriver)
    {
        if ((RxCapabilities->AttachmentMode != NetRxFragmentBufferAttachmentModeDriver) ||
            (RxCapabilities->EvtAdapterReturnRxBuffer == nullptr))
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidAdapterRxCapabilities,
                (ULONG_PTR) RxCapabilities,
                0);
        }
    }

    // If the mapping requirement is DMA, the adapter has to provide the DMA capabilities
    if (RxCapabilities->MappingRequirement == NetMemoryMappingRequirementDmaMapped &&
        ((RxCapabilities->DmaCapabilities == nullptr) || (RxCapabilities->DmaCapabilities->DmaEnabler == nullptr)))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterRxCapabilities,
            (ULONG_PTR)RxCapabilities,
            1);
    }

    // If the mapping requirement is DMA, the adapter has to provide the DMA v3 inteface
    if (RxCapabilities->MappingRequirement == NetMemoryMappingRequirementDmaMapped)
    {
        PDMA_ADAPTER dmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(RxCapabilities->DmaCapabilities->DmaEnabler,
                                          WdfDmaDirectionReadFromDevice);

        if (dmaAdapter->DmaOperations->GetDmaAdapterInfo == nullptr)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidAdapterRxCapabilities,
                (ULONG_PTR) RxCapabilities,
                2);
        }
    }

    if (RxCapabilities->MaximumNumberOfQueues == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterRxCapabilities,
            (ULONG_PTR)RxCapabilities,
            3);
    }

    // A valid fragment alignment requirement is either the default value or in the format (2^n)
    if (RxCapabilities->FragmentBufferAlignment != NET_ADAPTER_FRAGMENT_DEFAULT_ALIGNMENT &&
        !RTL_IS_POWER_OF_TWO(RxCapabilities->FragmentBufferAlignment))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            RxCapabilities->FragmentBufferAlignment,
            4);
    }

    if ((RxCapabilities->FragmentRingNumberOfElementsHint > 0) &&
        !RTL_IS_POWER_OF_TWO(RxCapabilities->FragmentRingNumberOfElementsHint))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            RxCapabilities->FragmentRingNumberOfElementsHint,
            5);
    }
}

void
Verifier_VerifyDeviceAdapterCollectionIsEmpty(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NxDevice const *Device,
    _In_ NxAdapterCollection const *AdapterCollection
)
{
    if (AdapterCollection->Count() > 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_RemovingDeviceWithAdapters,
            reinterpret_cast<ULONG_PTR>(Device),
            reinterpret_cast<ULONG_PTR>(AdapterCollection));
    }
}

void
Verifier_VerifyLsoCapabilities(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES const *LsoCapabilities
)
{
    if (LsoCapabilities->IPv4 || LsoCapabilities->IPv6)
    {
        if (LsoCapabilities->MaximumOffloadSize == 0)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLsoCapabilities,
                LsoCapabilities->MaximumOffloadSize,
                1);
        }

        if (LsoCapabilities->MinimumSegmentCount == 0)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLsoCapabilities,
                LsoCapabilities->MinimumSegmentCount,
                2);
        }
    }
}

void
Verifier_VerifyExtensionGlobals(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals
)
{
    Verifier_VerifyPrivateGlobals(PrivateGlobals);

    if (PrivateGlobals->ExtensionType == MediaExtensionType::None)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_IllegalPrivateApiCall,
            0,
            0);
    }
}

void
Verifier_VerifyRxQueueHandle(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NETPACKETQUEUE NetRxQueue
)
{
    if (!GetRxQueueFromHandle(NetRxQueue))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueHandle,
            0,
            0);
    }
}

void
Verifier_VerifyTxQueueHandle(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NETPACKETQUEUE NetTxQueue
)
{
    if (!GetTxQueueFromHandle(NetTxQueue))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueHandle,
            1,
            0);
    }
}

void
Verifier_VerifyPowerOffloadList(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_POWER_OFFLOAD_LIST const * List
)
{
    auto powerList = reinterpret_cast<NxPowerList const *>(&List->Reserved[0]);

    if (powerList->Signature != NX_POWER_OFFLOAD_LIST_SIGNATURE)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerOffloadListSignature,
            (ULONG_PTR)powerList,
            0);
    }
}


void
Verifier_VerifyWakeSourceList(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_WAKE_SOURCE_LIST const * List
)
{
    auto powerList = reinterpret_cast<NxPowerList const *>(&List->Reserved[0]);

    if (powerList->Signature != NX_WAKE_SOURCE_LIST_SIGNATURE)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidWakeSourceListSignature,
            (ULONG_PTR)powerList,
            0);
    }
}

static
void
VerifyRingImmutableDriverPacket(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_PACKET const * const PacketBefore,
    NET_PACKET const * const PacketAfter,
    FailureCode Code
)
{
    auto const size = offsetof(NET_PACKET, Layout) + sizeof(PacketBefore->Layout);
    auto const unmodified = RtlEqualMemory(PacketBefore, PacketAfter, size)
        && PacketBefore->Reserved1 == PacketAfter->Reserved1;

    if (! unmodified)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            Code,
            reinterpret_cast<ULONG_PTR>(PacketBefore),
            reinterpret_cast<ULONG_PTR>(PacketAfter));
    }
}

_Use_decl_annotations_
void
Verifier_VerifyRingImmutable(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_RING const & Before,
    NET_RING const & After
)
{
    if (! RtlEqualMemory(&Before, &After, offsetof(NET_RING, BeginIndex)))
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingImmutable,
            reinterpret_cast<ULONG_PTR>(&Before),
            reinterpret_cast<ULONG_PTR>(&After));
    }
}

_Use_decl_annotations_
void
Verifier_VerifyRingImmutableDriverTxPackets(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_RING const & Before,
    NET_RING const & After
)
{
    // Tx only NET_PACKET.Scratch may be modified

    for (auto beginIndex = Before.BeginIndex; beginIndex != Before.EndIndex;
        beginIndex = NetRingIncrementIndex(&Before, beginIndex))
    {
        auto const packetBefore = NetRingGetPacketAtIndex(&Before, beginIndex);
        auto const packetAfter = NetRingGetPacketAtIndex(&After, beginIndex);

        VerifyRingImmutableDriverPacket(
            PrivateGlobals,
            packetBefore,
            packetAfter,
            FailureCode_InvalidRingImmutableDriverTxPacket);

        if (packetBefore->Ignore != packetAfter->Ignore)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingImmutableDriverTxPacket,
                reinterpret_cast<ULONG_PTR>(packetBefore),
                reinterpret_cast<ULONG_PTR>(packetAfter));
        }
    }
}

_Use_decl_annotations_
void
Verifier_VerifyRingImmutableDriverTxFragments(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_RING const & Before,
    NET_RING const & After
)
{
    // Tx only NET_FRAGMENT.Scratch may be modified

    for (auto beginIndex = Before.BeginIndex; beginIndex != Before.EndIndex;
        beginIndex = NetRingIncrementIndex(&Before, beginIndex))
    {
        auto const fragmentBefore = NetRingGetFragmentAtIndex(&Before, beginIndex);
        auto const fragmentAfter = NetRingGetFragmentAtIndex(&After, beginIndex);

        auto const unmodified = fragmentBefore->ValidLength == fragmentAfter->ValidLength
            && fragmentBefore->Capacity == fragmentAfter->Capacity
            && fragmentBefore->Offset == fragmentAfter->Offset
            && fragmentBefore->OsReserved_Bounced == fragmentAfter->OsReserved_Bounced;

        if (! unmodified)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingImmutableDriverTxFragment,
                reinterpret_cast<ULONG_PTR>(fragmentBefore),
                reinterpret_cast<ULONG_PTR>(fragmentAfter));
        }
    }
}

_Use_decl_annotations_
void
Verifier_VerifyRingBeginIndex(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_RING const & Before,
    NET_RING const & After
)
{
    // Rx/Tx BeginIndex shall not be modified reference outside driver range

    if (NetRingGetRangeCount(&Before, Before.BeginIndex, Before.EndIndex)  <
        NetRingGetRangeCount(&After, After.BeginIndex, After.EndIndex))
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingBeginIndex,
            reinterpret_cast<ULONG_PTR>(&Before),
            reinterpret_cast<ULONG_PTR>(&After));
    }
}

_Use_decl_annotations_
void
Verifier_VerifyRingImmutableFrameworkElements(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_RING const & Before,
    NET_RING const & After
)
{
    // Rx/Tx NET_{PACKET,FRAGMENT} elements shall not be modified outside driver range

    auto const endIndex = NetRingAdvanceIndex(&Before, Before.BeginIndex, -1);
    for (auto beginIndex = Before.EndIndex; beginIndex != endIndex;
        beginIndex = NetRingIncrementIndex(&Before, beginIndex))
    {
        auto const elementBefore = NetRingGetElementAtIndex(&Before, beginIndex);
        auto const elementAfter = NetRingGetElementAtIndex(&After, beginIndex);

        if (! RtlEqualMemory(elementBefore, elementAfter, Before.ElementStride))
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingImmutableFrameworkElement,
                reinterpret_cast<ULONG_PTR>(elementBefore),
                reinterpret_cast<ULONG_PTR>(elementAfter));
        }
    }
}

static
bool
NetRingIndexInRange(
    _In_ NET_RING const & Ring,
    _In_ UINT32 Index,
    _In_ UINT32 BeginIndex,
    _In_ UINT32 EndIndex
)
{
    if (BeginIndex <= EndIndex)
    {
        return BeginIndex <= Index && Index < EndIndex;
    }

    return 0 <= Index && Index < EndIndex
        || BeginIndex <= Index && Index <= Ring.ElementIndexMask;
}

static
bool
NetRingIndexLess(
    _In_ NET_RING const & Ring,
    _In_ UINT32 Lhs,
    _In_ UINT32 Rhs
)
{
    auto const lhs = Lhs < Ring.BeginIndex ? Lhs + Ring.NumberOfElements : Lhs;
    auto const rhs = Rhs < Ring.BeginIndex ? Rhs + Ring.NumberOfElements : Rhs;

    return lhs < rhs;
}

static
void
VerifyPacketFields(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_RING const & FragmentRingBefore,
    NET_RING const & FragmentRingAfter,
    NET_PACKET const * const Packet
)
{
    // driver cannot return packets with 0 fragments.

    if (Packet->FragmentCount == 0)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketFragmentCountZero,
            reinterpret_cast<ULONG_PTR>(Packet),
            0);
    }

    // driver cannot return packets with more fragments in range. this
    // check is necessary still because the checks that follow won't catch
    // when index and index+count fall in range but get reversed.

    if (Packet->FragmentCount > NetRingGetRangeCount(
        &FragmentRingBefore,
        FragmentRingBefore.BeginIndex,
        FragmentRingBefore.EndIndex))
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketFragmentCountOutOfRange,
            reinterpret_cast<ULONG_PTR>(Packet),
            reinterpret_cast<ULONG_PTR>(&FragmentRingBefore));
    }

    // before

    auto const firstIndexInBeforeRange = NetRingIndexInRange(
        FragmentRingBefore,
        Packet->FragmentIndex,
        FragmentRingBefore.BeginIndex,
        FragmentRingBefore.EndIndex);

    auto const lastIndexInBeforeRange = NetRingIndexInRange(
        FragmentRingBefore,
        NetRingAdvanceIndex(&FragmentRingBefore, Packet->FragmentIndex, Packet->FragmentCount - 1),
        FragmentRingBefore.BeginIndex,
        FragmentRingBefore.EndIndex);

    // packet references an fragment index out of range before being called.

    if (! firstIndexInBeforeRange)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketFragmentIndexOutOfPreRange,
            reinterpret_cast<ULONG_PTR>(Packet),
            reinterpret_cast<ULONG_PTR>(&FragmentRingBefore));
    }

    // packet references a fragment index out of range before being called.

    if (! lastIndexInBeforeRange)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketFragmentCountOutOfPreRange,
            reinterpret_cast<ULONG_PTR>(Packet),
            reinterpret_cast<ULONG_PTR>(&FragmentRingBefore));
    }

    // after

    auto const firstIndexInAfterRange = NetRingIndexInRange(
        FragmentRingAfter,
        Packet->FragmentIndex,
        FragmentRingAfter.BeginIndex,
        FragmentRingAfter.EndIndex);

    auto const lastIndexInAfterRange = NetRingIndexInRange(
        FragmentRingAfter,
        NetRingAdvanceIndex(&FragmentRingAfter, Packet->FragmentIndex, Packet->FragmentCount - 1),
        FragmentRingAfter.BeginIndex,
        FragmentRingAfter.EndIndex);

    // packet references an fragment index out of range after being called.

    if (firstIndexInAfterRange)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketFragmentIndexOutOfPostRange,
            reinterpret_cast<ULONG_PTR>(Packet),
            reinterpret_cast<ULONG_PTR>(&FragmentRingAfter));
    }

    // packet references a fragment index out of range after being called.

    if (lastIndexInAfterRange)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketFragmentCountOutOfPostRange,
            reinterpret_cast<ULONG_PTR>(Packet),
            reinterpret_cast<ULONG_PTR>(&FragmentRingAfter));
    }

    auto const layer2TypeValid = NetPacketLayer2TypeUnspecified <= Packet->Layout.Layer2Type
        && Packet->Layout.Layer2Type <= NetPacketLayer2TypeEthernet;
    if (! layer2TypeValid)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketLayer2Type,
            reinterpret_cast<ULONG_PTR>(Packet),
            0);
    }

    auto const layer3TypeValid = NetPacketLayer3TypeUnspecified <= Packet->Layout.Layer3Type
        && Packet->Layout.Layer3Type <= NetPacketLayer3TypeIPv6NoExtensions;
    if (! layer3TypeValid)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketLayer3Type,
            reinterpret_cast<ULONG_PTR>(Packet),
            0);
    }

    auto const layer4TypeValid = NetPacketLayer4TypeUnspecified <= Packet->Layout.Layer4Type
        && Packet->Layout.Layer4Type <= NetPacketLayer4TypeIPNotFragment;
    if (! layer4TypeValid)
    {
        Verifier_ReportViolation(
            &PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRingRxPacketLayer4Type,
            reinterpret_cast<ULONG_PTR>(Packet),
            0);
    }

    switch (Packet->Layout.Layer2Type)
    {

    case NetPacketLayer2TypeNull:
        if (Packet->Layout.Layer2HeaderLength != 0)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingRxPacketLayer2TypeNullLengthNonZero,
                reinterpret_cast<ULONG_PTR>(Packet),
                0);
        }

        break;

    case NetPacketLayer2TypeEthernet:
        if (0 != Packet->Layout.Layer2HeaderLength
            && Packet->Layout.Layer2HeaderLength < 14)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingRxPacketLayer2TypeEthernetLengthInvalid,
                reinterpret_cast<ULONG_PTR>(Packet),
                0);
        }
    }

    if (NetPacketIsIpv4(Packet))
    {
        if (0 != Packet->Layout.Layer3HeaderLength
            && Packet->Layout.Layer3HeaderLength < 20)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingRxPacketLayer3TypeIPv4LengthInvalid,
                reinterpret_cast<ULONG_PTR>(Packet),
                0);
        }
    }

    if (NetPacketIsIpv6(Packet))
    {
        if (0 != Packet->Layout.Layer3HeaderLength
            && Packet->Layout.Layer3HeaderLength < 40)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingRxPacketLayer3TypeIPv6LengthInvalid,
                reinterpret_cast<ULONG_PTR>(Packet),
                0);
        }
    }

    if (0 != Packet->Layout.Layer4HeaderLength)
    {
        if (Packet->Layout.Layer4Type == NetPacketLayer4TypeTcp
            && Packet->Layout.Layer4HeaderLength < 40)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingRxPacketLayer4TypeTcpLengthInvalid,
                reinterpret_cast<ULONG_PTR>(Packet),
                0);
        }

        if (Packet->Layout.Layer4Type == NetPacketLayer4TypeUdp
            && Packet->Layout.Layer4HeaderLength < 8)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingRxPacketLayer4TypeUdpLengthInvalid,
                reinterpret_cast<ULONG_PTR>(Packet),
                0);
        }
    }
}

static
void
VerifyPacketFragmentsRx(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_PACKET const * Packet,
    bool Attaching,
    NET_RING const & FragmentRingBefore,
    NET_RING const & FragmentRingAfter
)
{
    for (UINT32 i = 0; i != Packet->FragmentCount; i++)
    {
        auto const index = NetRingAdvanceIndex(&FragmentRingBefore, Packet->FragmentIndex, i);
        auto const fragmentBefore = NetRingGetFragmentAtIndex(&FragmentRingBefore, index);
        auto const fragmentAfter = NetRingGetFragmentAtIndex(&FragmentRingAfter, index);

        auto const unmodified = Attaching ?
            fragmentBefore->OsReserved_Bounced == fragmentAfter->OsReserved_Bounced :
            fragmentBefore->OsReserved_Bounced == fragmentAfter->OsReserved_Bounced
                && fragmentBefore->Capacity == fragmentAfter->Capacity;

        if (! unmodified)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingUnmodifiedRxFragment,
                reinterpret_cast<ULONG_PTR>(fragmentBefore),
                reinterpret_cast<ULONG_PTR>(fragmentAfter));
        }

        auto const modified = Attaching ?
            fragmentBefore->ValidLength != fragmentAfter->ValidLength
                && fragmentBefore->Capacity != fragmentAfter->Capacity
                && fragmentBefore->Offset != fragmentAfter->Offset :
            fragmentBefore->ValidLength != fragmentAfter->ValidLength
                && fragmentBefore->Offset != fragmentAfter->Offset;

        if (! modified)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingModifiedRxFragment,
                reinterpret_cast<ULONG_PTR>(fragmentBefore),
                reinterpret_cast<ULONG_PTR>(fragmentAfter));
        }

        if (! (fragmentAfter->Offset < fragmentAfter->ValidLength))
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingRxPacketFragmentOffsetInvalid,
                reinterpret_cast<ULONG_PTR>(fragmentAfter),
                0);
        }

        if (! (fragmentAfter->Offset + fragmentAfter->ValidLength <= fragmentAfter->Capacity))
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingRxPacketFragmentLengthInvalid,
                reinterpret_cast<ULONG_PTR>(fragmentAfter),
                0);
        }
    }
}

_Use_decl_annotations_
void
Verifier_VerifyRingImmutableDriverRx(
    NX_PRIVATE_GLOBALS const & PrivateGlobals,
    NET_ADAPTER_RX_CAPABILITIES const & RxCapabilities,
    NET_RING const & PacketRingBefore,
    NET_RING const & PacketRingAfter,
    NET_RING const & FragmentRingBefore,
    NET_RING const & FragmentRingAfter
)
{

    bool returnedRange = true;

    for (auto beginIndex = PacketRingBefore.BeginIndex; beginIndex != PacketRingBefore.EndIndex;
        beginIndex = NetRingIncrementIndex(&PacketRingBefore, beginIndex))
    {
        auto const packetBefore = NetRingGetPacketAtIndex(&PacketRingBefore, beginIndex);
        auto const packetAfter = NetRingGetPacketAtIndex(&PacketRingAfter, beginIndex);

        if (packetAfter->Ignore)
        {
            // ignored packets shall not be modified (except to set Ignore = 1)

            VerifyRingImmutableDriverPacket(
                PrivateGlobals,
                packetBefore,
                packetAfter,
                FailureCode_InvalidRingImmutableDriverRxPacketIgnore);

            continue;
        }

        // Reserved fields shall not be modified, even for unreturned packets

        auto const unmodified = packetBefore->Reserved1 == packetAfter->Reserved1
            && packetBefore->Layout.Reserved0 == packetAfter->Layout.Reserved0;
        if (! unmodified)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingModifiedRxPacket,
                reinterpret_cast<ULONG_PTR>(packetBefore),
                reinterpret_cast<ULONG_PTR>(packetAfter));
        }

        if (beginIndex == PacketRingAfter.BeginIndex)
        {
            returnedRange = false;
        }

        if (! returnedRange)
        {
            continue;
        }

        // verification beyond this point is only on returned range

        // FragmentIndex, FragmentCount, Layout.Layer[234]Type shall be modified

        auto const modified = packetBefore->FragmentIndex != packetAfter->FragmentIndex
            && packetBefore->FragmentCount != packetAfter->FragmentCount
            && packetBefore->Layout.Layer2Type != packetAfter->Layout.Layer2Type
            && packetBefore->Layout.Layer3Type != packetAfter->Layout.Layer3Type
            && packetBefore->Layout.Layer4Type != packetAfter->Layout.Layer4Type;

        if (! modified)
        {
            Verifier_ReportViolation(
                &PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRingUnmodifiedRxPacket,
                reinterpret_cast<ULONG_PTR>(packetBefore),
                reinterpret_cast<ULONG_PTR>(packetAfter));
        }

        VerifyPacketFields(
            PrivateGlobals,
            FragmentRingBefore,
            FragmentRingAfter,
            packetAfter);

        // verify packet references to fragment indexes ascend in value and do not overlap

        if (beginIndex != PacketRingBefore.BeginIndex)
        {
            auto const indexPrev = NetRingAdvanceIndex(&PacketRingAfter, beginIndex, - 1);
            auto const packetPrev = NetRingGetPacketAtIndex(&PacketRingAfter, indexPrev);

            auto const prevIndexLess = NetRingIndexLess(
                FragmentRingAfter,
                packetPrev->FragmentIndex,
                packetAfter->FragmentIndex);

            auto const prevCountLess = NetRingIndexLess(
                FragmentRingAfter,
                NetRingAdvanceIndex(
                    &FragmentRingAfter,
                    packetPrev->FragmentIndex,
                    packetPrev->FragmentCount - 1),
                packetAfter->FragmentIndex);

            if (! prevIndexLess)
            {
                Verifier_ReportViolation(
                    &PrivateGlobals,
                    VerifierAction_BugcheckAlways,
                    FailureCode_InvalidRingRxPacketFragmentIndexOverlap,
                    reinterpret_cast<ULONG_PTR>(packetPrev),
                    reinterpret_cast<ULONG_PTR>(packetAfter));
            }

            if (! prevCountLess)
            {
                Verifier_ReportViolation(
                    &PrivateGlobals,
                    VerifierAction_BugcheckAlways,
                    FailureCode_InvalidRingRxPacketFragmentCountOverlap,
                    reinterpret_cast<ULONG_PTR>(packetPrev),
                    reinterpret_cast<ULONG_PTR>(packetAfter));
            }
        }

        VerifyPacketFragmentsRx(
            PrivateGlobals,
            packetAfter,
            RxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver,
            FragmentRingBefore,
            FragmentRingAfter);
    }
}

void
Verifier_VerifyNdisPmCapabilities(
    _In_ NX_PRIVATE_GLOBALS const * PrivateGlobals,
    _In_ NET_ADAPTER_NDIS_PM_CAPABILITIES const * NdisPmCapabilities
)
{
    auto const & mediaSpecificFlags = NdisPmCapabilities->MediaSpecificWakeUpEvents;

    switch (PrivateGlobals->ExtensionType)
    {
        case MediaExtensionType::Mbb:

            if(!VERIFIER_CHECK_FLAGS(mediaSpecificFlags, NDIS_MBB_WAKEUP_SUPPORTED_FLAGS))
            {
                Verifier_ReportViolation(
                    PrivateGlobals,
                    VerifierAction_BugcheckAlways,
                    FailureCode_InvalidMediaSpecificWakeUpFlags,
                    static_cast<ULONG_PTR>(PrivateGlobals->ExtensionType),
                    mediaSpecificFlags);
            }
            break;

        case MediaExtensionType::Wifi:

            if(!VERIFIER_CHECK_FLAGS(mediaSpecificFlags, NDIS_WIFI_WAKEUP_SUPPORTED_FLAGS))
            {
                Verifier_ReportViolation(
                    PrivateGlobals,
                    VerifierAction_BugcheckAlways,
                    FailureCode_InvalidMediaSpecificWakeUpFlags,
                    static_cast<ULONG_PTR>(PrivateGlobals->ExtensionType),
                    mediaSpecificFlags);
            }
            break;
    }
}
