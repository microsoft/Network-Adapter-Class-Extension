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
#include <net/wifi/exemptionactiontypes_p.h>
#include <net/ieee8021qtypes_p.h>
#include <net/packethashtypes_p.h>
#include <net/databuffer_p.h>

#include "WdfObjectCallback.hpp"
#include "NxAdapter.hpp"

#include "NxDevice.hpp"
#include "NxDriver.hpp"
#include "NxConfiguration.hpp"
#include "NxExtension.hpp"
#include "NxQueue.hpp"
#include "NxUtility.hpp"
#include "NxDefaultDatapath.hpp"
#include "NxMemory.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "ndisminiport/NxNdisMiniport.hpp"
#include "ndisminiport/NxNdisPnpPower.hpp"
#include "verifier.hpp"
#include "version.hpp"


#include "NxAdapter.tmh"


static void *
    PLUGPLAY_NOTIFICATION_TAG = reinterpret_cast<void *>('NPnP');

//
// I/O subsystem callbacks
//

static
DEVICE_INTERFACE_NOTIFICATION_CALLBACK
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
    _Out_ NET_EXECUTION_CONTEXT * ExecutionContextHandle,
    _Out_ NET_EXECUTION_CONTEXT_DISPATCH const ** ExecutionContextDispatch
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->CreateTxQueue(
        ClientContext,
        ClientDispatch,
        ClientQueueConfig,
        AdapterQueue,
        AdapterDispatch,
        ExecutionContextHandle,
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
    _Out_ NET_EXECUTION_CONTEXT * ExecutionContextHandle,
    _Out_ NET_EXECUTION_CONTEXT_DISPATCH const ** ExecutionContextDispatch
)
{
    return reinterpret_cast<NxAdapter *>(Adapter)->CreateRxQueue(
        ClientContext,
        ClientDispatch,
        ClientQueueConfig,
        AdapterQueue,
        AdapterDispatch,
        ExecutionContextHandle,
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

NONPAGEDX
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NET_MEMORY_MDL_WRAPPER *
NetClientGetMdlForDriverAllocatedMemory(
    _In_ NET_CLIENT_ADAPTER Adapter,
    _In_ ULONG64 MemoryId
)
{
    return reinterpret_cast<NxAdapter*>(Adapter)->GetMdlForDriverAllocatedMemory(MemoryId);
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
    &NetClientGetMdlForDriverAllocatedMemory,
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

    if (nxAdapter->IsDevicelessAdapter())
    {
        return nullptr;
    }

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
    , m_device(AdapterInit->ParentDevice)
    , m_mediaExtensionType(AdapterInit->ExtensionType)
    , PnpPower(this)
    , m_packetMonitor(NxPacketMonitor(*this))
{
}

NTSTATUS
NetInterfaceChangeNotification(
    DeviceInterfaceNotificationType NotificationType,
    SymbolicLinkNameType * SymbolicLinkName,
    void * Context
    )
{
    if (NotificationType == DeviceInterfaceNotificationType::Arrival)
    {
#ifdef NETCX_KMDF
        OBJECT_ATTRIBUTES objAttributes;

        InitializeObjectAttributes(
            &objAttributes,
            SymbolicLinkName,
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
#else
        DEVPROPTYPE propType{};
        GUID interfaceGuid{};
        ULONG size = sizeof(interfaceGuid);

        CONFIGRET cr = CM_Get_Device_Interface_PropertyW(
            SymbolicLinkName,
            &DEVPKEY_NetworkInterface_Guid,
            &propType,
            reinterpret_cast<BYTE *>(&interfaceGuid),
            &size,
            0);

        auto nxAdapter = static_cast<NxAdapter *>(Context);

        CX_RETURN_NTSTATUS_IF_MSG(
            CM_MapCrToWin32Err(
                cr,
                ERROR_UNIDENTIFIED_ERROR),
            cr != CR_SUCCESS,
            "CM_Get_Device_Interface_PropertyW failed. NETADAPTER %p",
            nxAdapter->GetFxObject());

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            propType != DEVPROP_TYPE_GUID,
            "Invalid DEVPKEY_NetworkInterface_Guid data type. NETADAPTER %p",
            nxAdapter->GetFxObject());

        if (*nxAdapter->GetInterfaceGuid() == interfaceGuid)
        {
            // If we're here it is because NetAdapterStart was already invoked, which means
            // we are guaranteed to have a reference on the NETADAPTER object, thus it is safe
            // to use the NxAdapter context
            nxAdapter->PnpPower.IoInterfaceArrived();
        }
#endif
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::InitializeDeviceBackedAdapter(
    AdapterInit const * AdapterInit
)
{
    m_ndisMiniport = wil::make_unique_nothrow<NxNdisPnpMiniport>(this, m_device);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_ndisMiniport);

#ifdef NETCX_UMDF
    m_ndisMiniport->SetBridgeToKernel(&m_bridgeToKernel);
#endif

    //
    // Report the newly created device to Ndis.sys
    //
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_ndisMiniport->CreateNdisMiniport(),
        "Failed to create NDIS miniport");

    //
    // Creating NxNdisPnpPower requires NDIS miniport is created successfully.
    //
    m_ndisPnpPower = wil::make_unique_nothrow<NxNdisPnpPower>(m_ndisMiniport->GetNdisHandle(), m_device);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_ndisPnpPower);

#ifdef NETCX_UMDF
    m_ndisPnpPower->SetBridgeToKernel(&m_bridgeToKernel);
#endif

    //
    // Initializing NxPowerPolicy requires NDIS miniport is created successfully.
    //

    bool const allowS0Idle = m_mediaExtensionType == MediaExtensionType::Mbb;

    m_powerPolicy = wil::make_unique_nothrow<NxPowerPolicy>(
        m_device,
        GetNxDeviceFromHandle(m_device)->GetPowerPolicyEventCallbacks(),
        m_adapter,
        AdapterInit->ShouldAcquirePowerReferencesForAdapter,
        allowS0Idle,
        *m_ndisPnpPower);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_powerPolicy);

    CX_RETURN_IF_NOT_NT_SUCCESS(m_powerPolicy->Initialize());

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        RegisterDeviceInterfaceNotification(
            GUID_DEVINTERFACE_NET,
            GetPrivateGlobals()->NxDriver->GetFxObject(),
            NetInterfaceChangeNotification,
            this,
            m_deviceInterfaceNotification),
        "Failed to register device interface notification. NETADAPTER %p",
        GetFxObject());

    WdfObjectReferenceWithTag(GetFxObject(), PLUGPLAY_NOTIFICATION_TAG);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PnpPower.Initialize(),
        "Failed to initialize Adapter PnpPower state machine");

    auto nxDevice = GetNxDeviceFromHandle(GetDevice());
    // Inform the Device PnpPower state machine so that it can bring the
    // new adapter PnP/Power to a consistent state
    nxDevice->PnpPower.AdapterCreated(this);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::InitializeDevicelessAdapter(
    AdapterInit const * AdapterInit
)
{
    m_interfaceGuid = AdapterInit->DevicelessNetworkInterfaceGuid;

    m_ndisMiniport = wil::make_unique_nothrow<NxNdisLwMiniport>(
        this,
        AdapterInit->DevicelessNetworkInterfaceGuid);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_ndisMiniport);

    //
    // Report the newly created device to Ndis.sys
    //
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        m_ndisMiniport->CreateNdisMiniport(),
        "Failed to create NDIS miniport");

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        PnpPower.Initialize(),
        "Failed to initialize Adapter PnpPower state machine");

    return STATUS_SUCCESS;
}

