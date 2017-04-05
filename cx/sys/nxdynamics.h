/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name: NxDynamics.h

Abstract:
    Generated header for NET APIs

Environment:
    kernel mode only

    Warning: manual changes to this file will be lost.
--*/

#ifndef _NXDYNAMICS_H_
#define _NXDYNAMICS_H_


typedef struct _NETFUNCTIONS {

    PFN_NETADAPTERDEVICEINITCONFIG                            pfnNetAdapterDeviceInitConfig;
    PFN_NETADAPTERCREATE                                      pfnNetAdapterCreate;
    PFN_NETADAPTERSETLINKLAYERCAPABILITIES                    pfnNetAdapterSetLinkLayerCapabilities;
    PFN_NETADAPTERSETLINKLAYERMTUSIZE                         pfnNetAdapterSetLinkLayerMtuSize;
    PFN_NETADAPTERSETPOWERCAPABILITIES                        pfnNetAdapterSetPowerCapabilities;
    PFN_NETADAPTERSETDATAPATHCAPABILITIES                     pfnNetAdapterSetDataPathCapabilities;
    PFN_NETADAPTERSETCURRENTLINKSTATE                         pfnNetAdapterSetCurrentLinkState;
    PFN_NETADAPTERWDMGETNDISHANDLE                            pfnNetAdapterWdmGetNdisHandle;
    PFN_NETADAPTERGETNETLUID                                  pfnNetAdapterGetNetLuid;
    PFN_NETADAPTEROPENCONFIGURATION                           pfnNetAdapterOpenConfiguration;
    PFN_NETADAPTERGETPOWERSETTINGS                            pfnNetAdapterGetPowerSettings;
    PFN_NETADAPTERDRIVERWDMGETHANDLE                          pfnNetAdapterDriverWdmGetHandle;
    PFN_NETPACKETGETTYPEDCONTEXT                              pfnNetPacketGetTypedContext;
    PFN_NETPACKETGET802_15_4INFO                              pfnNetPacketGet802_15_4Info;
    PFN_NETPACKETGET802_15_4STATUS                            pfnNetPacketGet802_15_4Status;
    PFN_NETCONFIGURATIONCLOSE                                 pfnNetConfigurationClose;
    PFN_NETCONFIGURATIONOPENSUBCONFIGURATION                  pfnNetConfigurationOpenSubConfiguration;
    PFN_NETCONFIGURATIONQUERYULONG                            pfnNetConfigurationQueryUlong;
    PFN_NETCONFIGURATIONQUERYSTRING                           pfnNetConfigurationQueryString;
    PFN_NETCONFIGURATIONQUERYMULTISTRING                      pfnNetConfigurationQueryMultiString;
    PFN_NETCONFIGURATIONQUERYBINARY                           pfnNetConfigurationQueryBinary;
    PFN_NETCONFIGURATIONQUERYNETWORKADDRESS                   pfnNetConfigurationQueryNetworkAddress;
    PFN_NETCONFIGURATIONASSIGNULONG                           pfnNetConfigurationAssignUlong;
    PFN_NETCONFIGURATIONASSIGNUNICODESTRING                   pfnNetConfigurationAssignUnicodeString;
    PFN_NETCONFIGURATIONASSIGNMULTISTRING                     pfnNetConfigurationAssignMultiString;
    PFN_NETCONFIGURATIONASSIGNBINARY                          pfnNetConfigurationAssignBinary;
    PFN_NETOBJECTMARKCANCELABLEEX                             pfnNetObjectMarkCancelableEx;
    PFN_NETOBJECTUNMARKCANCELABLE                             pfnNetObjectUnmarkCancelable;
    PFN_NETPOWERSETTINGSGETWAKEPATTERNCOUNT                   pfnNetPowerSettingsGetWakePatternCount;
    PFN_NETPOWERSETTINGSGETWAKEPATTERNCOUNTFORTYPE            pfnNetPowerSettingsGetWakePatternCountForType;
    PFN_NETPOWERSETTINGSGETWAKEPATTERN                        pfnNetPowerSettingsGetWakePattern;
    PFN_NETPOWERSETTINGSISWAKEPATTERNENABLED                  pfnNetPowerSettingsIsWakePatternEnabled;
    PFN_NETPOWERSETTINGSGETENABLEDWAKEUPFLAGS                 pfnNetPowerSettingsGetEnabledWakeUpFlags;
    PFN_NETPOWERSETTINGSGETENABLEDWAKEPATTERNS                pfnNetPowerSettingsGetEnabledWakePatterns;
    PFN_NETPOWERSETTINGSGETENABLEDPROTOCOLOFFLOADS            pfnNetPowerSettingsGetEnabledProtocolOffloads;
    PFN_NETPOWERSETTINGSGETENABLEDMEDIASPECIFICWAKEUPEVENTS    pfnNetPowerSettingsGetEnabledMediaSpecificWakeUpEvents;
    PFN_NETPOWERSETTINGSGETPROTOCOLOFFLOADCOUNT               pfnNetPowerSettingsGetProtocolOffloadCount;
    PFN_NETPOWERSETTINGSGETPROTOCOLOFFLOADCOUNTFORTYPE        pfnNetPowerSettingsGetProtocolOffloadCountForType;
    PFN_NETPOWERSETTINGSGETPROTOCOLOFFLOAD                    pfnNetPowerSettingsGetProtocolOffload;
    PFN_NETPOWERSETTINGSISPROTOCOLOFFLOADENABLED              pfnNetPowerSettingsIsProtocolOffloadEnabled;
    PFN_NETREQUESTRETRIEVEINPUTOUTPUTBUFFER                   pfnNetRequestRetrieveInputOutputBuffer;
    PFN_NETREQUESTWDMGETNDISOIDREQUEST                        pfnNetRequestWdmGetNdisOidRequest;
    PFN_NETREQUESTCOMPLETEWITHOUTINFORMATION                  pfnNetRequestCompleteWithoutInformation;
    PFN_NETREQUESTSETDATACOMPLETE                             pfnNetRequestSetDataComplete;
    PFN_NETREQUESTQUERYDATACOMPLETE                           pfnNetRequestQueryDataComplete;
    PFN_NETREQUESTMETHODCOMPLETE                              pfnNetRequestMethodComplete;
    PFN_NETREQUESTSETBYTESNEEDED                              pfnNetRequestSetBytesNeeded;
    PFN_NETREQUESTGETID                                       pfnNetRequestGetId;
    PFN_NETREQUESTGETPORTNUMBER                               pfnNetRequestGetPortNumber;
    PFN_NETREQUESTGETSWITCHID                                 pfnNetRequestGetSwitchId;
    PFN_NETREQUESTGETVPORTID                                  pfnNetRequestGetVPortId;
    PFN_NETREQUESTGETTYPE                                     pfnNetRequestGetType;
    PFN_NETREQUESTQUEUECREATE                                 pfnNetRequestQueueCreate;
    PFN_NETREQUESTQUEUEGETADAPTER                             pfnNetRequestQueueGetAdapter;
    PFN_NETRXQUEUECREATE                                      pfnNetRxQueueCreate;
    PFN_NETRXQUEUENOTIFYMORERECEIVEDPACKETSAVAILABLE          pfnNetRxQueueNotifyMoreReceivedPacketsAvailable;
    PFN_NETRXQUEUEINITGETQUEUEID                              pfnNetRxQueueInitGetQueueId;
    PFN_NETRXQUEUECONFIGUREDMAALLOCATOR                       pfnNetRxQueueConfigureDmaAllocator;
    PFN_NETRXQUEUEGETRINGBUFFER                               pfnNetRxQueueGetRingBuffer;
    PFN_NETTXQUEUECREATE                                      pfnNetTxQueueCreate;
    PFN_NETTXQUEUENOTIFYMORECOMPLETEDPACKETSAVAILABLE         pfnNetTxQueueNotifyMoreCompletedPacketsAvailable;
    PFN_NETTXQUEUEINITGETQUEUEID                              pfnNetTxQueueInitGetQueueId;
    PFN_NETTXQUEUEGETRINGBUFFER                               pfnNetTxQueueGetRingBuffer;

} NETFUNCTIONS, *PNETFUNCTIONS;


