// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:
    Contains definitions used by NetAdapterCx to detect and report violations

--*/

#pragma once

struct AdapterExtensionInit;
struct AdapterInit;
struct QUEUE_CREATION_CONTEXT;
struct NX_PRIVATE_GLOBALS;

class NxAdapter;
class NxAdapterCollection;
class NxDevice;
class NxRequest;
class NxWake;

//
// NET_*_SUPPORTED_FLAGS are used to check if a client is passing valid flags to NetAdapterCx APIs
//

// There is not yet a public NDIS_PM or NET_ADAPTER_POWER flag for NDIS_PM_AOAC_NAPS_SUPPORTED, but it's used in
// test code. So for now, just using that private define, but at some point we need to figure out the right thing to do here.
#define NET_ADAPTER_POWER_CAPABILITIES_SUPPORTED_FLAGS (NET_ADAPTER_POWER_WAKE_PACKET_INDICATION                    | \
                                                        NET_ADAPTER_POWER_SELECTIVE_SUSPEND                         | \
                                                        NDIS_PM_AOAC_NAPS_SUPPORTED)

#define NET_ADAPTER_PROTOCOL_OFFLOADS_SUPPORTED_FLAGS  (NET_ADAPTER_PROTOCOL_OFFLOAD_ARP                            | \
                                                        NET_ADAPTER_PROTOCOL_OFFLOAD_NS                             | \
                                                        NET_ADAPTER_PROTOCOL_OFFLOAD_80211_RSN_REKEY)

#define NET_ADAPTER_WAKEUP_SUPPORTED_FLAGS             (NET_ADAPTER_WAKE_ON_MEDIA_CONNECT                           | \
                                                        NET_ADAPTER_WAKE_ON_MEDIA_DISCONNECT)

#define NET_ADAPTER_WAKEUP_MEDIA_SPECIFIC_SUPPORTED_FLAGS (NET_ADAPTER_WLAN_WAKE_ON_NLO_DISCOVERY                   | \
                                                           NET_ADAPTER_WLAN_WAKE_ON_AP_ASSOCIATION_LOST             | \
                                                           NET_ADAPTER_WLAN_WAKE_ON_GTK_HANDSHAKE_ERROR             | \
                                                           NET_ADAPTER_WLAN_WAKE_ON_4WAY_HANDSHAKE_REQUEST          | \
                                                           NET_ADAPTER_WWAN_WAKE_ON_REGISTER_STATE                  | \
                                                           NET_ADAPTER_WWAN_WAKE_ON_SMS_RECEIVE                     | \
                                                           NET_ADAPTER_WWAN_WAKE_ON_USSD_RECEIVE                    | \
                                                           NET_ADAPTER_WWAN_WAKE_ON_PACKET_STATE                    | \
                                                           NET_ADAPTER_WWAN_WAKE_ON_UICC_CHANGE)

#define NET_ADAPTER_WAKE_SUPPORTED_FLAGS               (NET_ADAPTER_WAKE_BITMAP_PATTERN                              | \
                                                        NET_ADAPTER_WAKE_MAGIC_PACKET                                | \
                                                        NET_ADAPTER_WAKE_IPV4_TCP_SYN                                | \
                                                        NET_ADAPTER_WAKE_IPV6_TCP_SYN                                | \
                                                        NET_ADAPTER_WAKE_IPV4_DEST_ADDR_WILDCARD                     | \
                                                        NET_ADAPTER_WAKE_IPV6_DEST_ADDR_WILDCARD                     | \
                                                        NET_ADAPTER_WAKE_EAPOL_REQUEST_ID_MESSAGE)