bool
NxAdapter::IsDevicelessAdapter(
    void
) const
{
    return m_device == WDF_NO_HANDLE;
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

#ifdef NETCX_UMDF
    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_bridgeToKernel.OpenHandle(
            GetNxDeviceFromHandle(m_device)->GetPnpInstanceId()));

    CxDriverContext *cxDriverContext = GetCxDriverContextFromHandle(WdfGetDriver());
    wistd::unique_ptr<CanaryIoctl> ioctl;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        cxDriverContext->SendCanaryIoctl(
            m_bridgeToKernel.GetHandle(),
            IOCTL_NETCXRD_ADAPTER_CANARY,
            ioctl));

    m_bridgeToKernel.SetCanaryIoctl(wistd::move(ioctl));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_bridgeToKernel.RequestConnectToAlpc(
            this));
#endif

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

#ifdef NETCX_UMDF
    if (m_txDemux.count())
    {
        m_bridgeToKernel.SetClientTxDemuxConfiguration(&m_txDemux[0], m_txDemux.count());
    }
#else
    if (auto const demux = GetTxPeerAddressDemux())
    {
            // It is guranteed we'll have exactly one app in the m_apps array.
            // Support for multiple apps has never been implemented, and the m_apps 
            // array needs to be replaced with a simple pointer
            CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
                m_apps[0]->TxPeerDemuxTableInitialize(demux->Range),
                "Failed to initialize TxPeerDemuxTable ");
    }
#endif

    ClearGeneralAttributes();

#ifdef NETCX_KMDF
    auto ioObject = m_device != nullptr
        ? WdfDeviceWdmGetDeviceObject(m_device)
        : reinterpret_cast<DEVICE_OBJECT *>(WdfDriverWdmGetDriverObject(WdfGetDriver()));

    StateMachineEngineConfig smConfig(ioObject, NETADAPTERCX_TAG);
#else
    StateMachineEngineConfig smConfig;
#endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxAdapterStateMachine::Initialize(smConfig),
        "Failed to initialize NxAdapter state machine");

    // Clear the initial state machine transition
    m_transitionComplete.Clear();

    if (AdapterInit->ParentDevice != WDF_NO_HANDLE)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(InitializeDeviceBackedAdapter(AdapterInit));
    }
    else
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(InitializeDevicelessAdapter(AdapterInit));
    }

#ifdef NETCX_KMDF
    m_nblDatapath.SetNdisHandle(GetNdisHandle());
