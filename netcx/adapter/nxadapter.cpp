// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:
    This is the main NetAdapterCx driver framework.

--*/

#include "Nx.hpp"

#include <ndisguid.h>

#include <NetClientApi.h>
#include <NetClientBufferImpl.h>
#include <NetClientDriverConfigurationImpl.hpp>

#include <net/logicaladdresstypes_p.h>
#include <net/mdltypes_p.h>
#include <net/returncontexttypes_p.h>
#include <net/virtualaddresstypes_p.h>
#include <net/databuffer_p.h>
#include <net/wifi/exemptionactiontypes_p.h>
#include <net/ieee8021qtypes_p.h>
#include <net/packethashtypes_p.h>

#include "WdfObjectCallback.hpp"
#include "NxAdapter.hpp"

#include "NxDevice.hpp"
#include "NxDriver.hpp"
#include "NxConfiguration.hpp"
#include "NxExtension.hpp"
#include "NxQueue.hpp"
#include "NxUtility.hpp"
#include "NxDefaultDatapath.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include "NxAdapter.tmh"

static void *
    PLUGPLAY_NOTIFICATION_TAG = reinterpret_cast<void *>('NPnP');

//
// Standard NDIS callback declaration
//

static
MINIPORT_INITIALIZE
    EvtNdisInitializeEx;

static
MINIPORT_HALT
    EvtNdisHaltEx;

static
MINIPORT_PAUSE
    EvtNdisPause;

static
MINIPORT_RESTART
    EvtNdisRestart;

static
MINIPORT_SEND_NET_BUFFER_LISTS
    EvtNdisSendNetBufferLists;

static
MINIPORT_RETURN_NET_BUFFER_LISTS
    EvtNdisReturnNetBufferLists;

static
MINIPORT_CANCEL_SEND
    EvtNdisCancelSend;

static
MINIPORT_DEVICE_PNP_EVENT_NOTIFY
    EvtNdisDevicePnpEventNotify;

static
MINIPORT_SHUTDOWN
    EvtNdisShutdownEx;

static
MINIPORT_OID_REQUEST
    EvtNdisOidRequest;

static
MINIPORT_DIRECT_OID_REQUEST
    EvtNdisDirectOidRequest;

static
MINIPORT_CANCEL_OID_REQUEST
    EvtNdisCancelOidRequest;

static
MINIPORT_CANCEL_DIRECT_OID_REQUEST
    EvtNdisCancelDirectOidRequest;

static
MINIPORT_SYNCHRONOUS_OID_REQUEST
    EvtNdisSynchronousOidRequestHandler;

//
// NDIS-WDF callback declaration
//

static
EVT_NDIS_WDF_MINIPORT_GET_NDIS_HANDLE_FROM_DEVICE_OBJECT
    EvtNdisWdmDeviceGetNdisAdapterHandle;

static
EVT_NDIS_WDF_MINIPORT_GET_DEVICE_OBJECT
    EvtNdisGetDeviceObject;

static
EVT_NDIS_WDF_MINIPORT_GET_NEXT_DEVICE_OBJECT
    EvtNdisGetNextDeviceObject;

static
EVT_NDIS_WDF_MINIPORT_GET_ASSIGNED_FDO_NAME
    EvtNdisGetAssignedFdoName;

static
EVT_NDIS_WDF_MINIPORT_POWER_REFERENCE
    EvtNdisPowerReference;

static
EVT_NDIS_WDF_MINIPORT_POWER_DEREFERENCE
    EvtNdisPowerDereference;

static
EVT_NDIS_WDF_MINIPORT_UPDATE_IDLE_CONDITION
    EvtNdisUpdateIdleCondition;

static
EVT_NDIS_WDF_MINIPORT_AOAC_ENGAGE
    EvtNdisAoAcEngage;

static
EVT_NDIS_WDF_MINIPORT_AOAC_DISENGAGE
    EvtNdisAoAcDisengage;

static
EVT_NDIS_WDF_MINIPORT_UPDATE_PM_PARAMETERS
    EvtNdisUpdatePMParameters;

static
EVT_NDIS_WDF_MINIPORT_ALLOCATE_BLOCK
    EvtNdisAllocateMiniportBlock;

static
EVT_NDIS_WDF_MINIPORT_COMPLETE_ADD
    EvtNdisMiniportCompleteAdd;

static
EVT_NDIS_WDF_DEVICE_START_COMPLETE
    EvtNdisDeviceStartComplete;

static
EVT_NDIS_WDF_MINIPORT_DEVICE_RESET
    EvtNdisMiniportDeviceReset;

static
EVT_NDIS_WDF_MINIPORT_QUERY_DEVICE_RESET_SUPPORT
    EvtNdisMiniportQueryDeviceResetSupport;

static
EVT_NDIS_WDF_GET_WMI_EVENT_GUID
    EvtNdisGetWmiEventGuid;

//
// I/O subsystem callbacks
//

static
DRIVER_NOTIFICATION_CALLBACK_ROUTINE
    NetInterfaceChangeNotification;

static const NET_CLIENT_DISPATCH CxDispatch =
{
    sizeof(NET_CLIENT_DISPATCH),
    &NetClientCreateBufferPool,
    &NetClientQueryDriverConfigurationUlong,
    &NetClientQueryDriverConfigurationBoolean,
};

static
void
NetClientAdapterSetDeviceFailed(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NTSTATUS Status
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->SetDeviceFailed(Status);
}

static
void
NetClientAdapterGetProperties(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Out_ NET_CLIENT_ADAPTER_PROPERTIES * Properties
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->GetProperties(Properties);
}

static
void
NetClientAdapterGetDatapathCapabilities(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Out_ NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES * Capabilities
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->GetDatapathCapabilities(Capabilities);
}

static
void
NetClientAdapterGetReceiveScalingCapabilities(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Out_ NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES * Capabilities
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->RxScaling.GetCapabilities(Capabilities);
}

static
void
NetClientAdapterGetTxDemuxConfiguration(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Out_ NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION * Configuration
)
{
    reinterpret_cast<NxAdapter const *>(Adapter)->GetTxDemuxConfiguration(Configuration);
}

static
NTSTATUS
NetClientAdapterCreateTxQueue(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ void * ClientContext,
    _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    _Out_ NET_CLIENT_QUEUE * AdapterQueue,
    _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch,
    _Out_ NET_EXECUTION_CONTEXT * ExecutionContext,
    _Out_ NET_EXECUTION_CONTEXT_DISPATCH const ** ExecutionContextDispatch
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->CreateTxQueue(
        ClientContext,
        ClientDispatch,
        ClientQueueConfig,
        AdapterQueue,
        AdapterDispatch,
        ExecutionContext,
        ExecutionContextDispatch);
}

static
NTSTATUS
NetClientAdapterCreateRxQueue(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ void * ClientContext,
    _In_ NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    _In_ NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    _Out_ NET_CLIENT_QUEUE * AdapterQueue,
    _Out_ NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch,
    _Out_ NET_EXECUTION_CONTEXT * ExecutionContext,
    _Out_ NET_EXECUTION_CONTEXT_DISPATCH const ** ExecutionContextDispatch
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->CreateRxQueue(
        ClientContext,
        ClientDispatch,
        ClientQueueConfig,
        AdapterQueue,
        AdapterDispatch,
        ExecutionContext,
        ExecutionContextDispatch);
}

static
void
NetClientAdapterDestroyQueue(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_QUEUE Queue
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->DestroyQueue(Queue);
}

NONPAGEDX
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
static
void
NetClientReturnRxBuffer(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_FRAGMENT_RETURN_CONTEXT_HANDLE RxBufferReturnContext
)
{
    reinterpret_cast<NxAdapter*>(Adapter)->ReturnRxBuffer(RxBufferReturnContext);
}

static
NTSTATUS
NetClientAdapterRegisterExtension(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_EXTENSION const * Extension
)
{
    NET_EXTENSION_PRIVATE extenstionPrivate = {};
    extenstionPrivate.Name = Extension->Name;
    extenstionPrivate.Version = Extension->Version;
    extenstionPrivate.Size = Extension->ExtensionSize;
    extenstionPrivate.NonWdfStyleAlignment = Extension->Alignment;
    extenstionPrivate.Type = Extension->Type;

    return reinterpret_cast<NxAdapter *>(Adapter)->RegisterExtension(extenstionPrivate);
}

static
NTSTATUS
NetClientAdapterQueryExtension(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_EXTENSION const * Extension
)
{
    auto const extension = reinterpret_cast<NxAdapter *>(Adapter)->QueryExtensionLocked(
        Extension->Name,
        Extension->Version,
        Extension->Type);

    return extension != nullptr ?  STATUS_SUCCESS : STATUS_NOT_FOUND;
}

static
NTSTATUS
NetClientAdapterReceiveScalingEnable(
    _In_ NET_CLIENT_ADAPTER Adapter
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->RxScaling.Enable();
}

static
void
NetClientAdapterReceiveScalingDisable(
    _In_ NET_CLIENT_ADAPTER Adapter
)
{
    reinterpret_cast<NxAdapter *>(Adapter)->RxScaling.Disable();
}

static
NTSTATUS
NetClientAdapterReceiveScalingSetIndirectionEntries(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _Inout_ NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES * IndirectionEntries
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->RxScaling.SetIndirectionEntries(IndirectionEntries);
}

static
NTSTATUS
NetClientAdapterReceiveScalingSetHashSecretKey(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const * HashSecretKey
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->RxScaling.SetHashSecretKey(HashSecretKey);
}

static
NTSTATUS
NetClientAdapterReceiveScalingSetHashInfo(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ NET_CLIENT_RECEIVE_SCALING_HASH_INFO const * HashInfo
)
{
    auto const adapter = reinterpret_cast<NxAdapter *>(Adapter);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        adapter->RxScaling.CheckHashInfoCapabilities(HashInfo));

    auto const status = adapter->RxScaling.SetHashInfo(
        HashInfo);

    if (status != STATUS_SUCCESS)
    {
        Verifier_ReportViolation(
            adapter->GetPrivateGlobals(),
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidRSSConfiguration,
            status,
            0);
    }

    return status;
}

static
void
NetClientAdapterGetChecksumHardwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * HardwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetChecksumHardwareCapabilities(HardwareCapabilities);
}

static
void
NetClientAdapterGetChecksumDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetChecksumDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterSetChecksumActiveCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const * ActiveCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->SetChecksumActiveCapabilities(ActiveCapabilities);
}

static
void
NetClientAdapterGetTxChecksumHardwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * HardwareCapabilities
)
{
    reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetTxChecksumHardwareCapabilities(HardwareCapabilities);
}

static
void
NetClientAdapterGetTxChecksumSoftwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * SoftwareCapabilities
)
{
    reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetTxChecksumSoftwareCapabilities(SoftwareCapabilities);
}

static
void
NetClientAdapterGetTxIPv4ChecksumDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetTxIPv4ChecksumDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterGetTxTcpChecksumDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetTxTcpChecksumDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterGetTxUdpChecksumDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetTxUdpChecksumDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterSetTxChecksumActiveCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveIPv4Capabilities,
    _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveTcpCapabilities,
    _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveUdpCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->SetTxChecksumActiveCapabilities(
        ActiveIPv4Capabilities,
        ActiveTcpCapabilities,
        ActiveUdpCapabilities);
}

static
void
NetClientAdapterGetRxChecksumHardwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * HardwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetRxChecksumHardwareCapabilities(HardwareCapabilities);
}

static
void
NetClientAdapterGetRxChecksumDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetRxChecksumDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterSetRxChecksumActiveCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const * ActiveCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->SetRxChecksumActiveCapabilities(ActiveCapabilities);
}

static
void
NetClientAdapterGetGsoHardwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * HardwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetGsoHardwareCapabilities(HardwareCapabilities);
}

static
void
NetClientAdapterGetGsoSoftwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * SoftwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetGsoSoftwareCapabilities(SoftwareCapabilities);
}

static
void
NetClientAdapterGetLsoDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetLsoDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterGetUsoDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetUsoDefaultCapabilities(DefaultCapabilities);
}

static
void
NetClientAdapterSetGsoActiveCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const * LsoActiveCapabilities,
    _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const * UsoActiveCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->SetGsoActiveCapabilities(LsoActiveCapabilities,
        UsoActiveCapabilities);
}

static
void
NetClientAdapterGetRscHardwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * HardwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetRscHardwareCapabilities(HardwareCapabilities);
}

static
void
NetClientAdapterGetRscSoftwareCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * SoftwareCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetRscSoftwareCapabilities(SoftwareCapabilities);
}

static
void
NetClientAdapterGetRscDefaultCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * DefaultCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->GetRscDefaultCapabilities(DefaultCapabilities);
}

static
VOID
NetClientAdapterSetRscActiveCapabilities(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const * ActiveCapabilities
)
{
     reinterpret_cast<NxAdapter *>(ClientAdapter)->m_offloadManager->SetRscActiveCapabilities(ActiveCapabilities);
}

static
VOID
NetClientAdapterGetIeee8021qTagHardwareCapabilties(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _Out_ NET_CLIENT_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES * HardwareCapabilities
)
{
    reinterpret_cast<NxAdapter *>(ClientAdapter)->GetIeee8021qHardwareCapabilities(HardwareCapabilities);
}

static
NTSTATUS
NetClientAdapterSetReceiveFilter(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ ULONG PacketFilter,
    _In_ SIZE_T MulticastAddressCount,
    _In_ IF_PHYSICAL_ADDRESS const * MulticastAddressList
)
{
    auto const adapter = reinterpret_cast<NxAdapter *>(ClientAdapter);

    return adapter->SetReceiveFilter(PacketFilter, MulticastAddressCount, MulticastAddressList);
}

static
SIZE_T
NetClientAdapterWifiTxPeerDemux(
    _In_ NET_CLIENT_ADAPTER ClientAdapter,
    _In_ NET_CLIENT_EUI48_ADDRESS const * Address
)
{
    return reinterpret_cast<NxAdapter *>(ClientAdapter)->WifiTxPeerDemux(
        reinterpret_cast<NET_EUI48_ADDRESS const *>(Address));
}