typedef struct _NETVERSION {

    ULONG         Size;
    ULONG         FuncCount;
    NETFUNCTIONS  Functions;

} NETVERSION, *PNETVERSION;


_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterDeviceInitConfig)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _Inout_
    PWDFDEVICE_INIT DeviceInit
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterCreate)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFDEVICE Device,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES AdapterAttributes,
    _In_
    PNET_ADAPTER_CONFIG Configuration,
    _Out_
    NETADAPTER* Adapter
    );

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetLinkLayerCapabilities)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter,
    _In_
    PNET_ADAPTER_LINK_LAYER_CAPABILITIES LinkLayerCapabilities
    );

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetLinkLayerMtuSize)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter,
    _In_
    ULONG MtuSize
    );

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetPowerCapabilities)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter,
    _In_
    PNET_ADAPTER_POWER_CAPABILITIES PowerCapabilities
    );

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetDataPathCapabilities)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter,
    _In_
    PNET_ADAPTER_DATAPATH_CAPABILITIES DataPathCapabilities
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterSetCurrentLinkState)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter,
    _In_
    PNET_ADAPTER_LINK_STATE CurrentLinkState
    );

WDFAPI
NDIS_HANDLE
NETEXPORT(NetAdapterWdmGetNdisHandle)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter
    );

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NET_LUID
NETEXPORT(NetAdapterGetNetLuid)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter
    );