#endif

    // We let the driver start by default
    CX_RETURN_IF_NOT_NT_SUCCESS(PnpPower.NetAdapterStart(WdfGetDriver()));

    DECLARE_CONST_UNICODE_STRING(keyword, L"*RSS");
    CX_RETURN_IF_NOT_NT_SUCCESS(
        QueryStandardizedKeyword(keyword, RxScaling.RssKeywordEnabled));

    DECLARE_CONST_UNICODE_STRING(numRssQueues, L"*NumRssQueues");
    if (!NT_SUCCESS(QueryStandardizedKeyword(numRssQueues, RxScaling.DefaultNumberOfQueues)))
    {
        LogWarning(FLAG_ADAPTER, "*NumRssQueues keyword not present. NETADAPTER %p",
            m_adapter);
    }

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
#ifdef NETCX_KMDF
            m_apps[0]->SetUseWiFiCxDemuxCallback(true);
#endif
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

NDIS_HANDLE
NxAdapter::GetNdisHandle(
    void
) const
{
    return m_ndisMiniport
        ? m_ndisMiniport->GetNdisHandle()
        : nullptr;
}

#ifdef NETCX_UMDF
BridgeToKernel &
NxAdapter::GetBridgeToKernel(
    void
)
{
    return m_bridgeToKernel;
}

BridgeToKernel const &
NxAdapter::GetBridgeToKernel(
    void
) const
{
    return m_bridgeToKernel;
}
#endif

NTSTATUS
NxAdapter::SetRegistrationAttributes(
    void
)
{
    auto const ndisStatus = m_ndisMiniport->SetRegistrationAttributes();

    auto const status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(FLAG_ADAPTER,
        "NdisMSetMiniportAttributes failed with registration attributes, %!NDIS_STATUS!", ndisStatus);
    }

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
    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_NOT_SUPPORTED,
        m_receiveFilterCapabilities.SupportedPacketFilters == 0,
        "NETADAPTER %p",
        m_adapter);

    // Do not support requests to set unsupported packet filters.
    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_NOT_SUPPORTED,
        PacketFilter & ~(m_receiveFilterCapabilities.SupportedPacketFilters)
            && GetMediaType() != NdisMediumNative802_11,
        "NETADAPTER %p",
        m_adapter);

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
    NDIS_PM_CAPABILITIES                     pmCapabilities{};
    NTSTATUS                                 status = STATUS_INVALID_PARAMETER;
    NDIS_STATUS                              ndisStatus;

    if (m_rxCapabilities.Size == 0 || m_txCapabilities.Size == 0)
    {
        LogError(FLAG_ADAPTER,
            "Client missed or incorrectly called NetAdapterSetDatapathCapabilities");
        goto Exit;
    }

    bool const driverManagesBuffers =
        (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriver) &&
        (m_rxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver);

    bool const osManagesBuffers =
        (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeSystem) &&
        (m_rxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeSystem);

    bool const driverManagedBuffersV2 =
        (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriverV2) &&
        (m_rxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver);

    bool allowedConfiguration =
#ifdef NETCX_KMDF
        osManagesBuffers || driverManagesBuffers || driverManagedBuffersV2;
#else
        osManagesBuffers || driverManagedBuffersV2;
#endif
    if (!allowedConfiguration)
    {
        LogError(FLAG_ADAPTER,
            "Incorrect buffer management configuration: AllocationMode: %d, AttachmentMode: %d",
            m_rxCapabilities.AllocationMode, m_rxCapabilities.AttachmentMode);
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

    // Specify the RSS Capabilities
    adapterGeneral.RecvScaleCapabilities = RxScaling.RecvScaleCapabilities;

    const auto ETHERNET_MTU_SIZE = 1500;

    if (IsDevicelessAdapter())
    {
        adapterGeneral.PowerManagementCapabilitiesEx = NULL;

        adapterGeneral.SupportedOidList = NULL;
        adapterGeneral.SupportedOidListLength = 0U;

        //
        // Assume medium type to be 802.3 and use ethernet MTU size as LookaheadSize
        //
        adapterGeneral.MediaType = NdisMedium802_3;
        adapterGeneral.LookaheadSize = ETHERNET_MTU_SIZE;
        adapterGeneral.PhysicalMediumType = NdisPhysicalMediumUnspecified;

        //
        // COPY_LOOKAHEAD_DATA: protocol driver is free to access indicated data by any means
        // TRANSFERS_NOT_PEND: we indicate receive operations with the NdisMIndicateReceiveNetBufferLists
        // 8021P_PRIORITY / 8021Q_VLAN: supported by NetAdapterCx
        //
        adapterGeneral.MacOptions = (
            NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
            NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
            NDIS_MAC_OPTION_8021P_PRIORITY |
            NDIS_MAC_OPTION_8021Q_VLAN);

        adapterGeneral.IfType = IF_TYPE_ETHERNET_CSMACD;
        adapterGeneral.IfConnectorPresent = FALSE;

        adapterGeneral.AccessType = NET_IF_ACCESS_LOOPBACK;
        adapterGeneral.ConnectionType = NET_IF_CONNECTION_DEDICATED;
        adapterGeneral.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
    }
    else
    {
        //
        // Set the power management capabilities.
        //
        m_powerPolicy->InitializeNdisCapabilities(
            GetCombinedMediaSpecificWakeUpEvents(),
            GetCombinedSupportedProtocolOffloads(),
            &pmCapabilities);

        adapterGeneral.PowerManagementCapabilitiesEx = &pmCapabilities;

        NxDevice * nxDevice = GetNxDeviceFromHandle(m_device);
        adapterGeneral.SupportedOidList = const_cast<NDIS_OID *>(nxDevice->GetOidList());
        adapterGeneral.SupportedOidListLength = static_cast<ULONG>(nxDevice->GetOidListCount() * sizeof(NDIS_OID));
        //
        // The Remaining General Attributes Fields are not used for
        // a WDF NDIS model. Some of them come from Inf, some of them are
        // deprecated. Just set them to invalid values or default
        // values for diagnaosability purposes.
        //
        adapterGeneral.MediaType = static_cast<NDIS_MEDIUM>(-1);
        adapterGeneral.PhysicalMediumType = NdisPhysicalMediumUnspecified;
        adapterGeneral.LookaheadSize = 0;
        adapterGeneral.MacOptions = 0;

        adapterGeneral.IfType = static_cast<NET_IFTYPE>(-1);
        adapterGeneral.IfConnectorPresent = FALSE;
    }

    // Setting up device-specific NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES.
    ndisStatus = m_ndisMiniport->SetGeneralAttributes(adapterGeneral);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(FLAG_ADAPTER,
                 "NdisMSetMiniportAttributes failed with general attributes, %!NDIS_STATUS!", ndisStatus);
        goto Exit;
    }






    m_flags.GeneralAttributesSet = TRUE;