static const NET_CLIENT_ADAPTER_DISPATCH AdapterDispatch =
{
    sizeof(NET_CLIENT_ADAPTER_DISPATCH),
    &NetClientAdapterSetDeviceFailed,
    &NetClientAdapterGetProperties,
    &NetClientAdapterGetDatapathCapabilities,
    &NetClientAdapterGetReceiveScalingCapabilities,
    &NetClientAdapterGetTxDemuxConfiguration,
    &NetClientAdapterCreateTxQueue,
    &NetClientAdapterCreateRxQueue,
    &NetClientAdapterDestroyQueue,
    &NetClientReturnRxBuffer,
    &NetClientAdapterRegisterExtension,
    &NetClientAdapterQueryExtension,
    &NetClientAdapterSetReceiveFilter,
    &NetClientAdapterWifiTxPeerDemux,
    {
        &NetClientAdapterReceiveScalingEnable,
        &NetClientAdapterReceiveScalingDisable,
        &NetClientAdapterReceiveScalingSetIndirectionEntries,
        &NetClientAdapterReceiveScalingSetHashSecretKey,
        &NetClientAdapterReceiveScalingSetHashInfo,
    },
    {
        &NetClientAdapterGetChecksumHardwareCapabilities,
        &NetClientAdapterGetChecksumDefaultCapabilities,
        &NetClientAdapterSetChecksumActiveCapabilities,
        &NetClientAdapterGetGsoHardwareCapabilities,
        &NetClientAdapterGetGsoSoftwareCapabilities,
        &NetClientAdapterGetLsoDefaultCapabilities,
        &NetClientAdapterGetUsoDefaultCapabilities,
        &NetClientAdapterSetGsoActiveCapabilities,
        &NetClientAdapterGetRscHardwareCapabilities,
        &NetClientAdapterGetRscSoftwareCapabilities,
        &NetClientAdapterGetRscDefaultCapabilities,
        &NetClientAdapterSetRscActiveCapabilities,
        &NetClientAdapterGetIeee8021qTagHardwareCapabilties,
        &NetClientAdapterGetTxChecksumHardwareCapabilities,
        &NetClientAdapterGetTxChecksumSoftwareCapabilities,
        &NetClientAdapterGetTxIPv4ChecksumDefaultCapabilities,
        &NetClientAdapterGetTxTcpChecksumDefaultCapabilities,
        &NetClientAdapterGetTxUdpChecksumDefaultCapabilities,
        &NetClientAdapterSetTxChecksumActiveCapabilities,
        &NetClientAdapterGetRxChecksumHardwareCapabilities,
        &NetClientAdapterGetRxChecksumDefaultCapabilities,
        &NetClientAdapterSetRxChecksumActiveCapabilities,
    }
};

_Use_decl_annotations_
IdleStateMachine *
IdleStateMachineFromAdapter(
    NETADAPTER Object
)
{
    auto nxAdapter = GetNxAdapterFromHandle(Object);
    auto nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());
    return &nxDevice->IdleStateMachine;
}

static
NTSTATUS
AllocateQueueId(
    _Inout_ Rtl::KArray<unique_packet_queue> & Queues,
    _Out_ ULONG & QueueId
)
{
    for (size_t i = 0; i < Queues.count(); i++)
    {
        if (! Queues[i])
        {
            QueueId = static_cast<ULONG>(i);
            return STATUS_SUCCESS;
        }
    }

    ULONG queueId = static_cast<ULONG>(Queues.count());
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! Queues.resize(Queues.count() + 1));

    QueueId = queueId;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
AdapterInit *
GetAdapterInitFromHandle(
    NX_PRIVATE_GLOBALS * PrivateGlobals,
    NETADAPTER_INIT * Handle
)
{
    auto adapterInit = reinterpret_cast<AdapterInit *>(Handle);

    Verifier_VerifyAdapterInitSignature(PrivateGlobals, adapterInit);

    return adapterInit;
}

_Use_decl_annotations_
NxAdapter::NxAdapter(
    NX_PRIVATE_GLOBALS * PrivateGlobals,
    AdapterInit const * AdapterInit
)
    : m_adapter(static_cast<NETADAPTER>(WdfObjectContextGetObject(this)))
    , m_privateGlobals(*PrivateGlobals)
    , RxScaling(m_adapter)
    , m_powerPolicy(m_device, GetNxDeviceFromHandle(m_device)->GetPowerPolicyEventCallbacks(), m_adapter, AdapterInit->ShouldAcquirePowerReferencesForAdapter)
    , m_device(AdapterInit->ParentDevice)
    , m_mediaExtensionType(AdapterInit->ExtensionType)
    , PnpPower(this)
    , m_packetMonitor(NxPacketMonitor(*this))
{
}

_Use_decl_annotations_
NTSTATUS
NetInterfaceChangeNotification(
    void * NotificationStructure,
    void * Context
    )
{
    auto const notification = static_cast<DEVICE_INTERFACE_CHANGE_NOTIFICATION const *>(NotificationStructure);

    if (notification->Event == GUID_DEVICE_INTERFACE_ARRIVAL)
    {
        OBJECT_ATTRIBUTES objAttributes;

        InitializeObjectAttributes(
            &objAttributes,
            notification->SymbolicLinkName,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            nullptr,
            nullptr);

        wil::unique_kernel_handle handle;
        IO_STATUS_BLOCK statusBlock = {};

        // We open a file handle with the device interface symbolic link for 2 reasons:
        //    1) Ensure the DEVICE_OBJECT is referenced
        //    2) To match the device interface arrival with the NxAdapter object stored in the notification context
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            ZwOpenFile(
                &handle,
                0,
                &objAttributes,
                &statusBlock,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                FILE_OPEN),
            "Failed to open handle to device");

        unique_file_object_reference fileObject;

        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            ObReferenceObjectByHandle(
                handle.get(),
                0,
                *IoFileObjectType,
                KernelMode,
                reinterpret_cast<void **>(&fileObject),
                nullptr),
            "ObReferenceObjectByHandle failed.");

        auto const & fileName = fileObject->FileName;

        if (fileName.Length % sizeof(wchar_t) != 0)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (fileName.Length == 0)
        {
            // Not the notification we're waiting for
            return STATUS_SUCCESS;
        }

        UNICODE_STRING interfaceGuid;
        interfaceGuid.Buffer = fileName.Buffer + 1;
        interfaceGuid.Length = fileName.Length - sizeof(wchar_t);
        interfaceGuid.MaximumLength = fileName.MaximumLength - sizeof(wchar_t);

        auto nxAdapter = static_cast<NxAdapter *>(Context);
        auto baseName = nxAdapter->GetBaseName();

        if (RtlEqualUnicodeString(&interfaceGuid, &baseName, TRUE))
        {
            // If we're here it is because NetAdapterStart was already invoked, which means
            // we are guaranteed to have a reference on the NETADAPTER object, thus it is safe
            // to use the NxAdapter context
            nxAdapter->PnpPower.IoInterfaceArrived();
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::Initialize(
    AdapterInit const * AdapterInit
)
{
    auto offloadFacade = wil::make_unique_nothrow<NxOffloadFacade>(*this);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !offloadFacade);

    m_offloadManager = wil::make_unique_nothrow<NxOffloadManager>(offloadFacade.release());
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_offloadManager);

    CX_RETURN_IF_NOT_NT_SUCCESS(m_offloadManager.get()->Initialize());

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = GetFxObject();

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfWaitLockCreate(
            &objectAttributes,
            &m_dataPathControlLock));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfWaitLockCreate(
            &objectAttributes,
            &m_extensionCollectionLock));

    WDF_WORKITEM_CONFIG stopIdleWorkItemConfig;
    WDF_WORKITEM_CONFIG_INIT(&stopIdleWorkItemConfig, _EvtStopIdleWorkItem);
    stopIdleWorkItemConfig.AutomaticSerialization = FALSE;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfWorkItemCreate(
            &stopIdleWorkItemConfig,
            &objectAttributes,
            &m_powerRequiredWorkItem),
        "Failed to create m_powerRequiredWorkItem");

    #ifdef _KERNEL_MODE
    StateMachineEngineConfig smConfig(WdfDeviceWdmGetDeviceObject(m_device), NETADAPTERCX_TAG);
    #else
    StateMachineEngineConfig smConfig;
    #endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxAdapterStateMachine::Initialize(smConfig),
        "Failed to initialize NxAdapter state machine");

    // Clear the initial state machine transition
    m_transitionComplete.Clear();

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        InitializeDatapath(AdapterInit->DatapathCallbacks),
        "Failed to initialize datapath");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        InitializeAdapterExtensions(AdapterInit),
        "Failed to initialize NetAdapterCx extensions");

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_txDemux.reserve(AdapterInit->TxDemux.count()));

    for (auto const & demux : AdapterInit->TxDemux)
    {
        NT_FRE_ASSERT(m_txDemux.append(demux));
    }

    ClearGeneralAttributes();

    //
    // Report the newly created device to Ndis.sys
    //
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        CreateNdisMiniport(),
        "Failed to create NDIS miniport");

    ULONG wakeReasonSize =
        RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8) +
        RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8) +
        RTL_NUM_ALIGN_UP(MAX_ETHERNET_PACKET_SIZE, 8);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = GetFxObject();

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfMemoryCreate(&objectAttributes,
            NonPagedPoolNx,
            NETADAPTERCX_TAG,
            wakeReasonSize,
            &m_wakeReasonMemory,
            nullptr));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        IoRegisterPlugPlayNotification(
            EventCategoryDeviceInterfaceChange,
            0,
            (void *)&GUID_DEVINTERFACE_NET,
            WdfDriverWdmGetDriverObject(GetPrivateGlobals()->NxDriver->GetFxObject()),
            NetInterfaceChangeNotification,
            this,
            &m_plugPlayNotificationHandle));

    WdfObjectReferenceWithTag(GetFxObject(), PLUGPLAY_NOTIFICATION_TAG);

    m_nblDatapath.SetNdisHandle(GetNdisHandle());

    auto nxDevice = GetNxDeviceFromHandle(GetDevice());

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PnpPower.Initialize(),
        "Failed to initialize Adapter PnpPower state machine");

    // Inform the Device PnpPower state machine so that it can bring the
    // new adapter PnP/Power to a consistent state
    nxDevice->PnpPower.AdapterCreated(this);

    // We let the driver start by default
    CX_RETURN_IF_NOT_NT_SUCCESS(PnpPower.NetAdapterStart(WdfGetDriver()));

    DECLARE_CONST_UNICODE_STRING(keyword, L"*RSS");
    CX_RETURN_IF_NOT_NT_SUCCESS(
        QueryStandardizedKeyword(keyword, RxScaling.RssKeywordEnabled));

    DECLARE_CONST_UNICODE_STRING(numRssQueues, L"*NumRssQueues");
    if (!NT_SUCCESS(QueryStandardizedKeyword(numRssQueues, RxScaling.DefaultNumberOfQueues)))
    {
        LogWarning(FLAG_ADAPTER, "*NumRssQueues keyword not present");
    }

    // Initialize power policy
    m_powerPolicy.Initialize();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::QueryStandardizedKeyword(
    UNICODE_STRING const & Keyword,
    bool & Enabled
)
{
    NxConfiguration * configuration;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        NxConfiguration::_Create(GetPrivateGlobals(), GetFxObject(), NULL, &configuration));

    auto status = configuration->Open();
    if (status != STATUS_SUCCESS)
    {
        configuration->Close();
        return status;
    }

    Enabled = false;

    ULONG value;
    if (STATUS_SUCCESS ==
        configuration->QueryUlong(NET_CONFIGURATION_QUERY_ULONG_NO_FLAGS, &Keyword, &value))
    {
        Enabled = !! value;
    }

    configuration->Close();

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::QueryStandardizedKeyword(
    UNICODE_STRING const & Keyword,
    _Out_ ULONG & Value
)
{
    NxConfiguration * configuration;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        NxConfiguration::_Create(GetPrivateGlobals(), GetFxObject(), NULL, &configuration));

    auto status = configuration->Open();
    if (status != STATUS_SUCCESS)
    {
        configuration->Close();
        return status;
    }

    status = configuration->QueryUlong(NET_CONFIGURATION_QUERY_ULONG_NO_FLAGS, &Keyword, &Value);

    configuration->Close();

    return status;
}


void
NxAdapter::PrepareForRebalance(
    void
)
{
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::InitializeAdapterExtensions(
    AdapterInit const * AdapterInit
)
{
    for (auto& extension : AdapterInit->AdapterExtensions)
    {
        if (!m_adapterExtensions.append({ &extension }))
            return STATUS_INSUFFICIENT_RESOURCES;

        if (extension.WifiTxPeerDemux)
        {
            NT_FRE_ASSERT(nullptr == m_wifiTxPeerDemux);
            m_wifiTxPeerDemux = extension.WifiTxPeerDemux;
        }
    }

    return STATUS_SUCCESS;
}

NX_PRIVATE_GLOBALS *
NxAdapter::GetPrivateGlobals(
    void
) const
{
    // Return the private globals of the client driver that called NetAdapterCreate
    return &m_privateGlobals;
}

NET_DRIVER_GLOBALS *
NxAdapter::GetPublicGlobals(
    void
) const
{
    return &m_privateGlobals.Public;
}

_Use_decl_annotations_
void
NxAdapter::EnqueueEventSync(
    NxAdapter::Event Event
)
{
    EnqueueEvent(Event);
    m_transitionComplete.Wait();
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::_Create(
    NX_PRIVATE_GLOBALS * PrivateGlobals,
    AdapterInit const * AdapterInit
)
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxAdapter);

    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxAdapter::_EvtCleanup;
    attributes.EvtDestroyCallback = [](WDFOBJECT Object)
    {
        auto nxAdapter = GetNxAdapterFromHandle(Object);
        nxAdapter->~NxAdapter();
    };

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfObjectAllocateContext(
            AdapterInit->Object.get(),
            &attributes,
            nullptr));

    auto nxAdapter = new (GetNxAdapterFromHandle(AdapterInit->Object.get())) NxAdapter(
        PrivateGlobals,
        AdapterInit);

    CX_RETURN_IF_NOT_NT_SUCCESS(nxAdapter->Initialize(AdapterInit));

    return STATUS_SUCCESS;
}

NDIS_MEDIUM
NxAdapter::GetMediaType(
    void
) const
{
    return m_mediaType;
}

_Use_decl_annotations_
size_t
NxAdapter::WifiTxPeerDemux(
    NET_EUI48_ADDRESS const * Address
) const
{
    return m_wifiTxPeerDemux(GetFxObject(), Address);
}