_Must_inspect_result_
_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetAdapterOpenConfiguration)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES ConfigurationAttributes,
    _Out_
    NETCONFIGURATION* Configuration
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NETPOWERSETTINGS
NETEXPORT(NetAdapterGetPowerSettings)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETADAPTER Adapter
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_HANDLE
NETEXPORT(NetAdapterDriverWdmGetHandle)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFDRIVER Driver,
    _In_
    NET_ADAPTER_DRIVER_TYPE Type
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PVOID
NETEXPORT(NetPacketGetTypedContext)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NET_PACKET* NetPacket,
    _In_
    PCNET_CONTEXT_TYPE_INFO TypeInfo
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG_PTR
NETEXPORT(NetPacketGet802_15_4Info)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NET_PACKET* NetPacket
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG_PTR
NETEXPORT(NetPacketGet802_15_4Status)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NET_PACKET* NetPacket
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetConfigurationClose)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationOpenSubConfiguration)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    PCUNICODE_STRING SubConfigurationName,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES SubConfigurationAttributes,
    _Out_
    NETCONFIGURATION* SubConfiguration
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryUlong)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    NET_CONFIGURATION_QUERY_ULONG_FLAGS Flags,
    _In_
    PCUNICODE_STRING ValueName,
    _Out_
    PULONG Value
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryString)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    PCUNICODE_STRING ValueName,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES StringAttributes,
    _Out_
    WDFSTRING* WdfString
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryMultiString)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    PCUNICODE_STRING ValueName,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES StringsAttributes,
    _Inout_
    WDFCOLLECTION Collection
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryBinary)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    PCUNICODE_STRING ValueName,
    _In_
    _Strict_type_match_
    POOL_TYPE PoolType,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES MemoryAttributes,
    _Out_
    WDFMEMORY* Memory
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryNetworkAddress)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    ULONG BufferLength,
    _Out_writes_bytes_to_(BufferLength, *ResultLength)
    PVOID NetworkAddressBuffer,
    _Out_
    PULONG ResultLength
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationAssignUlong)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    PCUNICODE_STRING ValueName,
    _In_
    ULONG Value
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationAssignUnicodeString)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    PCUNICODE_STRING ValueName,
    _In_
    PCUNICODE_STRING Value
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationAssignMultiString)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    PCUNICODE_STRING ValueName,
    _In_
    WDFCOLLECTION Collection
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationAssignBinary)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETCONFIGURATION Configuration,
    _In_
    PCUNICODE_STRING ValueName,
    _In_reads_bytes_(BufferLength)
    PVOID Buffer,
    _In_
    ULONG BufferLength
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetObjectMarkCancelableEx)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFOBJECT NetObject,
    _In_
    PFN_NET_OBJECT_CANCEL EvtObjectCancel
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetObjectUnmarkCancelable)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    WDFOBJECT NetObject
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetWakePatternCount)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetWakePatternCountForType)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings,
    _In_
    NDIS_PM_WOL_PACKET WakePatternType
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PNDIS_PM_WOL_PATTERN
NETEXPORT(NetPowerSettingsGetWakePattern)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings,
    _In_
    ULONG Index
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
BOOLEAN
NETEXPORT(NetPowerSettingsIsWakePatternEnabled)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings,
    _In_
    PNDIS_PM_WOL_PATTERN NdisPmWolPattern
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetEnabledWakeUpFlags)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetEnabledWakePatterns)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetEnabledProtocolOffloads)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetEnabledMediaSpecificWakeUpEvents)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetProtocolOffloadCount)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetProtocolOffloadCountForType)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings,
    _In_
    NDIS_PM_PROTOCOL_OFFLOAD_TYPE ProtocolOffloadType
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PNDIS_PM_PROTOCOL_OFFLOAD
NETEXPORT(NetPowerSettingsGetProtocolOffload)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings,
    _In_
    ULONG Index
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
BOOLEAN
NETEXPORT(NetPowerSettingsIsProtocolOffloadEnabled)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETPOWERSETTINGS NetPowerSettings,
    _In_
    PNDIS_PM_PROTOCOL_OFFLOAD NdisProtocolOffload
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRequestRetrieveInputOutputBuffer)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request,
    _In_
    UINT MininumInputLengthRequired,
    _In_
    UINT MininumOutputLengthRequired,
    _Outptr_result_bytebuffer_(max(*InputBufferLength,*OutputBufferLength))
    PVOID* InputOutputBuffer,
    _Out_opt_
    PUINT InputBufferLength,
    _Out_opt_
    PUINT OutputBufferLength
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PNDIS_OID_REQUEST
NETEXPORT(NetRequestWdmGetNdisOidRequest)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestCompleteWithoutInformation)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request,
    _In_
    NTSTATUS CompletionStatus
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestSetDataComplete)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request,
    _In_
    NTSTATUS CompletionStatus,
    _In_
    UINT BytesRead
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestQueryDataComplete)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request,
    _In_
    NTSTATUS CompletionStatus,
    _In_
    UINT BytesWritten
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestMethodComplete)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request,
    _In_
    NTSTATUS CompletionStatus,
    _In_
    UINT BytesRead,
    _In_
    UINT BytesWritten
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRequestSetBytesNeeded)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request,
    _In_
    UINT BytesNeeded
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_OID
NETEXPORT(NetRequestGetId)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_PORT_NUMBER
NETEXPORT(NetRequestGetPortNumber)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_NIC_SWITCH_ID
NETEXPORT(NetRequestGetSwitchId)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_NIC_SWITCH_VPORT_ID
NETEXPORT(NetRequestGetVPortId)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NDIS_REQUEST_TYPE
NETEXPORT(NetRequestGetType)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUEST Request
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRequestQueueCreate)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    PNET_REQUEST_QUEUE_CONFIG NetRequestQueueConfig,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES QueueAttributes,
    _Out_opt_
    NETREQUESTQUEUE* Queue
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NETADAPTER
NETEXPORT(NetRequestQueueGetAdapter)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETREQUESTQUEUE NetRequestQueue
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRxQueueCreate)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _Inout_
    PNETRXQUEUE_INIT NetRxQueueInit,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES RxQueueAttributes,
    _In_
    PNET_RXQUEUE_CONFIG Configuration,
    _Out_
    NETRXQUEUE* RxQueue
    );