Exit:

    return status;
}

#ifdef NETCX_KMDF
void
NxAdapter::QueuePowerReferenceWorkItem(
    void
)
{
    m_powerPolicy->QueuePowerReferenceWorkItem();
}
#endif

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
    m_powerPolicy->InitializeNdisCapabilities(
        GetCombinedMediaSpecificWakeUpEvents(),
        GetCombinedSupportedProtocolOffloads(),
        &pmCapabilities);

    m_ndisMiniport->IndicateMiniportStatusChange(
        NDIS_STATUS_PM_HARDWARE_CAPABILITIES,
        pmCapabilities,
        nullptr);
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

    m_ndisMiniport->IndicateMiniportStatusChange(
        NDIS_STATUS_LINK_STATE,
        linkState,
        nullptr);
}

void
NxAdapter::IndicateMtuSizeChangeToNdis(
    void
)
{
    m_ndisMiniport->IndicateMiniportStatusChange(
        NDIS_STATUS_L2_MTU_SIZE_CHANGE,
        m_mtuSize,
        nullptr);
}

void
NxAdapter::IndicateCurrentLinkLayerAddressToNdis(
    void
)
{
    m_ndisMiniport->IndicateMiniportStatusChange(
        NDIS_STATUS_CURRENT_MAC_ADDRESS_CHANGE,
        m_currentLinkLayerAddress,
        nullptr);
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

#ifdef NETCX_KMDF
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
#endif

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
    return m_ndisPnpPower->GetMiniportReference();
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

    if (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriverV2 &&
        m_rxCapabilities.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver)
    {
        NET_EXTENSION_PRIVATE const netMemoryReturnContextExtensionIn = {
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_NAME,
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_RETURN_CONTEXT_VERSION_1,
            alignof(NET_FRAGMENT_RETURN_CONTEXT),
            NetExtensionTypeFragment,
        };
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(netMemoryReturnContextExtensionIn));

        NET_EXTENSION_PRIVATE const netMemoryIdExtension = {
            NET_FRAGMENT_EXTENSION_NET_MEMORY_NAME,
            NET_FRAGMENT_EXTENSION_NET_MEMORY_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_NET_MEMORY_VERSION_1,
            alignof(NET_FRAGMENT_NET_MEMORY),
            NetExtensionTypeFragment,
        };
        CX_RETURN_IF_NOT_NT_SUCCESS(
            RegisterExtension(netMemoryIdExtension));
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

    if (!IsDevicelessAdapter())
    {
        WdfDeviceSetFailed(m_device, WdfDeviceFailedAttemptRestart);
    }
}