_Use_decl_annotations_
NET_ADAPTER_TX_DEMUX const *
NxAdapter::GetTxPeerAddressDemux(
    void
) const
{
    for (auto const & demux : m_txDemux)
    {
        if (demux.Type == 2U) // PeerAddress
        {
            return &demux;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
void
NxAdapter::DispatchOidRequest(
    NDIS_OID_REQUEST * NdisOidRequest,
    DispatchContext * Context
)
{
    auto const nextHandler = Context->NextHandlerIndex++;
    auto const frameworkHandlerIndex = m_adapterExtensions.count();

    //
    // Note: It is not safe to use 'NdisOidRequest' or 'Context' after either one of
    // InvokeOidPreprocessCallback or FrameworkOidHandler has returned, since those
    // calls might complete the OID request before returning
    //

    if (nextHandler < frameworkHandlerIndex)
    {
        auto const & extension = m_adapterExtensions[nextHandler];
        extension.InvokeOidPreprocessCallback(GetFxObject(), NdisOidRequest, Context);
    }
    else if (nextHandler == frameworkHandlerIndex)
    {
        FrameworkOidHandler(NdisOidRequest, Context);
    }
    else
    {
        NT_ASSERT(nextHandler == frameworkHandlerIndex + 1);
        NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, NDIS_STATUS_NOT_SUPPORTED);
    }
}

_Use_decl_annotations_
void
NxAdapter::FrameworkOidHandler(
    NDIS_OID_REQUEST * NdisOidRequest,
    DispatchContext * Context
)
{
    switch (NdisOidRequest->RequestType)
    {
        case NdisRequestSetInformation:
            FrameworkSetOidHandler(NdisOidRequest, Context);
            break;

        default:
            //
            // Others types of OID requests are not handled by the framework,
            // dispatch to the next handler
            //
            DispatchOidRequest(NdisOidRequest, Context);
            break;
    }
}

_Use_decl_annotations_
void
NxAdapter::FrameworkSetOidHandler(
    NDIS_OID_REQUEST * NdisOidRequest,
    DispatchContext * Context
)
{
    auto Set = &NdisOidRequest->DATA.SET_INFORMATION;

    //
    // All internal OID handling is synchronous, we must either complete the request
    // (NdisMOidRequestComplete) or let the framework keep processing it (DispatchOidRequest)
    //

    switch(Set->Oid)
    {
        case OID_PM_ADD_WOL_PATTERN:
        {
            auto ndisStatus = m_powerPolicy.AddWakePattern(Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }

        case OID_PM_REMOVE_WOL_PATTERN:
        {
            auto ndisStatus = m_powerPolicy.RemoveWakePattern(Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }

        case OID_PM_ADD_PROTOCOL_OFFLOAD:
        {
            auto ndisStatus = m_powerPolicy.AddProtocolOffload(Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }
        case OID_PM_REMOVE_PROTOCOL_OFFLOAD:
        {
            auto ndisStatus = m_powerPolicy.RemoveProtocolOffload(Set);
            NdisMOidRequestComplete(GetNdisHandle(), NdisOidRequest, ndisStatus);
            break;
        }
        default:
        {
            DispatchOidRequest(NdisOidRequest, Context);
            break;
        }
    }
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::DispatchDirectOidRequest(
    NDIS_OID_REQUEST * NdisOidRequest,
    DispatchContext * Context
)
{
    auto const nextHandler = Context->NextHandlerIndex++;

    //
    // Note: It is not safe to use 'NdisOidRequest' or 'Context' after either one of
    // InvokeDirectOidPreprocessCallback has returned, since those calls might complete
    // the OID request before returning
    //

    if (nextHandler < m_adapterExtensions.count())
    {
        auto const & extension = m_adapterExtensions[nextHandler];
        return extension.InvokeDirectOidPreprocessCallback(GetFxObject(), NdisOidRequest, Context);
    }

    NT_FRE_ASSERT(nextHandler == m_adapterExtensions.count());
    return STATUS_NDIS_NOT_SUPPORTED;
}

NDIS_HANDLE
NxAdapter::GetNdisHandle(
    void
) const
{
    return m_ndisAdapterHandle;
}

_Use_decl_annotations_
void
NdisWdfCxCharacteristicsInitialize(
    NDIS_WDF_CX_CHARACTERISTICS & Characteristics
)
{
    NDIS_WDF_CX_CHARACTERISTICS_INIT(&Characteristics);

    Characteristics.EvtCxGetDeviceObject = EvtNdisGetDeviceObject;
    Characteristics.EvtCxGetNextDeviceObject = EvtNdisGetNextDeviceObject;
    Characteristics.EvtCxGetAssignedFdoName = EvtNdisGetAssignedFdoName;
    Characteristics.EvtCxPowerReference = EvtNdisPowerReference;
    Characteristics.EvtCxPowerDereference = EvtNdisPowerDereference;
    Characteristics.EvtCxUpdateIdleCondition = EvtNdisUpdateIdleCondition;
    Characteristics.EvtCxGetNdisHandleFromDeviceObject = EvtNdisWdmDeviceGetNdisAdapterHandle;
    Characteristics.EvtCxUpdatePMParameters = EvtNdisUpdatePMParameters;
    Characteristics.EvtCxAllocateMiniportBlock = EvtNdisAllocateMiniportBlock;
    Characteristics.EvtCxMiniportCompleteAdd = EvtNdisMiniportCompleteAdd;
    Characteristics.EvtCxDeviceStartComplete = EvtNdisDeviceStartComplete;
    Characteristics.EvtCxMiniportDeviceReset = EvtNdisMiniportDeviceReset;
    Characteristics.EvtCxMiniportQueryDeviceResetSupport = EvtNdisMiniportQueryDeviceResetSupport;
    Characteristics.EvtCxGetWmiEventGuid = EvtNdisGetWmiEventGuid;
}

_Use_decl_annotations_
DEVICE_OBJECT *
EvtNdisGetDeviceObject(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the WDM Device Object for the clients FDO

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    return WdfDeviceWdmGetDeviceObject(static_cast<NxAdapter *>(Adapter)->GetDevice());
}

_Use_decl_annotations_
DEVICE_OBJECT *
EvtNdisGetNextDeviceObject(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the attached WDM Device Object for the clients FDO

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    return WdfDeviceWdmGetAttachedDevice(static_cast<NxAdapter *>(Adapter)->GetDevice());
}

_Use_decl_annotations_
NTSTATUS
EvtNdisGetAssignedFdoName(
    NDIS_HANDLE Adapter,
    UNICODE_STRING * FdoName
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the name NetAdapterCx assigned to this device. NDIS
    needs to know this name to be able to create a symbolic link for the miniport.

Arguments:
    Adapter            - The NxAdapter object reported as context to NDIS
    FdoName            - UNICODE_STRING that will receive the FDO name
--*/
{
    NTSTATUS       status;
    WDFSTRING      fdoName;
    UNICODE_STRING usFdoName;

    status = WdfStringCreate(
        NULL,
        WDF_NO_OBJECT_ATTRIBUTES,
        &fdoName);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = WdfDeviceRetrieveDeviceName(static_cast<NxAdapter *>(Adapter)->GetDevice(), fdoName);

    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    WdfStringGetUnicodeString(fdoName, &usFdoName);

    if (FdoName->MaximumLength < usFdoName.Length)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    RtlCopyUnicodeString(FdoName, &usFdoName);

Exit:

    WdfObjectDelete(fdoName);
    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtNdisPowerReference(
    NDIS_HANDLE Adapter,
    BOOLEAN WaitForD0,
    BOOLEAN InvokeCompletionCallback
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine is called to acquire a power reference on the miniport.
    Every call to EvtNdisPowerReference must be matched by a call
    to EvtNdisPowerDereference. However, this function
    keeps tracks of failures from the underlying call to WdfDeviceStopIdle. So the
    caller only needs to make sure they always call power EvtNdisPowerDereference
    after calling EvtNdisPowerReference without keeping track of returned failures.

Arguments:
    Adapter   - The NxAdapter object reported as context to NDIS
    WaitForD0 - If WaitFromD0 is false and STATUS_PENDING status is returned
        the Cx will call back into NDIS once power up is complete.
    InvokeCompletionCallback - IFF WaitForD0 is FALSE (i.e. Async power ref),
        then the caller can ask for a callback when power up is complete
--*/
{
    NT_ASSERTMSG("Invalid parameter to NdisPowerRef",
                !(WaitForD0 && InvokeCompletionCallback));

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    if (InvokeCompletionCallback)
    {
        auto const ntStatus = nxAdapter->PowerReference(false);

        if (ntStatus == STATUS_PENDING)
        {
            // If the device is resuming from low power we need to schedule a work
            // item to invoke the NDIS completion callback.
            nxAdapter->QueuePowerReferenceWorkItem();
        }

        return ntStatus;
    }
    else
    {
        return nxAdapter->PowerReference(WaitForD0);
    }
}

void
NxAdapter::_EvtStopIdleWorkItem(
    _In_ WDFWORKITEM WorkItem
)
/*++
Routine Description:
    A shared WDF work item callback for processing an asynchronous
    EvtNdisPowerReference callback.

Arguments:
    WDFWORKITEM - The WDFWORKITEM object that is parented to NxAdapter.
--*/
{
    NTSTATUS status;
    WDFOBJECT nxAdapterWdfHandle = WdfWorkItemGetParentObject(WorkItem);
    auto nxAdapter = GetNxAdapterFromHandle((NETADAPTER)nxAdapterWdfHandle);

    // We need to notify NDIS with the device in D0, so we acquire a synchronous power reference
    status = nxAdapter->PowerReference(true);

    NdisWdfAsyncPowerReferenceCompleteNotification(nxAdapter->m_ndisAdapterHandle,
                                                   status);

    // Drop the reference we acquired above. The original reference will be released by NDIS
    nxAdapter->PowerDereference();

    nxAdapter->m_powerRequiredWorkItemPending = FALSE;
}

_Use_decl_annotations_
NTSTATUS
EvtNdisAllocateMiniportBlock(
    NDIS_HANDLE Adapter,
    ULONG Size,
    void ** MiniportBlock
)
/*++
Routine Description:
    This function will allocate the memory for the adapter's miniport block as
    a context in the NETADAPTER object. This way we can ensure the miniport block
    will be valid memory during the adapter's life time.
--*/
{

    NTSTATUS status = STATUS_SUCCESS;

    WDF_OBJECT_ATTRIBUTES contextAttributes;

    // Initialize contextAttributes with our dummy type
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &contextAttributes,
        NDIS_MINIPORT_BLOCK_TYPE);

    // Override the memory size to allocate with what NDIS told us
    contextAttributes.ContextSizeOverride = Size;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    status = WdfObjectAllocateContext(nxAdapter->GetFxObject(), &contextAttributes, MiniportBlock);

    if (!NT_SUCCESS(status))
    {
        LogError(FLAG_ADAPTER,
            "WdfObjectAllocateContext failed to allocate the miniport block, %!STATUS!", status);

        return status;
    }

    return status;
}

_Use_decl_annotations_
void
EvtNdisMiniportCompleteAdd(
    NDIS_HANDLE Adapter,
    NDIS_WDF_COMPLETE_ADD_PARAMS * Params
)
/*++
Routine Description:
    NDIS uses this callback to report certain adapter parameters to the NIC
    after the Miniport side of device add has completed.
--*/
{

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->NdisMiniportCreationComplete(*Params);
}

_Use_decl_annotations_
void
EvtNdisDeviceStartComplete(
    NDIS_HANDLE Adapter
)
/*
Description:

    NDIS uses this callback to indicate the underlying device has completed
    the start IRP, but because NDIS does not distinguish between device and
    adapter it reports for only one of a device's child adapters.
*/
{
    UNREFERENCED_PARAMETER(Adapter);
}


_Use_decl_annotations_
NTSTATUS
EvtNdisMiniportDeviceReset(
    NDIS_HANDLE Adapter,
    NET_DEVICE_RESET_TYPE NetDeviceResetType
)
/*
Routine Description:

    NDIS uses this callback to indicate the underlying device is in a bad state
    and it needs to be reset in order to recover.

Arguments:
    Adapter            - The NxAdapter object reported as context to NDIS
    NetDeviceResetType - The type of reset operation requested (by default this is PLDR)
*/
{
    DEVICE_RESET_TYPE DeviceResetType;
    NxAdapter *nxAdapter = static_cast<NxAdapter *>(Adapter);
    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    nxDevice->SetFailingDeviceRequestingResetFlag();

    // Trigger reset of the device as we are told to do so
    switch (NetDeviceResetType)
    {
    case FunctionLevelReset:
        DeviceResetType = FunctionLevelDeviceReset;
        break;
    case PlatformLevelReset:
        DeviceResetType = PlatformLevelDeviceReset;
        break;
    default:
        DeviceResetType = PlatformLevelDeviceReset;
        break;
    };

    return nxDevice->DispatchDeviceReset(DeviceResetType);
}

_Use_decl_annotations_
NTSTATUS
EvtNdisMiniportQueryDeviceResetSupport(
    NDIS_HANDLE Adapter,
    PULONG SupportedNetDeviceResetTypes
)
/*
Routine Description:

    NDIS uses this callback to query whether Device Reset is supported by the underlying
    device for which there is a NetAdapter based driver present

Arguments:
    Adapter                      - The NxAdapter object reported as context to NDIS
    SupportedNetDeviceResetTypes - Out parameter which will have the information about what all reset types is supported
*/
{
    NxAdapter *nxAdapter = static_cast<NxAdapter *>(Adapter);
    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    if (nxDevice->GetSupportedDeviceResetType(SupportedNetDeviceResetTypes))
    {
        return STATUS_SUCCESS;
    }
    else
    {
        return STATUS_NOT_SUPPORTED;
    }
}

_Use_decl_annotations_
void
EvtNdisPowerDereference(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine is called to release a power reference on the
    miniport that it previously acquired. Every call to EvtNdisPowerReference
    MUST be matched by a call to EvtNdisPowerDereference.
    However, since failures are tracked from calls to EvtNdisPowerReference, the
    caller can call EvtNdisPowerDereference without having to keep track of
    failures from calls to EvtNdisPowerReference.

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->PowerDereference();
}

_Use_decl_annotations_
NTSTATUS
EvtNdisGetWmiEventGuid(
    NDIS_HANDLE Adapter,
    NTSTATUS GuidStatus,
    NDIS_GUID ** Guid
)
{
    auto const nxAdapter = static_cast<NxAdapter *>(Adapter);
    auto const nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    return nxDevice->WmiGetEventGuid(GuidStatus, Guid);
}

_Use_decl_annotations_
void
EvtNdisUpdateIdleCondition(
    NDIS_HANDLE Adapter,
    NDIS_IDLE_CONDITION IdleCondition
)
/*++
Routine Description:
    An Ndis Event Callback

    This routine is called to notify adapter of an update to the
    Idle Condition so that it release / acquire WDF power references
    based upon idle restriction policy.

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
    IdleCondition - New idle condition
--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->m_powerPolicy.SetCurrentIdleCondition(IdleCondition);
}

NTSTATUS
NxAdapter::SetRegistrationAttributes(
    void
)
{
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES adapterRegistration = {0};
    NTSTATUS                status;
    NDIS_STATUS             ndisStatus;

    //
    // Set the registration attributes for the network adapter.
    //
    // Note: Unlike a traditional NDIS miniport, the NetAdapterCx client
    // must not set the adapterRegistration.MiniportAdapterContext value.
    //

    adapterRegistration.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
    adapterRegistration.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;
    adapterRegistration.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_2;

    adapterRegistration.AttributeFlags =
        NDIS_MINIPORT_ATTRIBUTES_SURPRISE_REMOVE_OK |
        NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM |
        NDIS_MINIPORT_ATTRIBUTES_NO_PAUSE_ON_SUSPEND;

    adapterRegistration.InterfaceType = NdisInterfacePNPBus;

    ndisStatus = NdisMSetMiniportAttributes(
        m_ndisAdapterHandle,
        (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&adapterRegistration);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(FLAG_ADAPTER,
                 "NdisMSetMiniportAttributes failed, %!NDIS_STATUS!", ndisStatus);
        goto Exit;
    }

Exit:

    return status;
}


void
NxAdapter::SetPermanentLinkLayerAddress(
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS *LinkLayerAddress
)
{
    NT_FRE_ASSERT(LinkLayerAddress->Length <= sizeof(LinkLayerAddress->Address));

    RtlCopyMemory(
        &m_permanentLinkLayerAddress,
        LinkLayerAddress,
        sizeof(m_permanentLinkLayerAddress));
}

void
NxAdapter::SetCurrentLinkLayerAddress(
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS *LinkLayerAddress
)
{
    NT_FRE_ASSERT(LinkLayerAddress->Length <= sizeof(LinkLayerAddress->Address));

    RtlCopyMemory(
        &m_currentLinkLayerAddress,
        LinkLayerAddress,
        sizeof(m_currentLinkLayerAddress));

    if (m_flags.GeneralAttributesSet)
    {
        IndicateCurrentLinkLayerAddressToNdis();
    }
}

NTSTATUS
NxAdapter::SetReceiveFilter(
    _In_ ULONG PacketFilter,
    _In_ SIZE_T MulticastAddressCount,
    _In_ IF_PHYSICAL_ADDRESS const * MulticastAddressList
)
{
    // When the adapter does not have any SupportedPacketFilters,  we cannot
    // set any packet filters.
    CX_RETURN_NTSTATUS_IF(
        STATUS_NOT_SUPPORTED,
        m_receiveFilterCapabilities.SupportedPacketFilters == 0);

    // Do not support requests to set unsupported packet filters.
    CX_RETURN_NTSTATUS_IF(
        STATUS_NOT_SUPPORTED,
        PacketFilter & ~(m_receiveFilterCapabilities.SupportedPacketFilters)
            && GetMediaType() != NdisMediumNative802_11);

    NET_RECEIVE_FILTER receiveFilter;
    receiveFilter.PacketFilter = static_cast<NET_PACKET_FILTER_FLAGS>(PacketFilter);
    receiveFilter.MulticastAddressCount = MulticastAddressCount;
    receiveFilter.MulticastAddressList = MulticastAddressList;
    receiveFilter.CurrentThread = KeGetCurrentThread();

    InvokePoweredCallback(
        m_receiveFilterCapabilities.EvtSetReceiveFilter,
        GetFxObject(),
        reinterpret_cast<NETRECEIVEFILTER>(&receiveFilter));

    return STATUS_SUCCESS;
}

NTSTATUS
NxAdapter::SetGeneralAttributes(
    void
)
{
    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES adapterGeneral = {0};
    NDIS_PM_CAPABILITIES                     pmCapabilities;
    NTSTATUS                                 status = STATUS_INVALID_PARAMETER;
    NDIS_STATUS                              ndisStatus;

    if (m_rxCapabilities.Size == 0 || m_txCapabilities.Size == 0)
    {
        LogError(FLAG_ADAPTER,
            "Client missed or incorrectly called NetAdapterSetDatapathCapabilities");
        goto Exit;
    }

    if (m_linkLayerCapabilities.Size == 0) {
        LogError(FLAG_ADAPTER,
                 "Client missed or incorrectly called NetAdapterSetLinkLayerCapabilities");
        goto Exit;
    }

    if (m_mtuSize == 0) {
        LogError(FLAG_ADAPTER,
            "Client missed or incorrectly called NetAdapterSetMtuSize");
        goto Exit;
    }

    // If the client set either only the permanent or the current link layer
    // address, use the one set as both
    if (HasPermanentLinkLayerAddress() && !HasCurrentLinkLayerAddress())
    {
        RtlCopyMemory(
            &m_currentLinkLayerAddress,
            &m_permanentLinkLayerAddress,
            sizeof(m_currentLinkLayerAddress));
    }
    else if (!HasPermanentLinkLayerAddress() && HasCurrentLinkLayerAddress())
    {
        RtlCopyMemory(
            &m_permanentLinkLayerAddress,
            &m_currentLinkLayerAddress,
            sizeof(m_permanentLinkLayerAddress));
    }

    // In NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES the length of both the permanent and the
    // current link layer addresses are represented by MacAddressLength, which implies that if the
    // client set link layer addresses in EvtAdapterSetCapabilities both need to have the same
    // length
    if (m_currentLinkLayerAddress.Length != m_permanentLinkLayerAddress.Length)
    {
        LogError(FLAG_ADAPTER,
            "The client set permanent and current link layer addresses with different lengths.");
        goto Exit;
    }

    adapterGeneral.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
    adapterGeneral.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_2;
    adapterGeneral.Header.Revision = NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_2;


    //
    // Specify the Link Capabilities
    //
    adapterGeneral.MtuSize          = m_mtuSize;
    adapterGeneral.MaxXmitLinkSpeed = m_linkLayerCapabilities.MaxTxLinkSpeed;
    adapterGeneral.MaxRcvLinkSpeed  = m_linkLayerCapabilities.MaxRxLinkSpeed;

    //
    // Specify the current Link State
    //
    adapterGeneral.XmitLinkSpeed           = m_currentLinkState.TxLinkSpeed;
    adapterGeneral.RcvLinkSpeed            = m_currentLinkState.RxLinkSpeed;
    adapterGeneral.MediaDuplexState        = m_currentLinkState.MediaDuplexState;
    adapterGeneral.MediaConnectState       = m_currentLinkState.MediaConnectState;
    adapterGeneral.AutoNegotiationFlags    = m_currentLinkState.AutoNegotiationFlags;
    adapterGeneral.SupportedPauseFunctions = m_currentLinkState.SupportedPauseFunctions;

    //
    // Specify the Mac Capabilities
    //

    adapterGeneral.SupportedPacketFilters = m_receiveFilterCapabilities.SupportedPacketFilters;
    adapterGeneral.MaxMulticastListSize = static_cast<ULONG>(m_receiveFilterCapabilities.MaximumMulticastAddresses);
    adapterGeneral.SupportedStatistics =  NDIS_STATISTICS_XMIT_OK_SUPPORTED |
        NDIS_STATISTICS_RCV_OK_SUPPORTED |
        NDIS_STATISTICS_GEN_STATISTICS_SUPPORTED;

    // Set the MacAddressLength to be the length of the permanent link layer address,
    // this is fine since we require that both the permanent and current addresses
    // have the same length during EvtAdapterSetCapabilities.
    //
    // It is legal to have a zero length link layer address.
    //
    // Note: We don't need to enforce the same behavior if the client tries to change
    // its current address later. NDIS only enforces this behavior in NdisMSetMiniportAttributes.
    adapterGeneral.MacAddressLength = m_permanentLinkLayerAddress.Length;

    C_ASSERT(sizeof(m_permanentLinkLayerAddress.Address) == sizeof(adapterGeneral.PermanentMacAddress));
    C_ASSERT(sizeof(m_currentLinkLayerAddress.Address) == sizeof(adapterGeneral.CurrentMacAddress));

    if (adapterGeneral.MacAddressLength > 0)
    {
        RtlCopyMemory(
            adapterGeneral.CurrentMacAddress,
            m_currentLinkLayerAddress.Address,
            sizeof(adapterGeneral.CurrentMacAddress));

        RtlCopyMemory(
            adapterGeneral.PermanentMacAddress,
            m_permanentLinkLayerAddress.Address,
            sizeof(adapterGeneral.PermanentMacAddress));
    }

    //
    // Set the power management capabilities.
    //
    m_powerPolicy.InitializeNdisCapabilities(
        GetCombinedMediaSpecificWakeUpEvents(),
        GetCombinedSupportedProtocolOffloads(),
        &pmCapabilities);

    adapterGeneral.PowerManagementCapabilitiesEx = &pmCapabilities;

    auto nxDevice = GetNxDeviceFromHandle(GetDevice());
    adapterGeneral.SupportedOidList = const_cast<NDIS_OID *>(nxDevice->GetOidList());
    adapterGeneral.SupportedOidListLength = (ULONG) (nxDevice->GetOidListCount() * sizeof(NDIS_OID));
    adapterGeneral.RecvScaleCapabilities = RxScaling.RecvScaleCapabilities;

    //
    // The Remaining General Attributes Fields are not used for
    // a WDF NDIS model. Some of them come from Inf, some of them are
    // deprecated. Just set them to invalid values or default
    // values for diagnaosability purposes.
    //
    adapterGeneral.MediaType = (NDIS_MEDIUM)-1;
    adapterGeneral.PhysicalMediumType = NdisPhysicalMediumUnspecified;
    adapterGeneral.LookaheadSize = 0;
    adapterGeneral.MacOptions = 0;

    adapterGeneral.IfType = (NET_IFTYPE)-1;
    adapterGeneral.IfConnectorPresent = FALSE;

    ndisStatus = NdisMSetMiniportAttributes(
        m_ndisAdapterHandle,
        (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&adapterGeneral);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(FLAG_ADAPTER,
                 "NdisMSetMiniportAttributes failed, %!NDIS_STATUS!", ndisStatus);
        goto Exit;
    }






    m_flags.GeneralAttributesSet = TRUE;

Exit:

    return status;
}

void
NxAdapter::QueuePowerReferenceWorkItem(
    void
)
{
    m_powerRequiredWorkItemPending = TRUE;
    WdfWorkItemEnqueue(m_powerRequiredWorkItem);
}

void
NxAdapter::ClearGeneralAttributes(
    void
)
{
    //
    // Clear the Capabilities that are mandatory for the NetAdapter driver to set
    //
    RtlZeroMemory(&m_linkLayerCapabilities,     sizeof(NET_ADAPTER_LINK_LAYER_CAPABILITIES));
    m_mtuSize = 0;
    m_receiveFilterCapabilities = {};

    //
    // Initialize the optional capabilities to defaults.
    //
    NET_ADAPTER_LINK_STATE_INIT_DISCONNECTED(&m_currentLinkState);
}

void
NxAdapter::SetMtuSize(
    _In_ ULONG mtuSize
)
{
    m_mtuSize = mtuSize;

    if (m_flags.GeneralAttributesSet)
    {
        LogVerbose(FLAG_ADAPTER, "Updated MTU Size to %lu", mtuSize);

        IndicateMtuSizeChangeToNdis();
    }
}

ULONG
NxAdapter::GetMtuSize(
    void
) const
/*++
Routine Description:

    Get the Mtu of the given NETADAPTER.

Returns:
    The Mtu of the given NETADAPTER
--*/
{
    return m_mtuSize;
}

ULONG
NxAdapter::GetCombinedMediaSpecificWakeUpEvents(
    void
) const
{
    ULONG mediaSpecificWakeUpEvents = 0;

    for (auto const & extension : m_adapterExtensions)
    {
        mediaSpecificWakeUpEvents |= extension.GetNdisPmCapabilities().MediaSpecificWakeUpEvents;
    }

    return mediaSpecificWakeUpEvents;
}

ULONG
NxAdapter::GetCombinedSupportedProtocolOffloads(
    void
) const
{
    ULONG supportedProtocolOffloads = 0;

    for (auto const & extension : m_adapterExtensions)
    {
        supportedProtocolOffloads |= extension.GetNdisPmCapabilities().SupportedProtocolOffloads;
    }

    return supportedProtocolOffloads;
}

void
NxAdapter::IndicatePowerCapabilitiesToNdis(
    void
)
/*++
Routine Description:

    Sends a Power Capabilities status indication to NDIS.

--*/
{
    NDIS_PM_CAPABILITIES pmCapabilities;
    m_powerPolicy.InitializeNdisCapabilities(
        GetCombinedMediaSpecificWakeUpEvents(),
        GetCombinedSupportedProtocolOffloads(),
        &pmCapabilities);

    auto statusIndication =
        MakeNdisStatusIndication(
            m_ndisAdapterHandle, NDIS_STATUS_PM_HARDWARE_CAPABILITIES, pmCapabilities);

    NdisMIndicateStatusEx(m_ndisAdapterHandle, &statusIndication);
}

void
NxAdapter::IndicateCurrentLinkStateToNdis(
    void
)
/*++
Routine Description:

    Sends a Link status indication to NDIS.

--*/
{
    NDIS_LINK_STATE linkState = {0};

    linkState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    linkState.Header.Size = NDIS_SIZEOF_LINK_STATE_REVISION_1;
    linkState.Header.Revision = NDIS_LINK_STATE_REVISION_1;

    linkState.MediaConnectState    = m_currentLinkState.MediaConnectState;
    linkState.MediaDuplexState     = m_currentLinkState.MediaDuplexState;
    linkState.XmitLinkSpeed        = m_currentLinkState.TxLinkSpeed;
    linkState.RcvLinkSpeed         = m_currentLinkState.RxLinkSpeed;
    linkState.AutoNegotiationFlags = m_currentLinkState.AutoNegotiationFlags;
    linkState.PauseFunctions       =
        (NDIS_SUPPORTED_PAUSE_FUNCTIONS) m_currentLinkState.SupportedPauseFunctions;

    auto statusIndication =
        MakeNdisStatusIndication(
            m_ndisAdapterHandle, NDIS_STATUS_LINK_STATE, linkState);

    NdisMIndicateStatusEx(m_ndisAdapterHandle, &statusIndication);
}

void
NxAdapter::IndicateMtuSizeChangeToNdis(
    void
)
{
    auto statusIndication =
        MakeNdisStatusIndication(
            m_ndisAdapterHandle, NDIS_STATUS_L2_MTU_SIZE_CHANGE, m_mtuSize);

    NdisMIndicateStatusEx(m_ndisAdapterHandle, &statusIndication);
}

void
NxAdapter::IndicateCurrentLinkLayerAddressToNdis(
    void
)
{
    auto statusIndication = MakeNdisStatusIndication(
        m_ndisAdapterHandle,
        NDIS_STATUS_CURRENT_MAC_ADDRESS_CHANGE,
        m_currentLinkLayerAddress);

    NdisMIndicateStatusEx(m_ndisAdapterHandle, &statusIndication);
}

void
NdisMiniportDriverCharacteristicsInitialize(
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS & Characteristics
)
{
    Characteristics = {};

    Characteristics.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    Characteristics.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3;
    Characteristics.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3;
    Characteristics.MajorNdisVersion = NDIS_MINIPORT_MAJOR_VERSION;
    Characteristics.MinorNdisVersion = NDIS_MINIPORT_MINOR_VERSION;

    //
    // NDIS should not hold driver version information of WDF managed miniports
    //
    Characteristics.MajorDriverVersion = 0;
    Characteristics.MinorDriverVersion = 0;

    Characteristics.Flags = 0;
    Characteristics.Flags |= NDIS_WDF_PNP_POWER_HANDLING; // prevent NDIS from owning the driver dispatch table
    Characteristics.Flags |= NDIS_DRIVER_POWERMGMT_PROXY; // this is a virtual device so need updated capabilities

    Characteristics.InitializeHandlerEx = EvtNdisInitializeEx;
    Characteristics.HaltHandlerEx = EvtNdisHaltEx;
    Characteristics.PauseHandler = EvtNdisPause;
    Characteristics.RestartHandler = EvtNdisRestart;
    Characteristics.OidRequestHandler = EvtNdisOidRequest;
    Characteristics.DirectOidRequestHandler = EvtNdisDirectOidRequest;
    Characteristics.SendNetBufferListsHandler = EvtNdisSendNetBufferLists;
    Characteristics.ReturnNetBufferListsHandler = EvtNdisReturnNetBufferLists;
    Characteristics.CancelSendHandler = EvtNdisCancelSend;
    Characteristics.DevicePnPEventNotifyHandler = EvtNdisDevicePnpEventNotify;
    Characteristics.ShutdownHandlerEx = EvtNdisShutdownEx;
    Characteristics.CancelOidRequestHandler = EvtNdisCancelOidRequest;
    Characteristics.CancelDirectOidRequestHandler = EvtNdisCancelDirectOidRequest;
    Characteristics.SynchronousOidRequestHandler = EvtNdisSynchronousOidRequestHandler;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisInitializeEx(
    NDIS_HANDLE NdisAdapterHandle,
    NDIS_HANDLE UnusedContext,
    NDIS_MINIPORT_INIT_PARAMETERS * MiniportInitParameters
)
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to initialize a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called ndisPnPStartDevice routine.

Arguments:

    NdisAdapterHandle - Opaque Ndis handle for the Adapter

Return Value:

    NDIS_STATUS_xxx code

--*/
{
    UNREFERENCED_PARAMETER((UnusedContext, MiniportInitParameters));

    auto nxAdapter = static_cast<NxAdapter *>(
        NdisWdfGetAdapterContextFromAdapterHandle(NdisAdapterHandle));

    return NdisConvertNtStatusToNdisStatus(nxAdapter->NdisInitialize());
}

_Use_decl_annotations_
void
EvtNdisHaltEx(
    NDIS_HANDLE Adapter,
    NDIS_HALT_ACTION HaltAction
)
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to halt a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called NdisWdfPnpAddDevice routine.

Arguments:

    Adapter - NxAdapter reported as Context to NDIS.sys

--*/
{
    UNREFERENCED_PARAMETER(HaltAction);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportHalt);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisPause(
    NDIS_HANDLE Adapter,
    NDIS_MINIPORT_PAUSE_PARAMETERS * PauseParameters
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to pause the adapter.
    NetAdapterCx in response calls the clients EvtAdapterPause callback.

--*/
{
    UNREFERENCED_PARAMETER(PauseParameters);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportPause);

    return NDIS_STATUS_PENDING;
}

const GUID *
NxAdapter::GetInterfaceGuid(
    void
)
{
    return &m_interfaceGuid;
}

NET_LUID
NxAdapter::GetNetLuid(
    void
)
{
    return m_netLuid;
}

UNICODE_STRING const &
NxAdapter::GetBaseName(
    void
) const
{
    return m_baseName;
}

UNICODE_STRING const &
NxAdapter::GetInstanceName(
    void
) const
{
    return m_instanceName;
}

UNICODE_STRING const &
NxAdapter::GetDriverImageName(
    void
) const
{
    return m_driverImageName;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisRestart(
    NDIS_HANDLE Adapter,
    NDIS_MINIPORT_RESTART_PARAMETERS * RestartParameters
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to start (restart) the adapter.
    NetAdapterCx in response calls the clients EvtAdapterStart callback.

--*/
{
    UNREFERENCED_PARAMETER(RestartParameters);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportRestart);

    return NDIS_STATUS_PENDING;
}

_Use_decl_annotations_
void
EvtNdisSendNetBufferLists(
    NDIS_HANDLE Adapter,
    NET_BUFFER_LIST * NetBufferLists,
    NDIS_PORT_NUMBER PortNumber,
    ULONG SendFlags
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to send data over the network the adapter.

    NetAdapterCx in response calls the clients EvtAdapterSendNetBufferLists callback.

--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->SendNetBufferLists(
        NetBufferLists,
        PortNumber,
        SendFlags);
}

_Use_decl_annotations_
void
EvtNdisReturnNetBufferLists(
    NDIS_HANDLE Adapter,
    NET_BUFFER_LIST * NetBufferLists,
    ULONG ReturnFlags
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to return a NBL to the miniport.

    NetAdapterCx in response calls the clients EvtAdapterReturnNetBufferLists callback.

--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->ReturnNetBufferLists(
        NetBufferLists,
        ReturnFlags);
}

_Use_decl_annotations_
void
EvtNdisCancelSend(
    NDIS_HANDLE Adapter,
    void * CancelId
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to cancel a send NBL request.

--*/
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(CancelId);

    NT_ASSERTMSG("Missing Implementation", FALSE);
}

_Use_decl_annotations_
void
EvtNdisDevicePnpEventNotify(
    NDIS_HANDLE Adapter,
    NET_DEVICE_PNP_EVENT * NetDevicePnPEvent
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to report a Pnp Event.

--*/
{
    UNREFERENCED_PARAMETER(Adapter);

    if (NetDevicePnPEvent->DevicePnPEvent == NdisDevicePnPEventPowerProfileChanged) {
        if (NetDevicePnPEvent->InformationBufferLength == sizeof(ULONG)) {
            ULONG NdisPowerProfile = *((PULONG)NetDevicePnPEvent->InformationBuffer);

            if (NdisPowerProfile == NdisPowerProfileBattery) {
                LogInfo(FLAG_ADAPTER, "On NdisPowerProfileBattery");
            } else if (NdisPowerProfile == NdisPowerProfileAcOnLine) {
                LogInfo(FLAG_ADAPTER, "On NdisPowerProfileAcOnLine");
            } else {
                LogError(FLAG_ADAPTER, "Unexpected PowerProfile %d", NdisPowerProfile);
                NT_ASSERTMSG("Unexpected PowerProfile", FALSE);
            }
        }
    } else {
        LogInfo(FLAG_ADAPTER, "PnpEvent %!NDIS_DEVICE_PNP_EVENT! from Ndis.sys", NetDevicePnPEvent->DevicePnPEvent);
    }
}

_Use_decl_annotations_
void
EvtNdisShutdownEx(
    NDIS_HANDLE Adapter,
    NDIS_SHUTDOWN_ACTION ShutdownAction
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(ShutdownAction);

    NT_ASSERTMSG("Missing Implementation", FALSE);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisOidRequest(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * NdisOidRequest
)
{
    NDIS_STATUS ndisStatus;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    // intercept and forward to the NDIS translator.
    if (nxAdapter->NetClientOidPreProcess(*NdisOidRequest, ndisStatus))
    {
        return ndisStatus;
    }

    // NDIS_PM_PARAMETERS is always delivered through EvtNdisUpdatePMParameters, but in certain scenarios
    // NDIS will also send OID_PM_PARAMETERS with the same information to keep legacy behavior. There is
    // no reason to show that OID to media extensions though, so complete it right here to avoid confusion.
    if (NdisOidRequest->DATA.SET_INFORMATION.Oid == OID_PM_PARAMETERS)
    {
        return NDIS_STATUS_SUCCESS;
    }

    auto dispatchContext = new (&NdisOidRequest->MiniportReserved[0]) DispatchContext();
    dispatchContext->Signature = OidSignature;

    nxAdapter->DispatchOidRequest(NdisOidRequest, dispatchContext);

    return NDIS_STATUS_PENDING;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisDirectOidRequest(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * Request
)
{
    NDIS_STATUS ndisStatus;

    // intercept and forward to the NDIS translator.
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    if (nxAdapter->NetClientDirectOidPreProcess(*Request, ndisStatus))
    {
        return ndisStatus;
    }

    auto dispatchContext = new (&Request->MiniportReserved[0]) DispatchContext();
    dispatchContext->Signature = DirectOidSignature;

    auto const ntStatus = nxAdapter->DispatchDirectOidRequest(Request, dispatchContext);

    return NdisConvertNtStatusToNdisStatus(ntStatus);
}

_Use_decl_annotations_
void
EvtNdisCancelOidRequest(
    NDIS_HANDLE Adapter,
    void * RequestId
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(RequestId);
}

_Use_decl_annotations_
void
EvtNdisCancelDirectOidRequest(
    NDIS_HANDLE Adapter,
    void * RequestId
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(RequestId);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisSynchronousOidRequestHandler(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * Request
)
{
    NDIS_STATUS ndisStatus;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    // intercept and forward to the NDIS translator.
    if (nxAdapter->NetClientSynchrohousOidPreProcess(*Request, ndisStatus))
    {
        return ndisStatus;
    }

    return NdisConvertNtStatusToNdisStatus(STATUS_NOT_SUPPORTED);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::InitializeDatapath(
    NET_ADAPTER_DATAPATH_CALLBACKS const & Callbacks
)
{
    if (Callbacks.Size != 0)
    {
        RtlCopyMemory(
            &m_datapathCallbacks,
            &Callbacks,
            Callbacks.Size);
    }
    else
    {
        auto const defaultDatapathCallbacks = GetDefaultDatapathCallbacks();

        RtlCopyMemory(
            &m_datapathCallbacks,
            &defaultDatapathCallbacks,
            defaultDatapathCallbacks.Size);
    }

    CxDriverContext *driverContext = GetCxDriverContextFromHandle(WdfGetDriver());

    void * client;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        driverContext->TranslationAppFactory.CreateApp(
        &CxDispatch,
        reinterpret_cast<NET_CLIENT_ADAPTER>(this),
        &AdapterDispatch,
        &client,
        &m_clientDispatch),
        "Create NxApp failed. NxAdapter=%p", this);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_apps.append(wistd::unique_ptr<INxApp> { static_cast<INxApp *>(client) }));

    return STATUS_SUCCESS;
}

NETADAPTER
NxAdapter::GetFxObject(
    void
) const
{
    return m_adapter;
}

unique_miniport_reference
NxAdapter::GetMiniportReference(
    void
) const
{
    auto const miniport = GetNdisHandle();

    if (!NdisWdfMiniportTryReference(miniport))
    {
        return  {};
    }

    return unique_miniport_reference{ miniport };
}

NxAdapterExtension *
NxAdapter::GetExtension(
    _In_ NX_PRIVATE_GLOBALS const * ExtensionGlobals
)
{
    for (auto & extension : m_adapterExtensions)
    {
        if (extension.GetPrivateGlobals() == ExtensionGlobals)
        {
            return &extension;
        }
    }

    return nullptr;
}

NTSTATUS
NxAdapter::CreateNdisMiniport(
    void
)
{
    NT_FRE_ASSERT(m_ndisAdapterHandle == nullptr);

    auto device = GetDevice();
    auto driver = WdfDeviceGetDriver(device);
    NDIS_WDF_ADD_DEVICE_INFO addDeviceInfo = {};
    addDeviceInfo.DriverObject = WdfDriverWdmGetDriverObject(driver);
    addDeviceInfo.PhysicalDeviceObject = WdfDeviceWdmGetPhysicalDevice(device);
    addDeviceInfo.MiniportAdapterContext = reinterpret_cast<NDIS_HANDLE>(this);
    addDeviceInfo.WdfCxPowerManagement = TRUE;

    return NdisWdfPnPAddDevice(
        &addDeviceInfo,
        &m_ndisAdapterHandle);
}

_Use_decl_annotations_
void
NxAdapter::NdisMiniportCreationComplete(
    NDIS_WDF_COMPLETE_ADD_PARAMS const & Parameters
)
{
    m_interfaceGuid = Parameters.InterfaceGuid;
    m_netLuid = Parameters.NetLuid;
    m_mediaType = Parameters.MediaType;

    // The buffer pointed by Parameters.BaseName.Buffer is part of the NDIS_MINIPORT_BLOCK,
    // which is a context of the NETADAPTER object. So we don't need to make a
    // deep copy here
    m_baseName = Parameters.BaseName;
    m_instanceName = Parameters.AdapterInstanceName;
    m_driverImageName = Parameters.DriverImageName;
    m_executionContextKnobs =
        static_cast<EXECUTION_CONTEXT_RUNTIME_KNOBS*>(Parameters.ExecutionContextKnobs);

    if (m_baseName.Length >= (sizeof(m_tag) + 1) * sizeof(wchar_t))
    {
        // Build a tag based on the first sizeof(void *) chars of the interface GUID
        const auto baseNameBytes = reinterpret_cast<uint8_t *>(&m_baseName.Buffer[1]);

        auto tagBytes = reinterpret_cast<uint8_t *>(&m_tag);

        for (size_t i = 0; i < sizeof(m_tag); i++)
        {
            // Copy every other byte from the base name Unicode string
            tagBytes[i] = baseNameBytes[i * 2];
        }
    }
}

_Use_decl_annotations_
void
NxAdapter::SendNetBufferLists(
    NET_BUFFER_LIST * NetBufferLists,
    NDIS_PORT_NUMBER PortNumber,
    ULONG SendFlags
)
{
    m_nblDatapath.SendNetBufferLists(
        NetBufferLists,
        PortNumber,
        SendFlags);
}

_Use_decl_annotations_
void
NxAdapter::ReturnNetBufferLists(
    NET_BUFFER_LIST * NetBufferLists,
    ULONG ReturnFlags
)
{
    m_nblDatapath.ReturnNetBufferLists(
        NetBufferLists,
        ReturnFlags);
}

_Use_decl_annotations_
bool
NxAdapter::NetClientOidPreProcess(
    NDIS_OID_REQUEST & Request,
    NDIS_STATUS & NdisStatus
) const
{
    for (auto & app : m_apps)
    {
        if (m_clientDispatch->NdisOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_clientDispatch->NdisOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}

_Use_decl_annotations_
bool
NxAdapter::NetClientDirectOidPreProcess(
    NDIS_OID_REQUEST & Request,
    NDIS_STATUS & NdisStatus
) const
{
    for (size_t i = 0; i < m_apps.count(); i++)
    {
        auto & app = m_apps[i];

        if (m_clientDispatch->NdisDirectOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_clientDispatch->NdisDirectOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}

_Use_decl_annotations_
bool
NxAdapter::NetClientSynchrohousOidPreProcess(
    NDIS_OID_REQUEST & Request,
    NDIS_STATUS & NdisStatus
) const
{
    for (size_t i = 0; i < m_apps.count(); i++)
    {
        auto & app = m_apps[i];

        if (m_clientDispatch->NdisSynchronousOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_clientDispatch->NdisSynchronousOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::RegisterRingExtensions(
    void
)
{
    NET_EXTENSION_PRIVATE const virtualExtension = {
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1_SIZE,
        NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
        alignof(NET_FRAGMENT_VIRTUAL_ADDRESS),
        NetExtensionTypeFragment,
    };
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RegisterExtension(virtualExtension));

    if (m_mediaExtensionType == MediaExtensionType::Wifi)
    {
        NET_EXTENSION_PRIVATE const wifiExemptionAction =
        {
            NET_PACKET_EXTENSION_WIFI_EXEMPTION_ACTION_NAME,
            NET_PACKET_EXTENSION_WIFI_EXEMPTION_ACTION_VERSION_1_SIZE,
            NET_PACKET_EXTENSION_WIFI_EXEMPTION_ACTION_VERSION_1,
            alignof(NET_PACKET_WIFI_EXEMPTION_ACTION),
            NetExtensionTypePacket,
        };

        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(wifiExemptionAction));
    }

    if (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriver &&
        m_rxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver)
    {
        NET_EXTENSION_PRIVATE const returnExtension = {
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_NAME,
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_VERSION_1,
            alignof(NET_FRAGMENT_RETURN_CONTEXT),
            NetExtensionTypeFragment,
        };
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(returnExtension));
    }
    else if (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeSystem &&
             m_rxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver)
    {
        NET_EXTENSION_PRIVATE const dataBufferExtension = {
            NET_FRAGMENT_EXTENSION_DATA_BUFFER_NAME,
            NET_FRAGMENT_EXTENSION_DATA_BUFFER_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_DATA_BUFFER_VERSION_1,
            alignof(NET_FRAGMENT_DATA_BUFFER),
            NetExtensionTypeFragment,
        };
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(dataBufferExtension));
    }

    if (m_txCapabilities.DmaCapabilities != nullptr || m_rxCapabilities.DmaCapabilities != nullptr)
    {
        NET_EXTENSION_PRIVATE const logicalExtension = {
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
            alignof(NET_FRAGMENT_LOGICAL_ADDRESS),
            NetExtensionTypeFragment,
        };
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(logicalExtension));
    }
    else if (m_mediaExtensionType == MediaExtensionType::Mbb)
    {
        // we only keep MDL extension for backcompat, i.e. cxwmbclass driver
        NET_EXTENSION_PRIVATE const mdlExtension = {
            NET_FRAGMENT_EXTENSION_MDL_NAME,
            NET_FRAGMENT_EXTENSION_MDL_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_MDL_VERSION_1,
            alignof(NET_FRAGMENT_MDL),
            NetExtensionTypeFragment,
        };
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(mdlExtension));
    }

    if (m_ieee8021qTagCapabilties.Flags)
    {
        NET_EXTENSION_PRIVATE const ieee8021q =
        {
            NET_PACKET_EXTENSION_IEEE8021Q_NAME,
            NET_PACKET_EXTENSION_IEEE8021Q_VERSION_1_SIZE,
            NET_PACKET_EXTENSION_IEEE8021Q_VERSION_1,
            alignof(NET_PACKET_IEEE8021Q),
            NetExtensionTypePacket,
        };

        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(ieee8021q));
    }

    // Hash extension can be filled in NetCx 2.2 and above
    auto const driverVersion = MAKEVER(m_privateGlobals.ClientVersion.Major,
        m_privateGlobals.ClientVersion.Minor);

    if (RxScaling.IsSupported() && driverVersion > MAKEVER(2,1))
    {
        NET_EXTENSION_PRIVATE const rxScaling =
        {
            NET_PACKET_EXTENSION_HASH_NAME,
            NET_PACKET_EXTENSION_HASH_VERSION_1_SIZE,
            NET_PACKET_EXTENSION_HASH_VERSION_1,
            alignof(NET_PACKET_HASH),
            NetExtensionTypePacket,
        };

        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(rxScaling));
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxAdapter::SetDeviceFailed(
    NTSTATUS Status
)
{
    LogError(FLAG_ADAPTER,
        "Translator initiated WdfDeviceSetFailed: %!STATUS!",
        Status);

    WdfDeviceSetFailed(m_device, WdfDeviceFailedNoRestart);
}

_Use_decl_annotations_
void
NxAdapter::GetProperties(
    NET_CLIENT_ADAPTER_PROPERTIES * Properties
) const
{
    Properties->MediaType = m_mediaType;
    Properties->NetLuid = m_netLuid;
    Properties->DriverIsVerifying = !!MmIsDriverVerifyingByAddress(m_datapathCallbacks.EvtAdapterCreateTxQueue);
    Properties->NdisAdapterHandle = m_ndisAdapterHandle;

    // C++ object shared across ABI
    // this is not viable once the translator is removed from the Cx
    Properties->NblDispatcher = const_cast<INxNblDispatcher *>(static_cast<const INxNblDispatcher *>(&m_nblDatapath));
    Properties->PacketMonitorLowerEdge = reinterpret_cast<PKTMON_LOWEREDGE_HANDLE>(const_cast<PKTMON_EDGE_CONTEXT *>(&m_packetMonitor.GetLowerEdgeContext()));
    Properties->PacketMonitorComponentContext = reinterpret_cast<PKTMON_COMPONENT_HANDLE>(const_cast<PKTMON_COMPONENT_CONTEXT *>(&m_packetMonitor.GetComponentContext()));
}

_Use_decl_annotations_
void
NxAdapter::GetDatapathCapabilities(
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES * DatapathCapabilities
) const
{
    RtlZeroMemory(DatapathCapabilities, sizeof(*DatapathCapabilities));

    auto const & txCap = GetTxCapabilities();
    auto const & rxCap = GetRxCapabilities();

    if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeDriver) &&
        (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver))
    {
        DatapathCapabilities->RxMemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER;
    }
    else if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem) &&
             (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeSystem))
    {
        DatapathCapabilities->RxMemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ALLOCATE_AND_ATTACH;
    }
    else if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem) &&
             (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver))
    {
        DatapathCapabilities->RxMemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_OS_ONLY_ALLOCATE;
    }
    else
    {
        NT_FRE_ASSERT(false);
    }

    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES gsoHardwareCapabilities = {};
    m_offloadManager->GetGsoHardwareCapabilities(&gsoHardwareCapabilities);

    size_t maxL2HeaderSize;
    switch(m_mediaType)
    {
        case NdisMedium802_3:
            maxL2HeaderSize = 14; // sizeof(ETHERNET_HEADER)
            break;

        case NdisMediumNative802_11:
            maxL2HeaderSize = 32; // sizeof(DOT11_QOS_DATA_LONG_HEADER)
            break;

        default:
            maxL2HeaderSize = 0;
    }


    if (gsoHardwareCapabilities.MaximumOffloadSize > 0)
    {
        //
        // MaximumTxFragmentSize = Max GSO offload size + Max TCP header size + Max IP header size + Ethernet overhead
        //
        DatapathCapabilities->MaximumTxFragmentSize = txCap.PayloadBackfill + maxL2HeaderSize + gsoHardwareCapabilities.MaximumOffloadSize + MAX_IP_HEADER_SIZE + MAX_TCP_HEADER_SIZE;
    }
    else
    {
        DatapathCapabilities->MaximumTxFragmentSize = txCap.PayloadBackfill + maxL2HeaderSize + GetMtuSize();
    }

    DatapathCapabilities->MaximumRxFragmentSize = rxCap.MaximumFrameSize;
    DatapathCapabilities->MaximumNumberOfTxQueues = txCap.MaximumNumberOfQueues;
    DatapathCapabilities->MaximumNumberOfRxQueues = rxCap.MaximumNumberOfQueues;
    DatapathCapabilities->PreferredTxFragmentRingSize = txCap.FragmentRingNumberOfElementsHint;
    DatapathCapabilities->PreferredRxFragmentRingSize = rxCap.FragmentRingNumberOfElementsHint;
    DatapathCapabilities->MaximumNumberOfTxFragments = txCap.MaximumNumberOfFragments;
    DatapathCapabilities->TxPayloadBackfill = txCap.PayloadBackfill;

    if (rxCap.AllocationMode == NetRxFragmentBufferAllocationModeSystem)
    {
        if (rxCap.MappingRequirement == NetMemoryMappingRequirementNone)
        {
            DatapathCapabilities->RxMemoryConstraints.MappingRequirement = NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_NONE;
        }
        else if (rxCap.MappingRequirement == NetMemoryMappingRequirementDmaMapped)
        {
            DatapathCapabilities->RxMemoryConstraints.MappingRequirement = NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED;
        }
        else
        {
            NT_FRE_ASSERT(false);
        }
    }

    DatapathCapabilities->TxMemoryConstraints.MappingRequirement = static_cast<NET_CLIENT_MEMORY_MAPPING_REQUIREMENT>(txCap.MappingRequirement);
    DatapathCapabilities->TxMemoryConstraints.AlignmentRequirement = txCap.FragmentBufferAlignment;

    DatapathCapabilities->NominalMaxTxLinkSpeed = m_linkLayerCapabilities.MaxTxLinkSpeed;
    DatapathCapabilities->NominalMaxRxLinkSpeed = m_linkLayerCapabilities.MaxRxLinkSpeed;
    DatapathCapabilities->NominalMtu = m_mtuSize;
    DatapathCapabilities->MtuWithGso = m_mtuSize;
    DatapathCapabilities->MtuWithRsc = m_mtuSize;

    DatapathCapabilities->RxMemoryConstraints.MappingRequirement = static_cast<NET_CLIENT_MEMORY_MAPPING_REQUIREMENT> (rxCap.MappingRequirement);
    DatapathCapabilities->RxMemoryConstraints.AlignmentRequirement = rxCap.FragmentBufferAlignment;

    auto physicalDeviceObject = WdfDeviceWdmGetPhysicalDevice(GetDevice());
    ULONG preferredNumaNode = MM_ANY_NODE_OK;

    (void) IoGetDeviceNumaNode(physicalDeviceObject, (PUSHORT) &preferredNumaNode);

    if (DatapathCapabilities->RxMemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED)
    {
        DatapathCapabilities->RxMemoryConstraints.Dma.CacheEnabled =
            static_cast<NET_CLIENT_TRI_STATE> (rxCap.DmaCapabilities->CacheEnabled);
        DatapathCapabilities->RxMemoryConstraints.Dma.MaximumPhysicalAddress = rxCap.DmaCapabilities->MaximumPhysicalAddress;
        DatapathCapabilities->RxMemoryConstraints.Dma.PreferredNode = rxCap.DmaCapabilities->PreferredNode;

        if (DatapathCapabilities->RxMemoryConstraints.Dma.PreferredNode == MM_ANY_NODE_OK)
        {
            DatapathCapabilities->RxMemoryConstraints.Dma.PreferredNode = preferredNumaNode;
        }

        DatapathCapabilities->RxMemoryConstraints.Dma.DmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(rxCap.DmaCapabilities->DmaEnabler,
                                          WdfDmaDirectionReadFromDevice);
    }

    if (DatapathCapabilities->TxMemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED)
    {
        DatapathCapabilities->TxMemoryConstraints.Dma.CacheEnabled =
            static_cast<NET_CLIENT_TRI_STATE> (txCap.DmaCapabilities->CacheEnabled);
        DatapathCapabilities->TxMemoryConstraints.Dma.PreferredNode = txCap.DmaCapabilities->PreferredNode;

        auto dmaAdapter = WdfDmaEnablerWdmGetDmaAdapter(txCap.DmaCapabilities->DmaEnabler,
            WdfDmaDirectionWriteToDevice);

        DatapathCapabilities->TxMemoryConstraints.Dma.DmaAdapter = dmaAdapter;

        // Make sure we don't allocate more than what the device's DMA controller can address
        DMA_ADAPTER_INFO adapterInfo{ DMA_ADAPTER_INFO_VERSION1 };

        dmaAdapter->DmaOperations->GetDmaAdapterInfo(
            dmaAdapter,
            &adapterInfo);

        auto const maximumDmaLogicalAddress = (1UI64 << adapterInfo.V1.DmaAddressWidth) - 1;
        auto const maximumDriverLogicalAddress = txCap.DmaCapabilities->MaximumPhysicalAddress.QuadPart > 0
            ? static_cast<UINT64>(txCap.DmaCapabilities->MaximumPhysicalAddress.QuadPart)
            : 0xffffffffffffffff;

        DatapathCapabilities->TxMemoryConstraints.Dma.MaximumPhysicalAddress.QuadPart = min(maximumDmaLogicalAddress, maximumDriverLogicalAddress);

        if (DatapathCapabilities->TxMemoryConstraints.Dma.PreferredNode == MM_ANY_NODE_OK)
        {
            DatapathCapabilities->TxMemoryConstraints.Dma.PreferredNode = preferredNumaNode;
        }

        // We also need the PDO to let buffer manager call DMA APIs
        DatapathCapabilities->TxMemoryConstraints.Dma.PhysicalDeviceObject = physicalDeviceObject;
    }

#if defined(_X86_) || defined(_AMD64_) || defined(_ARM64_)
    DatapathCapabilities->FlushBuffers = FALSE;
#else
    DatapathCapabilities->FlushBuffers = TRUE;
#endif

}

_Use_decl_annotations_
void
NxAdapter::GetTxDemuxConfiguration(
    NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION * Configuration
) const
{
    auto const demux = m_txDemux.count() != 0
        ? reinterpret_cast<NET_CLIENT_ADAPTER_TX_DEMUX const *>(&m_txDemux[0])
        : reinterpret_cast<NET_CLIENT_ADAPTER_TX_DEMUX const *>(nullptr);

    NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION const configuration = {
        sizeof(*Configuration),
        demux,
        m_txDemux.count(),
    };
    *Configuration = configuration;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::AddNetClientPacketExtensionsToQueueCreationContext(
    NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    QUEUE_CREATION_CONTEXT& QueueCreationContext
)
{
    //
    // convert queue config's NET_CLIENT_EXTENSION to NET_EXTENSION_PRIVATE
    // since we passed API boundary, CX should use private throughout.
    // CX needs to use the stored extension within the adapter object at this point
    // so that the object holding info about packet extension is owned by CX.
    //
    for (size_t i = 0; i < ClientQueueConfig->NumberOfExtensions; i++)
    {
        auto const extension = QueryExtensionLocked(
            ClientQueueConfig->Extensions[i].Name,
            ClientQueueConfig->Extensions[i].Version,
            ClientQueueConfig->Extensions[i].Type);

        // protocol will ask for any extensions it supports but if
        // the driver has not advertised capability it will not be
        // registered and we do not add it to the creation context
        if (extension)
        {
            CX_RETURN_NTSTATUS_IF(
                STATUS_INSUFFICIENT_RESOURCES,
                ! QueueCreationContext.Extensions.append(*extension));
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::CreateTxQueue(
    void * ClientQueue,
    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    NET_CLIENT_QUEUE * AdapterQueue,
    NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch,
    NET_EXECUTION_CONTEXT * ExecutionContext,
    NET_EXECUTION_CONTEXT_DISPATCH const ** ExecutionContextDispatch
)
{
    auto lock = wil::acquire_wdf_wait_lock(m_dataPathControlLock);

    ULONG QueueId;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AllocateQueueId(m_txQueues, QueueId),
        "Failed to allocate QueueId.");

    QUEUE_CREATION_CONTEXT context;

    context.CurrentThread = KeGetCurrentThread();
    context.ClientQueueConfig = ClientQueueConfig;
    context.ClientDispatch = ClientDispatch;
    context.AdapterDispatch = AdapterDispatch;
    context.ParentObject = GetFxObject();
    context.QueueId = QueueId;
    context.ClientQueue = ClientQueue;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AddNetClientPacketExtensionsToQueueCreationContext(ClientQueueConfig, context),
        "Failed to transfer NetClientQueueConfig's packet extension to queue_creation_context");

    WdfObjectRequiredCallback callback{ GetFxObject(), m_datapathCallbacks.EvtAdapterCreateTxQueue };

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        callback.Invoke(reinterpret_cast<NETTXQUEUE_INIT*>(&context)),
        "Client failed tx queue creation. NETADAPTER=%p", GetFxObject());

    CX_RETURN_NTSTATUS_IF(
        STATUS_UNSUCCESSFUL,
        ! context.CreatedQueueObject);

    m_txQueues[QueueId] = wistd::move(context.CreatedQueueObject);
    *AdapterQueue = reinterpret_cast<NET_CLIENT_QUEUE>(
        GetNxQueueFromHandle((NETPACKETQUEUE)m_txQueues[QueueId].get()));

    *ExecutionContext = reinterpret_cast<NET_EXECUTION_CONTEXT>(context.ExecutionContext);
    *ExecutionContextDispatch = GetExecutionContextDispatch();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::CreateRxQueue(
    void * ClientQueue,
    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const * ClientDispatch,
    NET_CLIENT_QUEUE_CONFIG const * ClientQueueConfig,
    NET_CLIENT_QUEUE * AdapterQueue,
    NET_CLIENT_QUEUE_DISPATCH const ** AdapterDispatch,
    NET_EXECUTION_CONTEXT * ExecutionContext,
    NET_EXECUTION_CONTEXT_DISPATCH const ** ExecutionContextDispatch
)
{
    auto lock = wil::acquire_wdf_wait_lock(m_dataPathControlLock);

    ULONG QueueId;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AllocateQueueId(m_rxQueues, QueueId),
        "Failed to allocate QueueId.");

    QUEUE_CREATION_CONTEXT context;

    context.CurrentThread = KeGetCurrentThread();
    context.ClientQueueConfig = ClientQueueConfig;
    context.ClientDispatch = ClientDispatch;
    context.AdapterDispatch = AdapterDispatch;
    context.ParentObject = GetFxObject();
    context.QueueId = QueueId;
    context.ClientQueue = ClientQueue;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AddNetClientPacketExtensionsToQueueCreationContext(ClientQueueConfig, context),
        "Failed to transfer NetClientQueueConfig's packet extension to queue_creation_context");

    WdfObjectRequiredCallback callback{ GetFxObject(), m_datapathCallbacks.EvtAdapterCreateRxQueue };

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        callback.Invoke(reinterpret_cast<NETRXQUEUE_INIT*>(&context)),
        "Client failed rx queue creation. NETADAPTER=%p", GetFxObject());

    CX_RETURN_NTSTATUS_IF(
        STATUS_UNSUCCESSFUL,
        ! context.CreatedQueueObject);

    m_rxQueues[QueueId] = wistd::move(context.CreatedQueueObject);
    *AdapterQueue = reinterpret_cast<NET_CLIENT_QUEUE>(
        GetNxQueueFromHandle((NETPACKETQUEUE)m_rxQueues[QueueId].get()));

    *ExecutionContext = reinterpret_cast<NET_EXECUTION_CONTEXT>(context.ExecutionContext);
    *ExecutionContextDispatch = GetExecutionContextDispatch();

    return STATUS_SUCCESS;
}

inline
void
NxAdapter::DestroyQueue(
    NxQueue * Queue
)
{
    auto lock = wil::acquire_wdf_wait_lock(m_dataPathControlLock);

    switch (Queue->m_queueType)
    {
    case NxQueue::Type::Rx:
        m_rxQueues[Queue->m_queueId].reset();
        break;

    case NxQueue::Type::Tx:
        m_txQueues[Queue->m_queueId].reset();
        break;
    }
}

void
NxAdapter::DestroyQueue(
    NET_CLIENT_QUEUE AdapterQueue
)
{
    DestroyQueue(reinterpret_cast<NxQueue *>(AdapterQueue));
}

_Use_decl_annotations_
NONPAGEDX
void
NxAdapter::ReturnRxBuffer(
    NET_FRAGMENT_RETURN_CONTEXT_HANDLE RxBufferReturnContext
)
{
    m_evtReturnRxBuffer(GetFxObject(), RxBufferReturnContext);
}

void
NxAdapter::_EvtCleanup(
    _In_  WDFOBJECT NetAdapter
)
/*++
Routine Description:
    A WDF Event Callback

    This routine is called when the client driver WDFDEVICE object is being
    deleted. This happens when WDF is processing the REMOVE irp for the client.

--*/
{
    auto nxAdapter = GetNxAdapterFromHandle((NETADAPTER)NetAdapter);

    //
    // If the adapter has been disposed the workitem for async power reference
    // would have been disposed as well. Enqueueing it will be a no-op.
    // In case there is a thread in NDIS waiting for the power complete notification
    // unblock it.
    //
    if (nxAdapter->m_powerRequiredWorkItemPending) {
        NdisWdfAsyncPowerReferenceCompleteNotification(nxAdapter->m_ndisAdapterHandle,
            STATUS_INVALID_DEVICE_STATE);
        nxAdapter->m_powerRequiredWorkItemPending = FALSE;
    }

    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    nxDevice->PnpPower.AdapterDeleted(nxAdapter);

    if (nxAdapter->m_plugPlayNotificationHandle != nullptr)
    {
        IoUnregisterPlugPlayNotificationEx(nxAdapter->m_plugPlayNotificationHandle);
        // The above call ensures that before it returns any ongoing notification callbacks finishes
        // and no new notifications will be delivered, so it is safe to drop the object reference
        WdfObjectDereferenceWithTag(NetAdapter, PLUGPLAY_NOTIFICATION_TAG);
    }

    nxAdapter->PnpPower.Cleanup();
    if (nxAdapter->IsInitialized())
    {
        nxAdapter->EnqueueEvent(NxAdapter::Event::EvtCleanup);
    }
    
    return;
}

_Use_decl_annotations_
NDIS_HANDLE
EvtNdisWdmDeviceGetNdisAdapterHandle(
    DEVICE_OBJECT * DeviceObject
)
/*++
Routine Description:
    A Ndis Event Callback

    This event callback converts a WDF Device Object to the NDIS's
    miniport Adapter handle.
--*/
{
    UNREFERENCED_PARAMETER(DeviceObject);
    return nullptr;
}

_Use_decl_annotations_
void
EvtNdisUpdatePMParameters(
    NDIS_HANDLE Adapter,
    NDIS_PM_PARAMETERS * PmParameters
)
{
    auto nxAdapter = reinterpret_cast<NxAdapter *>(Adapter);
    nxAdapter->UpdateNdisPmParameters(PmParameters);
}

WDFDEVICE
NxAdapter::GetDevice(
    void
) const
{
    return m_device;
}

NTSTATUS
NxAdapter::NdisInitialize(
    void
)
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to initialize a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called ndisPnPStartDevice routine.

Arguments:

    NdisAdapterHandle - Opaque Ndis handle for the Adapter

Return Value:

    NDIS_STATUS_xxx code

--*/
{
    NTSTATUS status;

    status = SetRegistrationAttributes();
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    status = SetGeneralAttributes();
    if (!NT_SUCCESS(status)) {
        goto Exit;
    }

    //
    // Register packet extensions for offloads which are advertised by the client driver
    //
    m_offloadManager->RegisterExtensions();

    for (auto const &app : m_apps)
    {
        if (m_clientDispatch->OffloadInitialize)
        {
            status = m_clientDispatch->OffloadInitialize(app.get());

            if (status != STATUS_SUCCESS)
            {
                goto Exit;
            }
        }
    }

Exit:

    if (status == STATUS_SUCCESS)
    {
        EnqueueEvent(NxAdapter::Event::NdisMiniportInitializeEx);

        m_transitionComplete.Wait();
    }

    return status;
}

_Use_decl_annotations_
void
NxAdapter::NdisHalt(
    void
)
{
    // Starting with NetAdapterCx 2.2 we don't require client drivers to set their capabilities
    // again before calling NetAdapterStart a second time.
    if (!m_privateGlobals.IsClientVersionGreaterThanOrEqual(2, 2))
    {
        ClearGeneralAttributes();
    }

    // We can change these values here because NetAdapterStop only returns after m_IsHalted is set
    m_flags.GeneralAttributesSet = FALSE;

    m_packetExtensions.resize(0);
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathCreate(
    void
)
{
    for (auto &app : m_apps)
    {
        m_clientDispatch->CreateDatapath(app.get());
    }

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathDestroy(
    void
)
{
    for (auto &app : m_apps)
    {
        m_clientDispatch->DestroyDatapath(app.get());
    }

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStart(
    void
)
{
    for (auto &app : m_apps)
    {
        m_clientDispatch->StartDatapath(app.get());
    }

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStop(
    void
)
{
    for (auto &app : m_apps)
    {
        m_clientDispatch->StopDatapath(app.get());
    }

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStartRestartComplete(
    void
)
{
    (void)DatapathStart();

    return DatapathRestartComplete();
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStopPauseComplete(
    void
)
{
    (void)DatapathStop();

    return DatapathPauseComplete();
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathPauseComplete(
    void
)
{
    NdisMPauseComplete(m_ndisAdapterHandle);

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathRestartComplete(
    void
)
{
    NdisMRestartComplete(m_ndisAdapterHandle,
        NdisConvertNtStatusToNdisStatus(STATUS_SUCCESS));

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
void
NxAdapter::AsyncTransitionComplete(
    void
)
{
    m_transitionComplete.Set();
}

_Use_decl_annotations_
void
NxAdapter::InitializePowerManagement(
    void
)
/*
    This routine ensures this adapter will have one (and only one) WDFDEVICE power
    reference per adapter (excluding references taken by m_powerPolicy.m_idleRestrictions).
    This is done to account for:
    - Cases where both AOAC and SS are enabled
    - One of them is enabled.
    - Neither of them are enabled.

--*/
{
    if (!m_powerPolicy.IsPowerCapable())
    {
        LogInfo(
            FLAG_ADAPTER,
            "Network interface has no power capabilities, power management will not be enabled. NETADAPTER: %p",
            GetFxObject());

        return;
    }

    // This is step 1 of power management initialization for this network interface.
    // This will ensure the WDFDEVICE has 1 power reference before the implicit
    // power up is complete. After the start IRP is complete NDIS will drop this
    // reference if either the network interface is idle or AOAC references are zero.
    const auto powerReferenceStatus = PowerReference(true);

    CX_LOG_IF_NOT_NT_SUCCESS_MSG(
        powerReferenceStatus,
        "Failed to acquire initial power reference on parent device. "
        "Power management will not be enabled on this interface. "
        "WDFDEVICE: %p, NETADAPTER: %p",
            GetDevice(),
            GetFxObject());

    // If we acquired the initial power reference now it is time to tell NDIS to start
    // power managment operations, which will let it invoke WdfDeviceStopIdle/ResumeIdle.
    // Note that this is done on IRP_MN_DEVICE_START completion so it conforms to the
    // requirements described in WDF documentation.
    if (m_powerPolicy.ShouldAcquirePowerReferencesForAdapter() && NT_SUCCESS(powerReferenceStatus))
    {
        // When SS and AOAC structures are initialized in NDIS, they are both initialized
        // with stop flags so that SS / AoAC engines dont try to drop / acquire power refs
        // until the WDFDEVICE is ready. Now that we're ready this routine will clear
        // those SS / AoAC stop flags by calling into NdisWdfActionStartPowerManagement.

        const auto startPMStatus = NdisWdfPnpPowerEventHandler(
            GetNdisHandle(),
            NdisWdfActionStartPowerManagement,
            NdisWdfActionPowerNone);






        NT_FRE_ASSERT(startPMStatus == STATUS_SUCCESS);
    }
}

bool
NxAdapter::HasPermanentLinkLayerAddress(
    void
)
{
    return (m_permanentLinkLayerAddress.Length != 0);
}

bool
NxAdapter::HasCurrentLinkLayerAddress(
    void
)
{
    return (m_currentLinkLayerAddress.Length != 0);
}

_Use_decl_annotations_
void
NxAdapter::EvtLogTransition(
    _In_ SmFx::TransitionType TransitionType,
    _In_ StateId SourceState,
    _In_ EventId ProcessedEvent,
    _In_ StateId TargetState
)
{
    // StateMachineContext is NxAdapter object;
    WDFDEVICE wdfDevice = GetDevice();
    NxDevice* nxDevice = GetNxDeviceFromHandle(wdfDevice);

    TraceLoggingWrite(
        g_hNetAdapterCxEtwProvider,
        "NxAdapterStateTransition",
        TraceLoggingDescription("NxAdapterStateTransition"),
        TraceLoggingKeyword(NET_ADAPTER_CX_NXADAPTER_STATE_TRANSITION),
        TraceLoggingUInt32(nxDevice->GetDeviceBusAddress(), "DeviceBusAddress"),
        TraceLoggingHexUInt64(GetNetLuid().Value, "AdapterNetLuid"),
        TraceLoggingHexInt32(static_cast<INT32>(SourceState), "AdapterStateTransitionFrom"),
        TraceLoggingHexInt32(static_cast<INT32>(ProcessedEvent), "AdapterStateTransitionEvent"),
        TraceLoggingHexInt32(static_cast<INT32>(TargetState), "AdapterStateTransitionTo"));

    LogInfo(FLAG_DEVICE, "NETADAPTER: %p"
        " [%!SMFXTRANSITIONTYPE!]"
        " From: %!ADAPTERSTATE!,"
        " Event: %!ADAPTEREVENT!,"
        " To: %!ADAPTERSTATE!",
        GetFxObject(),
        static_cast<unsigned>(TransitionType),
        static_cast<unsigned>(SourceState),
        static_cast<unsigned>(ProcessedEvent),
        static_cast<unsigned>(TargetState));
}

_Use_decl_annotations_
void
NxAdapter::EvtLogMachineException(
    _In_ SmFx::MachineException exception,
    _In_ EventId relevantEvent,
    _In_ StateId relevantState
)
{
    UNREFERENCED_PARAMETER((exception, relevantEvent, relevantState));

    DbgBreakPoint();
}

_Use_decl_annotations_
void
NxAdapter::EvtMachineDestroyed(
    void
)
{
}

_Use_decl_annotations_
void
NxAdapter::EvtLogEventEnqueue(
    _In_ EventId relevantEvent
)
{
    UNREFERENCED_PARAMETER(relevantEvent);
}

_Use_decl_annotations_
NET_EXTENSION_PRIVATE const *
NxAdapter::QueryExtension(
    PCWSTR Name,
    ULONG Version,
    NET_EXTENSION_TYPE Type
) const
{
    for (auto const & extension : m_packetExtensions)
    {
        if (0 == wcscmp(Name, extension.m_extension.Name) &&
            Version == extension.m_extension.Version &&
            Type == extension.m_extension.Type)
        {
            return &extension.m_extension;
        }
    }

    return nullptr;
}

_Use_decl_annotations_
NET_EXTENSION_PRIVATE const *
NxAdapter::QueryExtensionLocked(
    PCWSTR Name,
    ULONG Version,
    NET_EXTENSION_TYPE Type
) const
{
    auto locked = wil::acquire_wdf_wait_lock(m_extensionCollectionLock);

    return QueryExtension(Name, Version, Type);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::RegisterExtension(
    NET_EXTENSION_PRIVATE const & Extension
)
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_DUPLICATE_NAME,
        QueryExtension(Extension.Name, Extension.Version, Extension.Type) != nullptr);

    NxExtension extension;
    CX_RETURN_IF_NOT_NT_SUCCESS(
        extension.Initialize(&Extension));

    auto locked = wil::acquire_wdf_wait_lock(m_extensionCollectionLock);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_packetExtensions.append(wistd::move(extension)));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxAdapter::SetDatapathCapabilities(
    NET_ADAPTER_TX_CAPABILITIES const *TxCapabilities,
    NET_ADAPTER_RX_CAPABILITIES const *RxCapabilities
)
{
    RtlCopyMemory(
        &m_txCapabilities,
        TxCapabilities,
        TxCapabilities->Size);

    if (TxCapabilities->DmaCapabilities != nullptr)
    {
        RtlCopyMemory(
            &m_txDmaCapabilities,
            TxCapabilities->DmaCapabilities,
            TxCapabilities->DmaCapabilities->Size);

        m_txCapabilities.DmaCapabilities = &m_txDmaCapabilities;
    }

    RtlCopyMemory(
        &m_rxCapabilities,
        RxCapabilities,
        RxCapabilities->Size);

    if (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriver)
    {
        m_evtReturnRxBuffer = m_rxCapabilities.EvtAdapterReturnRxBuffer;
    }

    if (RxCapabilities->DmaCapabilities != nullptr)
    {
        RtlCopyMemory(
            &m_rxDmaCapabilities,
            RxCapabilities->DmaCapabilities,
            RxCapabilities->DmaCapabilities->Size);

        m_rxCapabilities.DmaCapabilities = &m_rxDmaCapabilities;
    }
}

const NET_ADAPTER_TX_CAPABILITIES&
NxAdapter::GetTxCapabilities(
    void
) const
{
    NT_ASSERT(0 != m_txCapabilities.Size);
    return m_txCapabilities;
}


const NET_ADAPTER_RX_CAPABILITIES&
NxAdapter::GetRxCapabilities(
    void
) const
{
    NT_ASSERT(0 != m_rxCapabilities.Size);
    return m_rxCapabilities;
}

_Use_decl_annotations_
void
NxAdapter::SetLinkLayerCapabilities(
    NET_ADAPTER_LINK_LAYER_CAPABILITIES const & LinkLayerCapabilities
)
{
    RtlCopyMemory(&m_linkLayerCapabilities,
        &LinkLayerCapabilities,
        LinkLayerCapabilities.Size);
}

_Use_decl_annotations_
void
NxAdapter::SetReceiveFilterCapabilities(
    NET_ADAPTER_RECEIVE_FILTER_CAPABILITIES const & Capabilities
)
{
    RtlCopyMemory(&m_receiveFilterCapabilities,
        &Capabilities,
        Capabilities.Size);
}

_Use_decl_annotations_
void
NxAdapter::SetIeee8021qTagCapabiilities(
    NET_ADAPTER_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES const & Capabilities
)
{
    RtlCopyMemory(&m_ieee8021qTagCapabilties,
        &Capabilities,
        Capabilities.Size);
}

_Use_decl_annotations_
void
NxAdapter::SetPowerOffloadArpCapabilities(
    NET_ADAPTER_POWER_OFFLOAD_ARP_CAPABILITIES const * Capabilities
)
{
    m_powerPolicy.SetPowerOffloadArpCapabilities(Capabilities);

    if (m_flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetPowerOffloadNSCapabilities(
    NET_ADAPTER_POWER_OFFLOAD_NS_CAPABILITIES const * Capabilities
)
{
    m_powerPolicy.SetPowerOffloadNSCapabilities(Capabilities);

    if (m_flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetWakeBitmapCapabilities(
    NET_ADAPTER_WAKE_BITMAP_CAPABILITIES const * Capabilities
)
{
    m_powerPolicy.SetWakeBitmapCapabilities(Capabilities);

    if (m_flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetMagicPacketCapabilities(
    NET_ADAPTER_WAKE_MAGIC_PACKET_CAPABILITIES const * Capabilities
)
{
    m_powerPolicy.SetMagicPacketCapabilities(Capabilities);

    if (m_flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetEapolPacketCapabilities(
    NET_ADAPTER_WAKE_EAPOL_PACKET_CAPABILITIES const * Capabilities
)
{
    m_powerPolicy.SetEapolPacketCapabilities(Capabilities);

    if (m_flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetWakeMediaChangeCapabilities(
    NET_ADAPTER_WAKE_MEDIA_CHANGE_CAPABILITIES const * Capabilities
)
{
    m_powerPolicy.SetWakeMediaChangeCapabilities(Capabilities);

    if (m_flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetWakePacketFilterCapabilities(
    NET_ADAPTER_WAKE_PACKET_FILTER_CAPABILITIES const * Capabilities
)
{
    m_powerPolicy.SetWakePacketFilterCapabilities(Capabilities);

    if (m_flags.GeneralAttributesSet)
    {
        IndicatePowerCapabilitiesToNdis();
    }
}

_Use_decl_annotations_
void
NxAdapter::SetCurrentLinkState(
    NET_ADAPTER_LINK_STATE const & CurrentLinkState
)
{
    auto const linkStateChanged =
        m_currentLinkState.TxLinkSpeed != CurrentLinkState.TxLinkSpeed ||
        m_currentLinkState.RxLinkSpeed != CurrentLinkState.RxLinkSpeed ||
        m_currentLinkState.MediaConnectState != CurrentLinkState.MediaConnectState ||
        m_currentLinkState.MediaDuplexState != CurrentLinkState.MediaDuplexState ||
        m_currentLinkState.SupportedPauseFunctions != CurrentLinkState.SupportedPauseFunctions ||
        m_currentLinkState.AutoNegotiationFlags != CurrentLinkState.AutoNegotiationFlags;

    if (linkStateChanged)
    {
        RtlCopyMemory(
            &m_currentLinkState,
            &CurrentLinkState,
            CurrentLinkState.Size);

        if (m_flags.GeneralAttributesSet)
        {
            IndicateCurrentLinkStateToNdis();
        }
    }
}

void
NxAdapter::GetTriageInfo(
    void
)
{
    g_NetAdapterCxTriageBlock.NxAdapterLinkageOffset = FIELD_OFFSET(NxAdapter, m_linkage);
}

_Use_decl_annotations_
void NxAdapter::ReportWakeReasonPacket(
    const NET_ADAPTER_WAKE_REASON_PACKET * Reason
) const
{
    Verifier_VerifyNetPowerUpTransition(GetPrivateGlobals(), GetNxDeviceFromHandle(m_device));

    if (Reason->PatternId == NetAdapterWakeFilterPatternId)
    {
        LogInfo(FLAG_POWER,
            "NETADAPTER %p reported wake. Reason: Packet matched receive filter",
                GetFxObject());
    }
    else if (Reason->PatternId == NetAdapterWakeMagicPatternId)
    {
        LogInfo(FLAG_POWER,
            "NETADAPTER %p reported wake. Reason: Magic Packet",
                GetFxObject());
    }
    else if (Reason->PatternId == NetAdapterWakeEapolPatternId)
    {
        LogInfo(FLAG_POWER,
            "NETADAPTER %p reported wake. Reason: EAPOL Packet",
                GetFxObject());
    }
    else
    {
        LogInfo(FLAG_POWER,
            "NETADAPTER %p reported wake. Reason: Packet matched a bitmap pattern (%u)",
                GetFxObject(),
                Reason->PatternId);
    }

    if (Reason->WakePacket == WDF_NO_HANDLE)
    {
        LogWarning(FLAG_POWER,
            "NETADAPTER %p reported wake reason packet without payload",
                GetFxObject());

        return;
    }

    size_t savedPacketSize;
    auto wakePacketBuffer = WdfMemoryGetBuffer(Reason->WakePacket, &savedPacketSize);
    if (savedPacketSize > MAX_ETHERNET_PACKET_SIZE)
    {
        savedPacketSize = MAX_ETHERNET_PACKET_SIZE;
    }
    auto const wakePacketSize = RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8) + RTL_NUM_ALIGN_UP(savedPacketSize, 8);
    auto const wakeReasonSize =
        RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8) +
        wakePacketSize;

    NT_FRE_ASSERT(wakePacketSize <= ULONG_MAX);
    NT_FRE_ASSERT(wakeReasonSize <= ULONG_MAX);

    size_t reservedWakeReasonSize;
    auto wakeReason = static_cast<PNDIS_PM_WAKE_REASON>(WdfMemoryGetBuffer(m_wakeReasonMemory, &reservedWakeReasonSize));
    RtlZeroMemory(wakeReason, reservedWakeReasonSize);
    wakeReason->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    wakeReason->Header.Revision = NDIS_PM_WAKE_REASON_REVISION_1;
    wakeReason->Header.Size = NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1;
    wakeReason->WakeReason = NdisWakeReasonPacket;
    wakeReason->InfoBufferOffset = RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8);
    wakeReason->InfoBufferSize = static_cast<ULONG>(wakePacketSize);

    auto wakePacket = reinterpret_cast<PNDIS_PM_WAKE_PACKET>((PUCHAR)wakeReason + RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1, 8));
    wakePacket->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    wakePacket->Header.Revision = NDIS_PM_WAKE_PACKET_REVISION_1;
    wakePacket->Header.Size = NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1;
    wakePacket->PatternId = Reason->PatternId;
    wakePacket->OriginalPacketSize = Reason->OriginalPacketSize;
    wakePacket->SavedPacketSize = static_cast<ULONG>(savedPacketSize);
    wakePacket->SavedPacketOffset = RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8);

    PUCHAR data = (PUCHAR)wakePacket + RTL_NUM_ALIGN_UP(NDIS_SIZEOF_PM_WAKE_PACKET_REVISION_1, 8);
    RtlCopyMemory(data, wakePacketBuffer, savedPacketSize);

    auto wakeIndication = MakeNdisStatusIndication(
        m_ndisAdapterHandle, NDIS_STATUS_PM_WAKE_REASON, *wakeReason);
    wakeIndication.StatusBufferSize = static_cast<ULONG>(wakeReasonSize);

    NdisMIndicateStatusEx(m_ndisAdapterHandle, &wakeIndication);
}

_Use_decl_annotations_
void NxAdapter::ReportWakeReasonMediaChange(
    NET_IF_MEDIA_CONNECT_STATE Reason
) const
{
    Verifier_VerifyNetPowerUpTransition(GetPrivateGlobals(), GetNxDeviceFromHandle(m_device));

    LogInfo(FLAG_POWER,
        "NETADAPTER %p reported wake. Reason: Media state change (%!NET_IF_MEDIA_CONNECT_STATE!)",
            GetFxObject(),
            Reason);

    NDIS_PM_WAKE_REASON wakeReason;
    wakeReason.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    wakeReason.Header.Revision = NDIS_PM_WAKE_REASON_REVISION_1;
    wakeReason.Header.Size = NDIS_SIZEOF_PM_WAKE_REASON_REVISION_1;
    switch (Reason)
    {
    case MediaConnectStateDisconnected:
        wakeReason.WakeReason = NdisWakeReasonMediaDisconnect;
        break;
    case MediaConnectStateConnected:
        wakeReason.WakeReason = NdisWakeReasonMediaConnect;
        break;
    default:
        Verifier_ReportViolation(
            GetPrivateGlobals(),
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetAdapterWakeReasonMediaChange,
            0,
            0);
    }
    wakeReason.InfoBufferOffset = 0;
    wakeReason.InfoBufferSize = 0;
    auto wakeIndication = MakeNdisStatusIndication(
        m_ndisAdapterHandle, NDIS_STATUS_PM_WAKE_REASON, wakeReason);

    NdisMIndicateStatusEx(m_ndisAdapterHandle, &wakeIndication);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::PowerReference(
    bool WaitForD0
)
{
    InterlockedIncrement(&m_powerReferences);

    if (!m_powerPolicy.ShouldAcquirePowerReferencesForAdapter())
    {
        LogInfo(FLAG_POWER,
            "%!FUNC! Not acquiring PowerReference because adapter indicated not to acquire references on its behalf.");
        return STATUS_SUCCESS;
    }

    const auto powerReferenceStatus = WdfDeviceStopIdleWithTag(
        GetDevice(),
        WaitForD0,
        m_tag);

    if (NT_SUCCESS(powerReferenceStatus))
    {
        InterlockedIncrement(&m_successfulPowerReferences);
    }
    else
    {
        LogWarning(FLAG_ADAPTER, "Power reference on parent device failed. WDFDEVICE: %p, NETADAPTER: %p",
            GetDevice(),
            GetFxObject());
    }

    return powerReferenceStatus;
}

void
NxAdapter::PowerDereference(
    void
)
{
    InterlockedDecrement(&m_powerReferences);

    if (-1 != NxInterlockedDecrementFloor(&m_successfulPowerReferences, 0))
    {
        NT_FRE_ASSERT(m_powerPolicy.ShouldAcquirePowerReferencesForAdapter());
        WdfDeviceResumeIdleWithTag(
            GetDevice(),
            m_tag);
    }
    else
    {
        LogWarning(FLAG_ADAPTER, "Dropped reference for failed WdfDeviceStopIdle call. NETADAPTER %p", GetFxObject());
    }
}

void
NxAdapter::ReleaseOutstandingPowerReferences(
    void
)
{
    LONG outstandingReferences;

    while ((outstandingReferences = NxInterlockedDecrementFloor(&m_successfulPowerReferences, 0)) > -1)
    {
        LogWarning(FLAG_ADAPTER, "Releasing outstanding reference for NETADAPTER %p. Remaining references %d",
            GetFxObject(),
            outstandingReferences);

        WdfDeviceResumeIdleWithTag(GetDevice(), m_tag);
    }
}

void
NxAdapter::WifiDestroyPeerAddressDatapath(
    size_t Demux
)
{
    for (auto & app : m_apps)
    {
        m_clientDispatch->WifiDestroyPeerAddressDatapath(app.get(), Demux);
    }
}

NxPacketMonitor &
NxAdapter::GetPacketMonitor(
    void
)
{
    return m_packetMonitor;
}

void
NxAdapter::GetIeee8021qHardwareCapabilities(
        NET_CLIENT_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES * HardwareCapabilities
    ) const
{
    HardwareCapabilities->Size = sizeof(NET_CLIENT_OFFLOAD_IEEE8021Q_TAG_CAPABILITIES);
    HardwareCapabilities->Flags = static_cast<NET_CLIENT_ADAPTER_OFFLOAD_IEEE8021Q_TAG_FLAGS>(m_ieee8021qTagCapabilties.Flags);
}

void
NxAdapter::PauseOffloads(
    void
)
{
    for (auto & app : m_apps)
    {
        m_clientDispatch->PauseOffloadCapabilities(app.get());
    }

    m_isOffloadPaused = true;
}

void
NxAdapter::ResumeOffloads(
    void
)
{
    for (auto & app : m_apps)
    {
        m_clientDispatch->ResumeOffloadCapabilities(app.get());
    }

    m_isOffloadPaused = false;
}

_Use_decl_annotations_
void
NxAdapter::UpdateNdisPmParameters(
    NDIS_PM_PARAMETERS * PmParameters
)
{
    // First let extensions see the PM parameters, then update internal state
    for (auto const & extension : m_adapterExtensions)
    {
        extension.UpdateNdisPmParameters(GetFxObject(), PmParameters);
    }

    m_powerPolicy.SetParameters(PmParameters);
}