_IRQL_requires_max_(HIGH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRxQueueNotifyMoreReceivedPacketsAvailable)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETRXQUEUE RxQueue
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetRxQueueInitGetQueueId)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    PNETRXQUEUE_INIT NetRxQueueInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRxQueueConfigureDmaAllocator)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETRXQUEUE RxQueue,
    _In_
    WDFDMAENABLER Enabler
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
PNET_RING_BUFFER
NETEXPORT(NetRxQueueGetRingBuffer)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETRXQUEUE NetRxQueue
    );

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetTxQueueCreate)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _Inout_
    PNETTXQUEUE_INIT NetTxQueueInit,
    _In_opt_
    PWDF_OBJECT_ATTRIBUTES TxQueueAttributes,
    _In_
    PNET_TXQUEUE_CONFIG Configuration,
    _Out_
    NETTXQUEUE* TxQueue
    );

_IRQL_requires_max_(HIGH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetTxQueueNotifyMoreCompletedPacketsAvailable)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETTXQUEUE TxQueue
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetTxQueueInitGetQueueId)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    PNETTXQUEUE_INIT NetTxQueueInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
PNET_RING_BUFFER
NETEXPORT(NetTxQueueGetRingBuffer)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETTXQUEUE NetTxQueue
    );


#ifdef NX_DYNAMICS_GENERATE_TABLE