#define NET_ADAPTER_STATISTICS_SUPPORTED_FLAGS         (NET_ADAPTER_STATISTICS_XMIT_OK                              | \
                                                        NET_ADAPTER_STATISTICS_RCV_OK                               | \
                                                        NET_ADAPTER_STATISTICS_XMIT_ERROR                           | \
                                                        NET_ADAPTER_STATISTICS_RCV_ERROR                            | \
                                                        NET_ADAPTER_STATISTICS_RCV_NO_BUFFER                        | \
                                                        NET_ADAPTER_STATISTICS_DIRECTED_BYTES_XMIT                  | \
                                                        NET_ADAPTER_STATISTICS_DIRECTED_FRAMES_XMIT                 | \
                                                        NET_ADAPTER_STATISTICS_MULTICAST_BYTES_XMIT                 | \
                                                        NET_ADAPTER_STATISTICS_MULTICAST_FRAMES_XMIT                | \
                                                        NET_ADAPTER_STATISTICS_BROADCAST_BYTES_XMIT                 | \
                                                        NET_ADAPTER_STATISTICS_BROADCAST_FRAMES_XMIT                | \
                                                        NET_ADAPTER_STATISTICS_DIRECTED_BYTES_RCV                   | \
                                                        NET_ADAPTER_STATISTICS_DIRECTED_FRAMES_RCV                  | \
                                                        NET_ADAPTER_STATISTICS_MULTICAST_BYTES_RCV                  | \
                                                        NET_ADAPTER_STATISTICS_MULTICAST_FRAMES_RCV                 | \
                                                        NET_ADAPTER_STATISTICS_BROADCAST_BYTES_RCV                  | \
                                                        NET_ADAPTER_STATISTICS_BROADCAST_FRAMES_RCV                 | \
                                                        NET_ADAPTER_STATISTICS_RCV_CRC_ERROR                        | \
                                                        NET_ADAPTER_STATISTICS_TRANSMIT_QUEUE_LENGTH                | \
                                                        NET_ADAPTER_STATISTICS_BYTES_RCV                            | \
                                                        NET_ADAPTER_STATISTICS_BYTES_XMIT                           | \
                                                        NET_ADAPTER_STATISTICS_RCV_DISCARDS                         | \
                                                        NET_ADAPTER_STATISTICS_GEN_STATISTICS                       | \
                                                        NET_ADAPTER_STATISTICS_XMIT_DISCARDS)                         \

#define NET_PACKET_FILTER_SUPPORTED_FLAGS              (NET_PACKET_FILTER_TYPE_DIRECTED                             | \
                                                        NET_PACKET_FILTER_TYPE_MULTICAST                            | \
                                                        NET_PACKET_FILTER_TYPE_ALL_MULTICAST                        | \
                                                        NET_PACKET_FILTER_TYPE_BROADCAST                            | \
                                                        NET_PACKET_FILTER_TYPE_SOURCE_ROUTING                       | \
                                                        NET_PACKET_FILTER_TYPE_PROMISCUOUS                          | \
                                                        NET_PACKET_FILTER_TYPE_ALL_LOCAL                            | \
                                                        NET_PACKET_FILTER_TYPE_MAC_FRAME                            | \
                                                        NET_PACKET_FILTER_TYPE_NO_LOCAL)

#define NDIS_AUTO_NEGOTIATION_SUPPORTED_FLAGS          (NET_ADAPTER_AUTO_NEGOTIATION_NO_FLAGS                       | \
                                                        NET_ADAPTER_LINK_STATE_XMIT_LINK_SPEED_AUTO_NEGOTIATED      | \
                                                        NET_ADAPTER_LINK_STATE_RCV_LINK_SPEED_AUTO_NEGOTIATED       | \
                                                        NET_ADAPTER_LINK_STATE_DUPLEX_AUTO_NEGOTIATED               | \
                                                        NET_ADAPTER_LINK_STATE_PAUSE_FUNCTIONS_AUTO_NEGOTIATED)       \

#define NET_CONFIGURATION_QUERY_ULONG_SUPPORTED_FLAGS  (NET_CONFIGURATION_QUERY_ULONG_NO_FLAGS                      | \
                                                        NET_CONFIGURATION_QUERY_ULONG_MAY_BE_STORED_AS_HEX_STRING)    \

//
// This macro is used to check if an input flag mask contains only allowed flag values, as defined by _supported
//
#define VERIFIER_CHECK_FLAGS(_flags, _supported) (((_flags) & ~(_supported)) == 0)