_Use_decl_annotations_
void
NxAdapter::GetProperties(
    NET_CLIENT_ADAPTER_PROPERTIES * Properties
) const
{
    Properties->MediaType = m_mediaType;
    Properties->NetLuid = m_netLuid;
#if NETCX_KMDF
    Properties->DriverIsVerifying = !!MmIsDriverVerifyingByAddress(m_datapathCallbacks.EvtAdapterCreateTxQueue);
#else
    Properties->DriverIsVerifying = GetPrivateGlobals()->CxVerifierOn;
#endif
    Properties->NdisAdapterHandle = GetNdisHandle();
    Properties->Name = &m_instanceName;
    Properties->ExecutionContextKnobs = m_executionContextKnobs;
    Properties->Process = nullptr;

    // C++ object shared across ABI
    // this is not viable once the translator is removed from the Cx
#ifdef NETCX_KMDF
    Properties->NblDispatcher = const_cast<INxNblDispatcher *>(static_cast<const INxNblDispatcher *>(&m_nblDatapath));
    Properties->PacketMonitorLowerEdge = reinterpret_cast<PKTMON_LOWEREDGE_HANDLE>(const_cast<PKTMON_EDGE_CONTEXT *>(&m_packetMonitor.GetLowerEdgeContext()));
    Properties->PacketMonitorComponentContext = reinterpret_cast<PKTMON_COMPONENT_HANDLE>(const_cast<PKTMON_COMPONENT_CONTEXT *>(&m_packetMonitor.GetComponentContext()));
#endif
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
    else if ((rxCap.AllocationMode == NetRxFragmentBufferAllocationModeDriverV2) &&
             (rxCap.AttachmentMode == NetRxFragmentBufferAttachmentModeDriver))
    {
        // Driver V2 mode relies on functions that were introduced in the Spring25 bug bundle fix
        DatapathCapabilities->RxMemoryManagementMode = NET_CLIENT_MEMORY_MANAGEMENT_MODE_DRIVER_V2;
#ifdef NETCX_UMDF
        NxMemory* memory = GetNxMemoryFromHandle(rxCap.MemoryCollection);
        DatapathCapabilities->RxMemoryCollection = memory->GetRdMemoryHandle();
#endif
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

#ifdef NETCX_KMDF
    DEVICE_OBJECT* physicalDeviceObject = nullptr;
    if (!IsDevicelessAdapter())
    {
        physicalDeviceObject = WdfDeviceWdmGetPhysicalDevice(GetDevice());
    }

    ULONG preferredNumaNode = MM_ANY_NODE_OK;
    if (physicalDeviceObject != nullptr)
    {
        USHORT deviceNumaNode;
        (void)IoGetDeviceNumaNode(physicalDeviceObject, &deviceNumaNode);
        preferredNumaNode = deviceNumaNode;
    }
#else
    ULONG preferredNumaNode = MM_ANY_NODE_OK;
#endif

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
#ifdef NETCX_KMDF
        DatapathCapabilities->RxMemoryConstraints.Dma.DmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(rxCap.DmaCapabilities->DmaEnabler,
                                            WdfDmaDirectionReadFromDevice);
#endif
    }

    if (DatapathCapabilities->TxMemoryConstraints.MappingRequirement == NET_CLIENT_MEMORY_MAPPING_REQUIREMENT_DMA_MAPPED)
    {
        DatapathCapabilities->TxMemoryConstraints.Dma.CacheEnabled =
            static_cast<NET_CLIENT_TRI_STATE> (txCap.DmaCapabilities->CacheEnabled);
        DatapathCapabilities->TxMemoryConstraints.Dma.PreferredNode = txCap.DmaCapabilities->PreferredNode;

#ifdef NETCX_KMDF
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
#else
        auto const maximumDriverLogicalAddress = txCap.DmaCapabilities->MaximumPhysicalAddress.QuadPart > 0
            ? static_cast<UINT64>(txCap.DmaCapabilities->MaximumPhysicalAddress.QuadPart)
            : 0xffffffffffffffff;
        DatapathCapabilities->TxMemoryConstraints.Dma.MaximumPhysicalAddress.QuadPart = maximumDriverLogicalAddress;
#endif

        if (DatapathCapabilities->TxMemoryConstraints.Dma.PreferredNode == MM_ANY_NODE_OK)
        {
            DatapathCapabilities->TxMemoryConstraints.Dma.PreferredNode = preferredNumaNode;
        }
#if NETCX_KMDF
        // We also need the PDO to let buffer manager call DMA APIs
        DatapathCapabilities->TxMemoryConstraints.Dma.PhysicalDeviceObject = physicalDeviceObject;
#endif
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
    NET_EXECUTION_CONTEXT * ExecutionContextHandle,
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

    *ExecutionContextHandle = context.ExecutionContext->GetHandle();
    *ExecutionContextDispatch = context.ExecutionContext->GetExecutionContextDispatch();

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
    NET_EXECUTION_CONTEXT * ExecutionContextHandle,
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

    context.CreateReturnContextRing = (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriverV2);

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

    *ExecutionContextHandle = context.ExecutionContext->GetHandle();
    *ExecutionContextDispatch = context.ExecutionContext->GetExecutionContextDispatch();

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

NET_MEMORY_MDL_WRAPPER *
NxAdapter::GetMdlForDriverAllocatedMemory(
    ULONG64 MemoryId
)
{
#ifdef NETCX_KMDF
    NxMemory* memory = GetNxMemoryFromHandle(m_rxMemoryCollection);
    return memory->GetMdlForDriverAllocatedMemory(reinterpret_cast<NET_MEMORY_ID>(MemoryId));
#else
    UNREFERENCED_PARAMETER(MemoryId);
    return nullptr;
#endif
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

    if (!nxAdapter->IsDevicelessAdapter())
    {
        //
        // If the adapter has been disposed the workitem for async power reference
        // would have been disposed as well. Enqueueing it will be a no-op.
        // In case there is a thread in NDIS waiting for the power complete notification
        // unblock it.
        //
#ifdef NETCX_KMDF
        if (nxAdapter->m_powerPolicy && nxAdapter->m_powerPolicy->IsPowerRequiredWorkItemPending() &&
            nxAdapter->m_ndisPnpPower)
        {
            nxAdapter->m_ndisPnpPower->NotifyAsyncPowerReferenceCompletion(STATUS_INVALID_DEVICE_STATE);
        }
#endif

        NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

        nxDevice->PnpPower.AdapterDeleted(nxAdapter);
    }

    if (nxAdapter->m_deviceInterfaceNotification)
    {
        nxAdapter->m_deviceInterfaceNotification.reset();

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

#ifdef NETCX_KMDF
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
#else
    m_bridgeToKernel.OffloadInitialize();
#endif

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
#ifdef NETCX_KMDF
    for (auto &app : m_apps)
    {
        m_clientDispatch->CreateDatapath(app.get());
    }
#else
    m_bridgeToKernel.CreateDatapath();
#endif

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathDestroy(
    void
)
{
#ifdef NETCX_KMDF
    for (auto &app : m_apps)
    {
        m_clientDispatch->DestroyDatapath(app.get());
    }
#else
    m_bridgeToKernel.DestroyDatapath();
    // Kernel does not currently send individual destroy events for each queue.
    // Clear queues here until the translator owns queue lifetime for Tx scaling/RSS.
    m_rxQueues.clear();
    m_txQueues.clear();
#endif

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStart(
    void
)
{
#ifdef NETCX_KMDF
    for (auto &app : m_apps)
    {
        m_clientDispatch->StartDatapath(app.get());
    }
#else
    m_bridgeToKernel.StartDatapath();
#endif

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathStop(
    void
)
{
#ifdef NETCX_KMDF
    for (auto &app : m_apps)
    {
        m_clientDispatch->StopDatapath(app.get());
    }
#else
    m_bridgeToKernel.StopDatapath();
#endif

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
    m_ndisMiniport->CompleteMiniportPause();

    return NxAdapter::Event::SyncSuccess;
}

_Use_decl_annotations_
NxAdapter::Event
NxAdapter::DatapathRestartComplete(
    void
)
{
    m_ndisMiniport->CompleteMiniportRestart();

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
    This routine ensures that NDIS will keep one (and only one) WDFDEVICE power reference per adapter
    when the NETADAPTER is busy.
    This is done to account for:
    - Cases where both AOAC and SS are enabled
    - One of them is enabled.
    - Neither of them are enabled.
--*/
{
    if (!m_powerPolicy->IsPowerCapable())
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
    const auto powerReferenceStatus = m_powerPolicy->PowerReference(true);

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
    if (m_powerPolicy->ShouldAcquirePowerReferencesForAdapter() && NT_SUCCESS(powerReferenceStatus))
    {
        // When SS and AOAC structures are initialized in NDIS, they are both initialized
        // with stop flags so that SS / AoAC engines dont try to drop / acquire power refs
        // until the WDFDEVICE is ready. Now that we're ready this routine will clear
        // those SS / AoAC stop flags by calling into NdisWdfActionStartPowerManagement.

        const auto startPMStatus = NotifyNdisPowerOperation(
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
    if (IsDevicelessAdapter())
    {
        TraceLoggingWrite(
            g_hNetAdapterCxEtwProvider,
            "NxAdapterStateTransition",
            TraceLoggingDescription("NxAdapterStateTransition"),
            TraceLoggingKeyword(NET_ADAPTER_CX_NXADAPTER_STATE_TRANSITION),
            TraceLoggingGuid(m_interfaceGuid, "DevicelessAdapterInterfaceGuid"),
            TraceLoggingHexUInt64(m_netLuid.Value, "AdapterNetLuid"),
            TraceLoggingHexInt32(static_cast<INT32>(SourceState), "AdapterStateTransitionFrom"),
            TraceLoggingHexInt32(static_cast<INT32>(ProcessedEvent), "AdapterStateTransitionEvent"),
            TraceLoggingHexInt32(static_cast<INT32>(TargetState), "AdapterStateTransitionTo"));
    }
    else
    {
        NxDevice* nxDevice = GetNxDeviceFromHandle(m_device);

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
    }

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
            Version <= extension.m_extension.Version &&
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
NxAdapter::TxPeerDemuxTableRemovePeer(
    NET_EUI48_ADDRESS const* Address
)
{
#ifdef NETCX_KMDF
    m_apps[0]->TxPeerDemuxTableRemovePeer(
        reinterpret_cast<NET_CLIENT_EUI48_ADDRESS const*>(Address)
    );
#else
    m_bridgeToKernel.TxPeerDemuxTableRemovePeer(reinterpret_cast<NET_CLIENT_EUI48_ADDRESS const*>(Address));
#endif
}

_Use_decl_annotations_
void
NxAdapter::TxPeerDemuxTableAddPeer(
    NET_EUI48_ADDRESS const* Address
)
{
#ifdef NETCX_KMDF
    m_apps[0]->TxPeerDemuxTableAddPeer(
        reinterpret_cast<NET_CLIENT_EUI48_ADDRESS const*>(Address)
    );
#else
    m_bridgeToKernel.TxPeerDemuxTableAddPeer(reinterpret_cast<NET_CLIENT_EUI48_ADDRESS const*>(Address));
#endif
}

void
NxAdapter::SetDatapathCapabilities(
    NET_ADAPTER_TX_CAPABILITIES const* TxCapabilities,
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
    else if (m_rxCapabilities.AllocationMode == NetRxFragmentBufferAllocationModeDriverV2)
    {
        m_rxMemoryCollection = m_rxCapabilities.MemoryCollection;
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

#ifdef NETCX_UMDF
void
NxAdapter::SetDatapathCapabilities_UMDF_2_5_and_older(
    _In_ NET_ADAPTER_TX_CAPABILITIES const * TxCapabilities,
    _In_ NET_ADAPTER_RX_CAPABILITIES const * RxCapabilities
)
{
    // UMDF drivers 2.5 and 2.4 do not have the DmaCapabilities field in the NET_ADAPTER_*_CAPABILITIES
    // structures and do not support AllocationMode == NetRxFragmentBufferAllocationModeDriver.
    //
    // The DmaCapabilities field is the last field in the NET_ADAPTER_*_CAPABILITIES structures, so
    // it is safe to copy the entire structure.
    RtlCopyMemory(
        &m_txCapabilities,
        TxCapabilities,
        TxCapabilities->Size);

    RtlCopyMemory(
        &m_rxCapabilities,
        RxCapabilities,
        RxCapabilities->Size);

    m_txCapabilities.Size = sizeof(m_txCapabilities);
    m_rxCapabilities.Size = sizeof(m_rxCapabilities);
}
#endif

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
    m_powerPolicy->SetPowerOffloadArpCapabilities(Capabilities);

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
    m_powerPolicy->SetPowerOffloadNSCapabilities(Capabilities);

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
    m_powerPolicy->SetWakeBitmapCapabilities(Capabilities);

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
    m_powerPolicy->SetMagicPacketCapabilities(Capabilities);

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
    m_powerPolicy->SetEapolPacketCapabilities(Capabilities);

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
    m_powerPolicy->SetWakeMediaChangeCapabilities(Capabilities);

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
    m_powerPolicy->SetWakePacketFilterCapabilities(Capabilities);

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
    g_NetAdapterCxTriageBlock.NxAdapterNxNdisMiniportOffset = FIELD_OFFSET(NxAdapter, m_ndisMiniport);
    NxNdisMiniport::GetTriageInfo();
}

_Use_decl_annotations_
void NxAdapter::ReportWakeReasonPacket(
    const NET_ADAPTER_WAKE_REASON_PACKET * Reason
)
{
    Verifier_VerifyNetPowerUpTransition(GetPrivateGlobals(), GetNxDeviceFromHandle(m_device));

    switch (Reason->PatternId)
    {
        case NetAdapterWakeFilterPatternId:
            SetWakeReason(NetWakeReasonTypePacketFilterMatch);
            break;

        case NetAdapterWakeMagicPatternId:
            SetWakeReason(NetWakeReasonTypeMagicPacket);
            break;

        case NetAdapterWakeEapolPatternId:
            SetWakeReason(NetWakeReasonTypeEapolPacket);
            break;

        default:
            SetWakeReason(NetWakeReasonTypeBitmapPattern);
            break;
    }

    NDIS_PM_WAKE_REASON * wakeReason;
    ULONG wakeReasonSize;

    auto const ntStatus = m_powerPolicy->MakeWakeReasonPacket(Reason, &wakeReason, &wakeReasonSize);

    if (NT_SUCCESS(ntStatus))
    {
        m_ndisMiniport->IndicateMiniportStatusChange(
            NDIS_STATUS_PM_WAKE_REASON,
            *wakeReason,
            reinterpret_cast<ULONG const *>(&wakeReasonSize));
    }
}

_Use_decl_annotations_
void NxAdapter::ReportWakeReasonMediaChange(
    NET_IF_MEDIA_CONNECT_STATE Reason
)
{
    Verifier_VerifyNetPowerUpTransition(GetPrivateGlobals(), GetNxDeviceFromHandle(m_device));

    SetWakeReason(NetWakeReasonTypeMediaChange);

    NDIS_PM_WAKE_REASON wakeReason;

    auto const ntStatus = m_powerPolicy->MakeWakeReasonMediaChange(Reason, &wakeReason);

    if (!NT_SUCCESS(ntStatus))
    {
        Verifier_ReportViolation(
            GetPrivateGlobals(),
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetAdapterWakeReasonMediaChange,
            0,
            0);
    }

    m_ndisMiniport->IndicateMiniportStatusChange(
        NDIS_STATUS_PM_WAKE_REASON,
        wakeReason,
        nullptr);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::PowerReference(
    bool WaitForD0
)
{
    return m_powerPolicy->PowerReference(WaitForD0);
}

void
NxAdapter::PowerDereference(
    void
)
{
    m_powerPolicy->PowerDereference();
}

void
NxAdapter::ReleaseOutstandingPowerReferences(
    void
)
{
    if (m_powerPolicy)
    {
        m_powerPolicy->ReleaseOutstandingPowerReferences();
    }
}

void
NxAdapter::WifiDestroyPeerAddressDatapath(
    size_t Demux
)
{
#ifdef NETCX_KMDF
    for (auto & app : m_apps)
    {
        m_clientDispatch->WifiDestroyPeerAddressDatapath(app.get(), Demux);
    }
#else
    m_bridgeToKernel.WifiDestroyPeerAddressDatapath(Demux);
#endif
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
#ifdef NETCX_KMDF
    for (auto & app : m_apps)
    {
        m_clientDispatch->PauseOffloadCapabilities(app.get());
    }
#else
    m_bridgeToKernel.PauseOffloadCapabilities();
#endif

    m_isOffloadPaused = true;
}

void
NxAdapter::ResumeOffloads(
    void
)
{
#ifdef NETCX_KMDF
    for (auto & app : m_apps)
    {
        m_clientDispatch->ResumeOffloadCapabilities(app.get());
    }
#else
    m_bridgeToKernel.ResumeOffloadCapabilities();
#endif

    m_isOffloadPaused = false;
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
        m_ndisMiniport->CompleteOidRequest(NdisOidRequest, NDIS_STATUS_NOT_SUPPORTED);
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
    NDIS_STATUS ndisStatus = STATUS_NDIS_NOT_SUPPORTED;

    bool processedByFramework = FrameworkSetOidHandler(Set, ndisStatus);
    if (processedByFramework)
    {
        m_ndisMiniport->CompleteOidRequest(NdisOidRequest, ndisStatus);
    }
    else
    {
        DispatchOidRequest(NdisOidRequest, Context);
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

_Use_decl_annotations_
void
NxAdapter::CompleteOidRequest(
    NDIS_OID_REQUEST* NdisOidRequest,
    NDIS_STATUS Status
)
{
    m_ndisMiniport->CompleteOidRequest(NdisOidRequest, Status);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::SetNative80211Attributes(
    NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES * Attributes
)
{
    m_ndisMiniport->SetNative80211Attributes(Attributes);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxAdapter::IndicateMiniportStatus(
    NDIS_STATUS_INDICATION * NdisStatusIndication
)
{
    m_ndisMiniport->IndicateMiniportStatus(NdisStatusIndication);
}

_Use_decl_annotations_
bool
NxAdapter::FrameworkSetOidHandler(
    NDIS_OID_REQUEST::_REQUEST_DATA::_SET const* SetInformation,
    NDIS_STATUS& Status
)
{
    //
    // All internal OID handling is synchronous, we must either complete the request
    // (NdisMOidRequestComplete) or let the framework keep processing it (DispatchOidRequest)
    //

    switch (SetInformation->Oid)
    {
        case OID_PM_ADD_WOL_PATTERN:
            Status = m_powerPolicy->AddWakePattern(SetInformation);
            return true;

        case OID_PM_REMOVE_WOL_PATTERN:
            Status = m_powerPolicy->RemoveWakePattern(SetInformation);
            return true;

        case OID_PM_ADD_PROTOCOL_OFFLOAD:
            Status = m_powerPolicy->AddProtocolOffload(SetInformation);
            return true;

        case OID_PM_REMOVE_PROTOCOL_OFFLOAD:
            Status = m_powerPolicy->RemoveProtocolOffload(SetInformation);
            return true;
    }

    Status = STATUS_NDIS_NOT_SUPPORTED;
    return false;
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

    m_powerPolicy->SetParameters(PmParameters);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::NotifyNdisPowerOperation(
    NdisWdfPnpPowerAction PnpPowerActionOccured,
    NdisWdfPnpPowerAction LastPowerDownAction
)
{
    return m_ndisPnpPower->NotifyNdisPowerOperation(PnpPowerActionOccured, LastPowerDownAction);
}

_Use_decl_annotations_
NTSTATUS
NxAdapter::HandleQueryAllDataIrp(
    NDIS_GUID * NdisGuid,
    GUID * Guid,
    void * Wnode,
    ULONG BufferSize,
    ULONG * ReturnSize
)
{
    return m_ndisPnpPower->HandleQueryAllDataIrp(NdisGuid, Guid, Wnode, BufferSize, ReturnSize);
}

_Use_decl_annotations_
NET_WAKE_REASON_TYPE
NxAdapter::GetWakeReason(
    void
) const
{
    KAcquireSpinLock lock(m_wakeReasonLock);
    return m_wakeReason;
}

_Use_decl_annotations_
void
NxAdapter::SetWakeReason(
    NET_WAKE_REASON_TYPE Reason
)
{
    KAcquireSpinLock lock(m_wakeReasonLock);
    // Don't overwrite the wake reason if it is already set - this will ensure the
    // first indicated wake reason is cached
    if (m_wakeReason == NetWakeReasonTypeNone)
    {
        m_wakeReason = Reason;
    }

    // We know the device initiated the wake - update if we get a specific reason from the device
    if (m_wakeReason == NetWakeReasonTypeDevice &&
        Reason != NetWakeReasonTypeNdis &&
        Reason != NetWakeReasonTypeNone)
    {
        m_wakeReason = Reason;
    }
}

_Use_decl_annotations_
void
NxAdapter::ResetWakeReason(
    void
)
{
    KAcquireSpinLock lock(m_wakeReasonLock);
    m_wakeReason = NetWakeReasonTypeNone;
}

_Use_decl_annotations_
void
NxAdapter::LogWakeReason(
    void
) const
{
    LogInfo(FLAG_ADAPTER, "Wake reason is %!NET_WAKE_REASON_TYPE! NETADAPTER: %p", m_wakeReason, GetFxObject());
}