NETVERSION NetVersion = {
    sizeof(NETVERSION),
    sizeof(NETFUNCTIONS)/sizeof(PVOID),
    {
        NETEXPORT(NetAdapterDeviceInitConfig),
        NETEXPORT(NetAdapterCreate),
        NETEXPORT(NetAdapterSetLinkLayerCapabilities),
        NETEXPORT(NetAdapterSetLinkLayerMtuSize),
        NETEXPORT(NetAdapterSetPowerCapabilities),
        NETEXPORT(NetAdapterSetDataPathCapabilities),
        NETEXPORT(NetAdapterSetCurrentLinkState),
        NETEXPORT(NetAdapterWdmGetNdisHandle),
        NETEXPORT(NetAdapterGetNetLuid),
        NETEXPORT(NetAdapterOpenConfiguration),
        NETEXPORT(NetAdapterGetPowerSettings),
        NETEXPORT(NetAdapterDriverWdmGetHandle),
        NETEXPORT(NetPacketGetTypedContext),
        NETEXPORT(NetPacketGet802_15_4Info),
        NETEXPORT(NetPacketGet802_15_4Status),
        NETEXPORT(NetConfigurationClose),
        NETEXPORT(NetConfigurationOpenSubConfiguration),
        NETEXPORT(NetConfigurationQueryUlong),
        NETEXPORT(NetConfigurationQueryString),
        NETEXPORT(NetConfigurationQueryMultiString),
        NETEXPORT(NetConfigurationQueryBinary),
        NETEXPORT(NetConfigurationQueryNetworkAddress),
        NETEXPORT(NetConfigurationAssignUlong),
        NETEXPORT(NetConfigurationAssignUnicodeString),
        NETEXPORT(NetConfigurationAssignMultiString),
        NETEXPORT(NetConfigurationAssignBinary),
        NETEXPORT(NetObjectMarkCancelableEx),
        NETEXPORT(NetObjectUnmarkCancelable),
        NETEXPORT(NetPowerSettingsGetWakePatternCount),
        NETEXPORT(NetPowerSettingsGetWakePatternCountForType),
        NETEXPORT(NetPowerSettingsGetWakePattern),
        NETEXPORT(NetPowerSettingsIsWakePatternEnabled),
        NETEXPORT(NetPowerSettingsGetEnabledWakeUpFlags),
        NETEXPORT(NetPowerSettingsGetEnabledWakePatterns),
        NETEXPORT(NetPowerSettingsGetEnabledProtocolOffloads),
        NETEXPORT(NetPowerSettingsGetEnabledMediaSpecificWakeUpEvents),
        NETEXPORT(NetPowerSettingsGetProtocolOffloadCount),
        NETEXPORT(NetPowerSettingsGetProtocolOffloadCountForType),
        NETEXPORT(NetPowerSettingsGetProtocolOffload),
        NETEXPORT(NetPowerSettingsIsProtocolOffloadEnabled),
        NETEXPORT(NetRequestRetrieveInputOutputBuffer),
        NETEXPORT(NetRequestWdmGetNdisOidRequest),
        NETEXPORT(NetRequestCompleteWithoutInformation),
        NETEXPORT(NetRequestSetDataComplete),
        NETEXPORT(NetRequestQueryDataComplete),
        NETEXPORT(NetRequestMethodComplete),
        NETEXPORT(NetRequestSetBytesNeeded),
        NETEXPORT(NetRequestGetId),
        NETEXPORT(NetRequestGetPortNumber),
        NETEXPORT(NetRequestGetSwitchId),
        NETEXPORT(NetRequestGetVPortId),
        NETEXPORT(NetRequestGetType),
        NETEXPORT(NetRequestQueueCreate),
        NETEXPORT(NetRequestQueueGetAdapter),
        NETEXPORT(NetRxQueueCreate),
        NETEXPORT(NetRxQueueNotifyMoreReceivedPacketsAvailable),
        NETEXPORT(NetRxQueueInitGetQueueId),
        NETEXPORT(NetRxQueueConfigureDmaAllocator),
        NETEXPORT(NetRxQueueGetRingBuffer),
        NETEXPORT(NetTxQueueCreate),
        NETEXPORT(NetTxQueueNotifyMoreCompletedPacketsAvailable),
        NETEXPORT(NetTxQueueInitGetQueueId),
        NETEXPORT(NetTxQueueGetRingBuffer),
    }
};

#endif // NX_DYNAMICS_GENERATE_TABLE

#endif // _NXDYNAMICS_H_