//
// NetAdapterCx failure codes
//
typedef enum _FailureCode : ULONG_PTR
{
    FailureCode_CorruptedPrivateGlobals = 0,
    FailureCode_IrqlIsNotPassive,
    FailureCode_IrqlNotLessOrEqualDispatch,
    FailureCode_AdapterAlreadyStarted,
    FailureCode_EvtArmDisarmWakeNotInProgress,
    FailureCode_CompletingNetRequestWithPendingStatus,
    FailureCode_InvalidNetRequestType,
    FailureCode_DefaultRequestQueueAlreadyExists,
    FailureCode_InvalidStructTypeSize,
    FailureCode_InvalidQueueConfiguration,
    FailureCode_InvalidPowerCapabilities,
    FailureCode_MacAddressLengthTooLong,
    FailureCode_InvalidLinkLayerCapabilities,
    FailureCode_InvalidLinkState,
    FailureCode_ObjectIsNotCancelable,
    FailureCode_ParameterCantBeNull,
    FailureCode_InvalidQueryUlongFlag,
    FailureCode_QueryNetworkAddressInvalidParameter,
    FailureCode_QueueConfigurationHasError,
    FailureCode_InvalidRequestQueueType,
    FailureCode_NetPacketContextTypeMismatch,
    FailureCode_NetPacketDoesNotHaveContext,
    FailureCode_MtuMustBeGreaterThanZero,
    FailureCode_BadQueueInitContext,
    FailureCode_CreatingNetQueueFromWrongThread,
    FailureCode_InvalidDatapathCapabilities,
    FailureCode_NetQueueInvalidConfiguration,
    FailureCode_ParentObjectNotNull,
    FailureCode_InvalidNetAdapterConfig,
    FailureCode_QueueAlreadyCreated,
    FailureCode_ObjectAttributesContextSizeTooLarge,
    FailureCode_NotPowerOfTwo,
    FailureCode_IllegalObjectAttributes,
    FailureCode_InvalidPacketContextToken,
    FailureCode_InvalidPacketContextSizeOverride,
    FailureCode_InvalidNetPacketExtensionName,
    FailureCode_InvalidNetPacketExtensionVersion,
    FailureCode_InvalidNetPacketExtensionAlignment,
    FailureCode_InvalidNetPacketExtensionExtensionSize,
    FailureCode_NetPacketExtensionVersionedSizeMismatch,
    FailureCode_InvalidAdapterTxCapabilities,
    FailureCode_InvalidAdapterRxCapabilities,
    FailureCode_InvalidReceiveScalingHashType,
    FailureCode_InvalidReceiveScalingProtocolType,
    FailureCode_InvalidReceiveScalingEncapsulationType,
    FailureCode_IllegalAdapterDelete,
    FailureCode_RemovingDeviceWithAdapters,
    FailureCode_InvalidDatapathCallbacks,
    FailureCode_InvalidNetAdapterInitSignature,
    FailureCode_NetAdapterInitAlreadyUsed,
    FailureCode_InvalidNetAdapterExtensionInitSignature,
    FailureCode_InvalidLsoCapabilities,
} FailureCode;

//
// Verifier_ReportViolation uses a value from this enum to decide what to do in case of a violation
//
// Verifier_Verify* functions that use only VerifierAction_BugcheckAlways should not return any value.
// Verifier_Verify* functions that use VerifierAction_DbgBreakIfDebuggerPresent at least one time should return NTSTATUS, and the caller should deal with the status
//
typedef enum _VerifierAction
{
    VerifierAction_BugcheckAlways,
    VerifierAction_DbgBreakIfDebuggerPresent
} VerifierAction;

VOID
NetAdapterCxBugCheck(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ FailureCode         FailureCode,
    _In_ ULONG_PTR           Parameter2,
    _In_ ULONG_PTR           Parameter3
    );

VOID
Verifier_ReportViolation(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ VerifierAction      Action,
    _In_ FailureCode         FailureCode,
    _In_ ULONG_PTR           Parameter2,
    _In_ ULONG_PTR           Parameter3
    );

VOID
Verifier_VerifyPrivateGlobals(
    NX_PRIVATE_GLOBALS * PrivateGlobals
    );

VOID
Verifier_VerifyIrqlPassive(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals
    );

VOID
Verifier_VerifyIrqlLessThanOrEqualDispatch(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals
    );

VOID
Verifier_VerifyAdapterNotStarted(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxAdapter *         pNxAdapter
    );

VOID
Verifier_VerifyNetPowerSettingsAccessible(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxWake *             NetWake
    );

VOID
Verifier_VerifyObjectSupportsCancellation(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ WDFOBJECT Object
    );

VOID
Verifier_VerifyNetRequestCompletionStatusNotPending(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NETREQUEST NetRequest,
    _In_ NTSTATUS   CompletionStatus
    );

VOID
Verifier_VerifyNetRequestType(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxRequest * NxRequest,
    _In_ NDIS_REQUEST_TYPE Type
    );

VOID
Verifier_VerifyNetRequestIsQuery(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxRequest * NxRequest
    );

VOID
Verifier_VerifyNetRequest(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxRequest * pNxRequest
    );

template <typename T>
VOID
Verifier_VerifyTypeSize(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ T *Input)
{
    ULONG uInputSize = Input->Size;
    ULONG uExpectedSize = sizeof(T);

    if (uInputSize != uExpectedSize)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidStructTypeSize,
            uInputSize,
            uExpectedSize);
    }
}

VOID
Verifier_VerifyNotNull(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ void const * const Ptr
    );

NTSTATUS
Verifier_VerifyQueueConfiguration(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PNET_REQUEST_QUEUE_CONFIG QueueConfig
    );

VOID
Verifier_VerifyReceiveScalingCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PCNET_ADAPTER_RECEIVE_SCALING_CAPABILITIES Capabilities
    );

VOID
Verifier_VerifyPowerCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxAdapter const & NxAdapter,
    _In_ NET_ADAPTER_POWER_CAPABILITIES const & PowerCapabilities
    );

VOID
Verifier_VerifyLinkLayerCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PNET_ADAPTER_LINK_LAYER_CAPABILITIES LinkLayerCapabilities
    );

VOID
Verifier_VerifyCurrentLinkState(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PNET_ADAPTER_LINK_STATE LinkState
    );

VOID
Verifier_VerifyLinkLayerAddress(
    _In_ NX_PRIVATE_GLOBALS *PrivateGlobals,
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS *LinkLayerAddress
    );

VOID
Verifier_VerifyQueryAsUlongFlags(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_CONFIGURATION_QUERY_ULONG_FLAGS Flags
    );

NTSTATUS
Verifier_VerifyQueryNetworkAddressParameters(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ ULONG BufferLength,
    _In_ PVOID NetworkAddressBuffer
    );

VOID
Verifier_VerifyNetPacketContextToken(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_DATAPATH_DESCRIPTOR const * Descriptor,
    _In_ NET_PACKET* NetPacket,
    _In_ PNET_PACKET_CONTEXT_TOKEN Token
    );

VOID
Verifier_VerifyMtuSize(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
         ULONG MtuSize
    );

VOID
Verifier_VerifyQueueInitContext(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ QUEUE_CREATION_CONTEXT *NetQueueInit
    );

VOID
Verifier_VerifyNetPacketQueueConfiguration(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_PACKET_QUEUE_CONFIG const * Configuration
    );

VOID
Verifier_VerifyObjectAttributesParentIsNull(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PWDF_OBJECT_ATTRIBUTES ObjectAttributes
    );

VOID
Verifier_VerifyObjectAttributesContextSize(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES ObjectAttributes,
    _In_ SIZE_T MaximumContextSize
    );

VOID
Verifier_VerifyDatapathCallbacks(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_ADAPTER_DATAPATH_CALLBACKS const *DatapathCallbacks
    );

VOID
Verifier_VerifyAdapterInitSignature(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ AdapterInit const *AdapterInit
    );

VOID
Verifier_VerifyAdapterExtensionInitSignature(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ AdapterExtensionInit const *AdapterExtensionInit
    );

VOID
Verifier_VerifyAdapterInitNotUsed(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ AdapterInit const *AdapterInit
    );

VOID
Verifier_VerifyNetPacketContextAttributes(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PNET_PACKET_CONTEXT_ATTRIBUTES PacketContextAttributes
    );

VOID
Verifier_VerifyNetPacketExtension(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PNET_PACKET_EXTENSION NetPacketExtension
    );

VOID
Verifier_VerifyNetPacketExtensionQuery(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PNET_PACKET_EXTENSION_QUERY NetPacketExtension
    );

VOID
Verifier_VerifyNetAdapterTxCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ const NET_ADAPTER_TX_CAPABILITIES *TxCapabilities
    );

VOID
Verifier_VerifyNetAdapterRxCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ const NET_ADAPTER_RX_CAPABILITIES *RxCapabilities
    );

VOID
Verifier_VerifyAdapterCanBeDeleted(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxAdapter const *Adapter
    );

VOID
Verifier_VerifyDeviceAdapterCollectionIsEmpty(
    _In_ NX_PRIVATE_GLOBALS *PrivateGlobals,
    _In_ NxDevice const *Device,
    _In_ NxAdapterCollection const *AdapterCollection
    );

VOID
Verifier_VerifyLsoCapabilities(
    _In_ NX_PRIVATE_GLOBALS *PrivateGlobals,
    _In_ NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES const *LsoCapabilities
    );
