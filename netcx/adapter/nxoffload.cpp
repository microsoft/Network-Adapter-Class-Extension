#include "Nx.hpp"
#include "NxAdapter.hpp"
#include "NxConfiguration.hpp"
#include "idle/IdleStateMachine.hpp"
#include "WdfObjectCallback.hpp"
#include "NxOffload.hpp"

#include <net/checksumtypes_p.h>
#include <net/gsotypes_p.h>
#include <net/rsctypes_p.h>

#include "NxOffload.tmh"

typedef enum _NDIS_CHECKSUM_KEYWORD_FLAGS {
    NdisChecksumTxEnabled = 0x00000001,
    NdisChecksumRxEnabled = 0x00000002
} NDIS_CHECKSUM_KEYWORD_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS(NDIS_CHECKSUM_KEYWORD_FLAGS);

_Use_decl_annotations_
NxOffloadBase::NxOffloadBase(
    OffloadType OffloadType
) : m_offloadType(OffloadType)
{
}

_Use_decl_annotations_
OffloadType
NxOffloadBase::GetOffloadType(
    void
) const
{
    return this->m_offloadType;
}

_Use_decl_annotations_
template <typename T>
NxOffload<T>::NxOffload(
    OffloadType OffloadType,
    T const &DefaultHardwareCapabilities
) : NxOffloadBase(OffloadType)
{
    RtlCopyMemory(&m_hardwareCapabilities, &DefaultHardwareCapabilities, sizeof(DefaultHardwareCapabilities));
}

_Use_decl_annotations_
template <typename T>
void
NxOffload<T>::SetHardwareCapabilities(
    T const * HardwareCapabilities,
    NxOffloadCallBack<T> OffloadCallback
)
{
    RtlCopyMemory(&m_hardwareCapabilities, HardwareCapabilities, sizeof(*HardwareCapabilities));
    m_offloadCallback = OffloadCallback;

    //
    // Set a flag to indicate that the offload is supported by hardware
    // Packet extensions need to be registered later for offloads whose
    // capabilities are explicitly set by the client driver
    //
    m_hardwareSupport = true;
}

_Use_decl_annotations_
template <typename T>
void
NxOffload<T>::SetOffloadCallback(
    NxOffloadCallBack<T> OffloadCallback
)
{
    m_offloadCallback = OffloadCallback;

    //
    // Set a flag to indicate that the offload is supported by hardware
    // Packet extensions need to be registered later for offloads whose
    // capabilities are explicitly set by the client driver
    //
    m_hardwareSupport = true;
}

_Use_decl_annotations_
template <typename T>
NxOffloadCallBack<T>
NxOffload<T>::GetOffloadCallback(
    void
) const
{
    return m_offloadCallback;
}

_Use_decl_annotations_
template <typename T>
T const *
NxOffload<T>::GetHardwareCapabilities(
    void
) const
{
    return &m_hardwareCapabilities;
}

_Use_decl_annotations_
template <typename T>
bool
NxOffload<T>::HasHardwareSupport(
    void
) const
{
    return m_hardwareSupport;
}

_Use_decl_annotations_
NxOffloadFacade::NxOffloadFacade(
    NxAdapter& nxAdapter
) : m_adapter(nxAdapter)
{
}

_Use_decl_annotations_
NTSTATUS
NxOffloadFacade::QueryStandardizedKeywordSetting(
    PCUNICODE_STRING Keyword,
    PULONG Value
)
{
    NxConfiguration *nxConfiguration;
    NTSTATUS status = NxConfiguration::_Create<WDFDEVICE>(
        m_adapter.GetPrivateGlobals(),
        m_adapter.GetDevice(),
        NULL,
        &nxConfiguration);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = nxConfiguration->Open();

    if (!NT_SUCCESS(status))
    {
        nxConfiguration->Close();
        return status;
    }

    status = nxConfiguration->QueryUlong(NET_CONFIGURATION_QUERY_ULONG_NO_FLAGS, Keyword, Value);
    nxConfiguration->Close();

    return status;
}

_Use_decl_annotations_
NTSTATUS
NxOffloadFacade::RegisterExtension(
    OffloadType type,
    NxOffloadBase const * offload
)
{
    NET_EXTENSION_PRIVATE extensionPrivate = {};

    switch (type)
    {
    case OffloadType::Checksum:
    case OffloadType::TxChecksum:
    case OffloadType::RxChecksum:
        extensionPrivate.Name = NET_PACKET_EXTENSION_CHECKSUM_NAME;
        extensionPrivate.Size = NET_PACKET_EXTENSION_CHECKSUM_VERSION_1_SIZE;
        extensionPrivate.Version = NET_PACKET_EXTENSION_CHECKSUM_VERSION_1;
        extensionPrivate.NonWdfStyleAlignment = alignof(NET_PACKET_CHECKSUM);
        extensionPrivate.Type = NetExtensionTypePacket;
        break;

    case OffloadType::Gso:
        extensionPrivate.Name = NET_PACKET_EXTENSION_GSO_NAME;
        extensionPrivate.Size = NET_PACKET_EXTENSION_GSO_VERSION_1_SIZE;
        extensionPrivate.Version = NET_PACKET_EXTENSION_GSO_VERSION_1;
        extensionPrivate.NonWdfStyleAlignment = alignof(NET_PACKET_GSO);
        extensionPrivate.Type = NetExtensionTypePacket;
        break;

    case OffloadType::Rsc:
        extensionPrivate.Name = NET_PACKET_EXTENSION_RSC_NAME;
        extensionPrivate.Size = NET_PACKET_EXTENSION_RSC_VERSION_1_SIZE;
        extensionPrivate.Version = NET_PACKET_EXTENSION_RSC_VERSION_1;
        extensionPrivate.NonWdfStyleAlignment = alignof(NET_PACKET_RSC);
        extensionPrivate.Type = NetExtensionTypePacket;

        {
            auto const nxOffload = static_cast<NxOffload<NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES> const *>(offload);
            auto const capabilities = nxOffload->GetHardwareCapabilities();

            if (capabilities->TcpTimestampOption)
            {
                CX_RETURN_IF_NOT_NT_SUCCESS(m_adapter.RegisterExtension(extensionPrivate));
                extensionPrivate.Name = NET_PACKET_EXTENSION_RSC_TIMESTAMP_NAME;
                extensionPrivate.Size = NET_PACKET_EXTENSION_RSC_TIMESTAMP_VERSION_1_SIZE;
                extensionPrivate.Version = NET_PACKET_EXTENSION_RSC_TIMESTAMP_VERSION_1;
                extensionPrivate.NonWdfStyleAlignment = alignof(NET_PACKET_RSC_TIMESTAMP);
                extensionPrivate.Type = NetExtensionTypePacket;
            }
        }
        break;
    }

    return m_adapter.RegisterExtension(extensionPrivate);
}

_Use_decl_annotations_
NETADAPTER
NxOffloadFacade::GetNetAdapter(
    void
) const
{
    return m_adapter.GetFxObject();
}

_Use_decl_annotations_
NxOffloadManager::NxOffloadManager(
    INxOffloadFacade *NxOffloadFacade
)
{
    m_offloadFacade = wistd::unique_ptr<INxOffloadFacade>{NxOffloadFacade};
}

_Use_decl_annotations_
NTSTATUS
NxOffloadManager::Initialize(
    void
)
{
    const NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES checksumCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES),
        FALSE,
        FALSE,
        FALSE
    };

    auto checksumOffload = wil::make_unique_nothrow<NxOffload<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES>>(OffloadType::Checksum, checksumCapabilities);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !checksumOffload);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_offloads.append(wistd::move(checksumOffload)));

    const NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES rxChecksumCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES),
        FALSE,
        FALSE,
        FALSE
    };

    auto rxChecksumOffload = wil::make_unique_nothrow<NxOffload<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES>>(OffloadType::RxChecksum, rxChecksumCapabilities);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !rxChecksumOffload);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_offloads.append(wistd::move(rxChecksumOffload)));

    const NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES txChecksumCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES),
    };

    auto txChecksumOffload = wil::make_unique_nothrow<NxOffload<NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES>>(OffloadType::TxChecksum, txChecksumCapabilities);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !txChecksumOffload);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_offloads.append(wistd::move(txChecksumOffload)));

    const NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES gsoCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES),
    };

    auto gsoOffload = wil::make_unique_nothrow<NxOffload<NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES>>(OffloadType::Gso, gsoCapabilities);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !gsoOffload);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_offloads.append(wistd::move(gsoOffload)));

    const NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES rscCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES),
    };

    auto rscOffload = wil::make_unique_nothrow<NxOffload<NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES>>(OffloadType::Rsc, rscCapabilities);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !rscOffload);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_offloads.append(wistd::move(rscOffload)));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
template <typename T>
void
NxOffloadManager::SetHardwareCapabilities(
    T const * HardwareCapabilities,
    OffloadType OffloadType,
    NxOffloadCallBack<T> OffloadCallback
)
{
    auto nxOffload = static_cast<NxOffload<T> *>(FindNxOffload(OffloadType));

    nxOffload->SetHardwareCapabilities(HardwareCapabilities, OffloadCallback);
}

_Use_decl_annotations_
template <typename T>
T const *
NxOffloadManager::GetHardwareCapabilities(
    OffloadType OffloadType
) const
{
    auto nxOffload = static_cast<NxOffload<T> *>(FindNxOffload(OffloadType));

    return nxOffload->GetHardwareCapabilities();
}

_Use_decl_annotations_
template <typename T>
void
NxOffloadManager::SetActiveCapabilities(
    T const & ActiveCapabilities,
    OffloadType OffloadType
)
{
    auto nxOffload = FindNxOffload(OffloadType);

    if (OffloadType == OffloadType::TxChecksum)
    {
        auto offloadCallbackGso = static_cast<NxOffload<NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES> *>(nxOffload)->GetOffloadCallback();

        if (offloadCallbackGso != nullptr)
        {
            InvokePoweredCallback(
                offloadCallbackGso,
                m_offloadFacade->GetNetAdapter(),
                reinterpret_cast<NETOFFLOAD>(const_cast<T *>(&ActiveCapabilities)));
        }
    }
    else if (OffloadType == OffloadType::Gso)
    {
        auto offloadCallbackGso = static_cast<NxOffload<NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES> *>(nxOffload)->GetOffloadCallback();

        if (offloadCallbackGso != nullptr)
        {
            InvokePoweredCallback(
                offloadCallbackGso,
                m_offloadFacade->GetNetAdapter(),
                reinterpret_cast<NETOFFLOAD>(const_cast<T *>(&ActiveCapabilities)));
        }
    }
    else
    {
        auto offloadCallback = static_cast<NxOffload<T> *>(nxOffload)->GetOffloadCallback();

        if (offloadCallback != nullptr)
        {
            InvokePoweredCallback(
                offloadCallback,
                m_offloadFacade->GetNetAdapter(),
                reinterpret_cast<NETOFFLOAD>(const_cast<T *>(&ActiveCapabilities)));
        }
    }
}

_Use_decl_annotations_
template <typename T>
void
NxOffloadManager::SetOffloadCallback(
    OffloadType OffloadType,
    NxOffloadCallBack<T> OffloadCallback
)
{
    auto nxOffload = static_cast<NxOffload<T> *>(FindNxOffload(OffloadType));

    nxOffload->SetOffloadCallback(OffloadCallback);
}

_Use_decl_annotations_
NxOffloadBase *
NxOffloadManager::FindNxOffload(
    OffloadType OffloadType
) const
{
    for (auto const & offload : this->m_offloads)
    {
        if (offload->GetOffloadType() == OffloadType)
        {
            return offload.get();
        }
    }

    return nullptr;
}

_Use_decl_annotations_
bool
NxOffloadManager::IsKeywordSettingDisabled(
    NDIS_STRING Keyword
) const
{
    ULONG value = 0;
    NTSTATUS status = m_offloadFacade->QueryStandardizedKeywordSetting((PCUNICODE_STRING)&Keyword, &value);

    //
    // If the registry key cannot be queried (i.e. status = failure), treat the keyword setting as enabled
    //
    if (NT_SUCCESS(status) && value == 0)
    {
        return true;
    }

    return false;
}

_Use_decl_annotations_
bool
NxOffloadManager::IsKeywordPresent(
    UNICODE_STRING Keyword
) const
{
    ULONG value = 0;
    NTSTATUS status = m_offloadFacade->QueryStandardizedKeywordSetting(&Keyword, &value);

    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
    {
        return false;
    }

    return true;
}

_Use_decl_annotations_
ULONG
NxOffloadManager::GetStandardizedKeywordSetting(
    UNICODE_STRING Keyword,
    ULONG DefaultValue
) const
{
    ULONG value = 0;
    if (!NT_SUCCESS(m_offloadFacade->QueryStandardizedKeywordSetting(&Keyword, &value)))
    {
        return DefaultValue;
    }

    return value;
}

_Use_decl_annotations_
void
NxOffloadManager::SetChecksumHardwareCapabilities(
    NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const * hardwareCapabilities
)
{
    SetHardwareCapabilities(hardwareCapabilities, OffloadType::Checksum, hardwareCapabilities->EvtAdapterOffloadSetChecksum);
}

_Use_decl_annotations_
void
NxOffloadManager::GetChecksumHardwareCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * HardwareCapabilities
) const
{
    auto const capabilities = GetHardwareCapabilities<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES>(OffloadType::Checksum);

    HardwareCapabilities->Size = sizeof(NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES);
    HardwareCapabilities->IPv4 = capabilities->IPv4;
    HardwareCapabilities->Tcp = capabilities->Tcp;
    HardwareCapabilities->Udp = capabilities->Udp;
}

_Use_decl_annotations_
void
NxOffloadManager::GetChecksumDefaultCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * DefaultCapabilities
) const
{
    //
    // Initialize the default capabilities to the hardware capabilities
    //
    GetChecksumHardwareCapabilities(DefaultCapabilities);

    //
    // The hardware capabilities supported by the client driver are turned ON by default
    // Unless there is a registry keyword which specifies that they need to be turned off
    //
    if (DefaultCapabilities->IPv4)
    {
        DECLARE_CONST_UNICODE_STRING(ipv4, L"*IPChecksumOffloadIPv4");
        if (IsKeywordSettingDisabled(ipv4))
        {
            DefaultCapabilities->IPv4 = FALSE;
        }
    }

    if (DefaultCapabilities->Tcp)
    {
        DECLARE_CONST_UNICODE_STRING(tcpIpv4, L"*TCPChecksumOffloadIPv4");
        DECLARE_CONST_UNICODE_STRING(tcpIpv6, L"*TCPChecksumOffloadIPv6");
        if (IsKeywordSettingDisabled(tcpIpv4) && IsKeywordSettingDisabled(tcpIpv6))
        {
            DefaultCapabilities->Tcp = FALSE;
        }
    }

    if (DefaultCapabilities->Udp)
    {
        DECLARE_CONST_UNICODE_STRING(udpIpv4, L"*UDPChecksumOffloadIPv4");
        DECLARE_CONST_UNICODE_STRING(udpIpv6, L"*UDPChecksumOffloadIPv6");
        if (IsKeywordSettingDisabled(udpIpv4) && IsKeywordSettingDisabled(udpIpv6))
        {
            DefaultCapabilities->Udp = FALSE;
        }
    }
}

_Use_decl_annotations_
void
NxOffloadManager::SetChecksumActiveCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const * ActiveCapabilities
)
{
    NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES netAdapterCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES),
        ActiveCapabilities->IPv4,
        ActiveCapabilities->Tcp,
        ActiveCapabilities->Udp,
        nullptr,
    };

    SetActiveCapabilities(netAdapterCapabilities, OffloadType::Checksum);
}

_Use_decl_annotations_
void
NxOffloadManager::SetTxChecksumHardwareCapabilities(
    NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * hardwareCapabilities
)
{
    SetHardwareCapabilities(hardwareCapabilities, OffloadType::TxChecksum, hardwareCapabilities->EvtAdapterOffloadSetTxChecksum);
}

_Use_decl_annotations_
void
NxOffloadManager::GetTxChecksumHardwareCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * HardwareCapabilities
) const
{
    auto const capabilities = GetHardwareCapabilities<NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES>(OffloadType::TxChecksum);

    HardwareCapabilities->Size = sizeof(NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES);
    HardwareCapabilities->Layer3Flags = static_cast<NET_CLIENT_ADAPTER_OFFLOAD_LAYER3_FLAGS>(capabilities->Layer3Flags);
    HardwareCapabilities->Layer3HeaderOffsetLimit = capabilities->Layer3HeaderOffsetLimit;
    HardwareCapabilities->Layer4Flags = static_cast<NET_CLIENT_ADAPTER_OFFLOAD_LAYER4_FLAGS>(capabilities->Layer4Flags);
    HardwareCapabilities->Layer4HeaderOffsetLimit = capabilities->Layer4HeaderOffsetLimit;
}

_Use_decl_annotations_
void
NxOffloadManager::GetTxChecksumSoftwareCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * SoftwareCapabilities
) const
{
    *SoftwareCapabilities = {};
    SoftwareCapabilities->Size = sizeof(NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES);

    // Enable software offload if any of the keywords are present.
    if (IsTxChecksumSoftwareFallbackEnabled())
    {
        SoftwareCapabilities->Layer3Flags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
            NetClientAdapterOffloadLayer3FlagIPv4WithOptions |
            NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
            NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

        SoftwareCapabilities->Layer4Flags = NetClientAdapterOffloadLayer4FlagTcpNoOptions |
            NetClientAdapterOffloadLayer4FlagTcpWithOptions |
            NetClientAdapterOffloadLayer4FlagUdp;
    }
}

_Use_decl_annotations_
void
NxOffloadManager::GetTxIPv4ChecksumDefaultCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
) const
{
    // Initialize the default capabilities to the software capabilities
    GetTxChecksumSoftwareCapabilities(DefaultCapabilities);

    DefaultCapabilities->Layer3Flags &= NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
        NetClientAdapterOffloadLayer3FlagIPv4WithOptions;

    auto const layer4Flags = NetClientAdapterOffloadLayer4FlagTcpNoOptions |
        NetClientAdapterOffloadLayer4FlagTcpWithOptions |
        NetClientAdapterOffloadLayer4FlagUdp;

    WI_ClearAllFlags(DefaultCapabilities->Layer4Flags, layer4Flags);

    // The capabilities supported by the client driver are turned ON by default
    // Unless there is a registry keyword which specifies that they need to be turned OFF
    if (WI_IsFlagSet(DefaultCapabilities->Layer3Flags, NetClientAdapterOffloadLayer3FlagIPv4NoOptions))
    {
        DECLARE_CONST_UNICODE_STRING(ipv4, L"*IPChecksumOffloadIPv4");
        auto const ipv4Value = GetStandardizedKeywordSetting(ipv4,
            NdisChecksumTxEnabled);

        auto const isTxIpv4ChecksumEnabled = WI_IsFlagSet(ipv4Value, NdisChecksumTxEnabled);
        if (!isTxIpv4ChecksumEnabled)
        {
            auto const ipFlags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
                NetClientAdapterOffloadLayer3FlagIPv4WithOptions;
            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipFlags);
        }
    }
}

_Use_decl_annotations_
void
NxOffloadManager::GetTxTcpChecksumDefaultCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
) const
{
    // Initialize the default capabilities to the software capabilities
    GetTxChecksumSoftwareCapabilities(DefaultCapabilities);

    DefaultCapabilities->Layer4Flags &= NetClientAdapterOffloadLayer4FlagTcpNoOptions |
        NetClientAdapterOffloadLayer4FlagTcpWithOptions;

    // The capabilities supported by the client driver are turned ON by default
    // Unless there is a registry keyword which specifies that they need to be turned OFF
    if (WI_IsFlagSet(DefaultCapabilities->Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions))
    {
        DECLARE_CONST_UNICODE_STRING(tcpIpv4, L"*TCPChecksumOffloadIPv4");
        auto const tcpIpv4Value = GetStandardizedKeywordSetting(tcpIpv4,
            NdisChecksumTxEnabled);

        DECLARE_CONST_UNICODE_STRING(tcpIpv6, L"*TCPChecksumOffloadIPv6");
        auto const tcpIpv6Value = GetStandardizedKeywordSetting(tcpIpv6,
            NdisChecksumTxEnabled);

        if (WI_IsFlagClear(tcpIpv4Value, NdisChecksumTxEnabled))
        {
            auto const ipv4Flags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
                NetClientAdapterOffloadLayer3FlagIPv4WithOptions;

            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipv4Flags);
        }

        if (WI_IsFlagClear(tcpIpv6Value, NdisChecksumTxEnabled))
        {
            auto const ipv6Flags = NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
                NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipv6Flags);
        }

        auto const tcpValue = tcpIpv4Value | tcpIpv6Value;
        auto const isTxTcpChecksumEnabled = WI_IsFlagSet(tcpValue, NdisChecksumTxEnabled);

        if (!isTxTcpChecksumEnabled)
        {
            auto const tcpFlags = NetClientAdapterOffloadLayer4FlagTcpNoOptions |
                NetClientAdapterOffloadLayer4FlagTcpWithOptions;

            WI_ClearAllFlags(DefaultCapabilities->Layer4Flags, tcpFlags);
        }
    }
}

_Use_decl_annotations_
void
NxOffloadManager::GetTxUdpChecksumDefaultCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
) const
{
    // Initialize the default capabilities to the hardware capabilities
    GetTxChecksumSoftwareCapabilities(DefaultCapabilities);

    DefaultCapabilities->Layer4Flags &= NetClientAdapterOffloadLayer4FlagUdp;

    // The capabilities supported by the client driver are turned ON by default
    // Unless there is a registry keyword which specifies that they need to be turned OFF
    if (WI_IsFlagSet(DefaultCapabilities->Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
    {
        DECLARE_CONST_UNICODE_STRING(udpIpv4, L"*UDPChecksumOffloadIPv4");
        auto const udpIpv4Value= GetStandardizedKeywordSetting(udpIpv4,
            NdisChecksumTxEnabled);

        DECLARE_CONST_UNICODE_STRING(udpIpv6, L"*UDPChecksumOffloadIPv6");
        auto const udpIpv6Value = GetStandardizedKeywordSetting(udpIpv6,
            NdisChecksumTxEnabled);

        if (WI_IsFlagClear(udpIpv4Value, NdisChecksumTxEnabled))
        {
            auto const ipv4Flags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
                NetClientAdapterOffloadLayer3FlagIPv4WithOptions;

            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipv4Flags);
        }

        if (WI_IsFlagClear(udpIpv6Value, NdisChecksumTxEnabled))
        {
            auto const ipv6Flags = NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
                NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipv6Flags);
        }

        auto const udpValue = udpIpv4Value | udpIpv6Value;
        auto const isTxUdpChecksumEnabled = WI_IsFlagSet(udpValue, NdisChecksumTxEnabled);

        if (!isTxUdpChecksumEnabled)
        {
            WI_ClearFlag(DefaultCapabilities->Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp);
        }
    }
}

_Use_decl_annotations_
void
NxOffloadManager::SetTxChecksumActiveCapabilities(
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveIPv4Capabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveTcpCapabilities,
    NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveUdpCapabilities
)
{
    NET_OFFLOAD_TX_CHECKSUM_CAPABILITIES netAdapterCapabilities = {
        {
            sizeof(NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES),
            static_cast<NET_ADAPTER_OFFLOAD_LAYER3_FLAGS>(ActiveIPv4Capabilities->Layer3Flags),
            static_cast<NET_ADAPTER_OFFLOAD_LAYER4_FLAGS>(ActiveIPv4Capabilities->Layer4Flags),
            0,
            0,
            nullptr,
            },
            {
            sizeof(NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES),
            static_cast<NET_ADAPTER_OFFLOAD_LAYER3_FLAGS>(ActiveTcpCapabilities->Layer3Flags),
            static_cast<NET_ADAPTER_OFFLOAD_LAYER4_FLAGS>(ActiveTcpCapabilities->Layer4Flags),
            0,
            0,
            nullptr,
            },
            {
            sizeof(NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES),
            static_cast<NET_ADAPTER_OFFLOAD_LAYER3_FLAGS>(ActiveUdpCapabilities->Layer3Flags),
            static_cast<NET_ADAPTER_OFFLOAD_LAYER4_FLAGS>(ActiveUdpCapabilities->Layer4Flags),
            0,
            0,
            nullptr,
        }
    };

    SetActiveCapabilities(netAdapterCapabilities, OffloadType::TxChecksum);
}

bool
NxOffloadManager::IsTxChecksumSoftwareFallbackEnabled(
    void
) const
{
    DECLARE_CONST_UNICODE_STRING(ipv4, L"*IPChecksumOffloadIPv4");
    DECLARE_CONST_UNICODE_STRING(tcpIpv4, L"*TCPChecksumOffloadIPv4");
    DECLARE_CONST_UNICODE_STRING(tcpIpv6, L"*TCPChecksumOffloadIPv6");
    DECLARE_CONST_UNICODE_STRING(udpIpv4, L"*UDPChecksumOffloadIPv4");
    DECLARE_CONST_UNICODE_STRING(udpIpv6, L"*UDPChecksumOffloadIPv6");

    // Enable software offload if any of the keywords are present.
    if (IsKeywordPresent(ipv4) ||
        IsKeywordPresent(tcpIpv4) ||
        IsKeywordPresent(tcpIpv6) ||
        IsKeywordPresent(udpIpv4) ||
        IsKeywordPresent(udpIpv6))
    {
        return true;
    }

    return false;
}

_Use_decl_annotations_
void
NxOffloadManager::SetRxChecksumCapabilities(
    NET_ADAPTER_OFFLOAD_RX_CHECKSUM_CAPABILITIES const * Capabilites
)
{
    SetOffloadCallback<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES>(OffloadType::RxChecksum, Capabilites->EvtAdapterOffloadSetRxChecksum);
}

_Use_decl_annotations_
void
NxOffloadManager::GetRxChecksumHardwareCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * HardwareCapabilities
) const
{
    auto nxOffload = FindNxOffload(OffloadType::RxChecksum);

    if (nxOffload->HasHardwareSupport() == true)
    {
        // If rx checksum capabilities are registered, we assume that the driver can perform all checksum offloads
        HardwareCapabilities->IPv4 = TRUE;
        HardwareCapabilities->Tcp = TRUE;
        HardwareCapabilities->Udp = TRUE;
    }
}

_Use_decl_annotations_
void
NxOffloadManager::GetRxChecksumDefaultCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * DefaultCapabilities
) const
{
    // Initialize the default capabilities to the hardware capabilities
    GetRxChecksumHardwareCapabilities(DefaultCapabilities);

    // The capabilities supported by the client driver are turned ON by default
    // Unless there is a registry keyword which specifies that they need to be turned OFF
    DECLARE_CONST_UNICODE_STRING(ipv4, L"*IPChecksumOffloadIPv4");
    auto const ipv4Value = GetStandardizedKeywordSetting(ipv4,
        NdisChecksumRxEnabled);
    DefaultCapabilities->IPv4 &= WI_IsFlagSet(ipv4Value, NdisChecksumRxEnabled);

    DECLARE_CONST_UNICODE_STRING(tcpIpv4, L"*TCPChecksumOffloadIPv4");
    auto const tcpIpv4Value = GetStandardizedKeywordSetting(tcpIpv4,
        NdisChecksumRxEnabled);

    DECLARE_CONST_UNICODE_STRING(tcpIpv6, L"*TCPChecksumOffloadIPv6");
    auto const tcpIpv6Value = GetStandardizedKeywordSetting(tcpIpv6,
        NdisChecksumRxEnabled);

    auto const tcpValue = tcpIpv4Value | tcpIpv6Value;
    DefaultCapabilities->Tcp &= WI_IsFlagSet(tcpValue, NdisChecksumRxEnabled);

    DECLARE_CONST_UNICODE_STRING(udpIpv4, L"*UDPChecksumOffloadIPv4");
    auto const udpIpv4Value = GetStandardizedKeywordSetting(udpIpv4,
        NdisChecksumRxEnabled);

    DECLARE_CONST_UNICODE_STRING(udpIpv6, L"*UDPChecksumOffloadIPv6");
    auto const udpIpv6Value = GetStandardizedKeywordSetting(udpIpv6,
        NdisChecksumRxEnabled);

    auto const udpValue = udpIpv4Value | udpIpv6Value;
    DefaultCapabilities->Udp &= WI_IsFlagSet(udpValue, NdisChecksumRxEnabled);
}

_Use_decl_annotations_
void
NxOffloadManager::SetRxChecksumActiveCapabilities(
    NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const * ActiveCapabilities
)
{
    NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES netAdapterCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES),
        ActiveCapabilities->IPv4,
        ActiveCapabilities->Tcp,
        ActiveCapabilities->Udp,
        nullptr,
    };

    SetActiveCapabilities(netAdapterCapabilities, OffloadType::RxChecksum);
}

_Use_decl_annotations_
void
NxOffloadManager::SetGsoHardwareCapabilities(
    NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES const * offloadCapabilities
)
{
    SetHardwareCapabilities(offloadCapabilities, OffloadType::Gso, offloadCapabilities->EvtAdapterOffloadSetGso);
}

_Use_decl_annotations_
void
NxOffloadManager::GetGsoHardwareCapabilities(
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * HardwareCapabilities
) const
{
    auto const capabilities = GetHardwareCapabilities<NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES>(OffloadType::Gso);

    HardwareCapabilities->Size = sizeof(NET_CLIENT_OFFLOAD_GSO_CAPABILITIES);
    HardwareCapabilities->Layer3Flags = static_cast<NET_CLIENT_ADAPTER_OFFLOAD_LAYER3_FLAGS>(capabilities->Layer3Flags);
    HardwareCapabilities->Layer4Flags = static_cast<NET_CLIENT_ADAPTER_OFFLOAD_LAYER4_FLAGS>(capabilities->Layer4Flags);
    HardwareCapabilities->Layer4HeaderOffsetLimit = capabilities->Layer4HeaderOffsetLimit;
    HardwareCapabilities->MaximumOffloadSize = capabilities->MaximumOffloadSize;
    HardwareCapabilities->MinimumSegmentCount = capabilities->MinimumSegmentCount;
}

_Use_decl_annotations_
void
NxOffloadManager::GetGsoSoftwareCapabilities(
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * SoftwareCapabilities
) const
{
    *SoftwareCapabilities = {};
    SoftwareCapabilities->Size = sizeof(NET_CLIENT_OFFLOAD_GSO_CAPABILITIES);

    // Enable software offload if any of the keywords are present.
    auto const isLsoSoftwareFallbackEnabled = IsLsoSoftwareFallbackEnabled();
    auto const isUsoSoftwareFallbackEnabled = IsUsoSoftwareFallbackEnabled();

    if (isLsoSoftwareFallbackEnabled || isUsoSoftwareFallbackEnabled)
    {
        SoftwareCapabilities->Layer3Flags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
            NetClientAdapterOffloadLayer3FlagIPv4WithOptions |
            NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
            NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

        // if the NIC supports Gso in hardware, we will use the its MaximumOffloadSize and MinimumSegmentCount
        // when reporting to TCPIP, so packets from TCPIP will be within NIC limitation and that make them more
        // likely to be offload in the hardware than falling back to software offload
        auto const capabilities = GetHardwareCapabilities<NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES>(OffloadType::Gso);

        SoftwareCapabilities->MaximumOffloadSize = (capabilities->MaximumOffloadSize == 0U)
            ? NETCX_GSO_SOFTWARE_FALLBACK_MAX_SIZE
            : capabilities->MaximumOffloadSize;

        SoftwareCapabilities->MinimumSegmentCount = (capabilities->MinimumSegmentCount == 0U)
            ? NETCX_GSO_SOFTWARE_FALLBACK_MIN_SEGMENT_COUNT
            : capabilities->MinimumSegmentCount;

        if (isLsoSoftwareFallbackEnabled)
        {
            SoftwareCapabilities->Layer4Flags |= NetClientAdapterOffloadLayer4FlagTcpNoOptions |
                NetClientAdapterOffloadLayer4FlagTcpWithOptions;
        }

        if (isUsoSoftwareFallbackEnabled)
        {
            SoftwareCapabilities->Layer4Flags |= NetClientAdapterOffloadLayer4FlagUdp;
        }
    }
}

_Use_decl_annotations_
void
NxOffloadManager::GetLsoDefaultCapabilities(
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * DefaultCapabilities
) const
{
    // Initialize the default capabilities to the software capabilities
    GetGsoSoftwareCapabilities(DefaultCapabilities);

    auto const ipv4Flags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
        NetClientAdapterOffloadLayer3FlagIPv4WithOptions;

    auto const ipv6Flags = NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
        NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

    DefaultCapabilities->Layer4Flags &= NetClientAdapterOffloadLayer4FlagTcpNoOptions |
        NetClientAdapterOffloadLayer4FlagTcpWithOptions;

    // The hardware capabilities supported by the client driver are turned ON by default,
    // unless there is a registry keyword which specifies that they need to be turned off.
    // If either LsoV1IPv4 or LsoV2IPv4 is disabled, LSO for IPv4 packets will be disabled.

    if (WI_IsFlagSet(DefaultCapabilities->Layer4Flags, NetClientAdapterOffloadLayer4FlagTcpNoOptions))
    {
        DECLARE_CONST_UNICODE_STRING(lsoV1Ipv4, L"*LsoV1IPv4");
        DECLARE_CONST_UNICODE_STRING(lsoV2Ipv4, L"*LsoV2IPv4");
        if (IsKeywordSettingDisabled(lsoV1Ipv4) || IsKeywordSettingDisabled(lsoV2Ipv4))
        {
            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipv4Flags);
        }

        DECLARE_CONST_UNICODE_STRING(lsoV2Ipv6, L"*LsoV2IPv6");
        if (IsKeywordSettingDisabled(lsoV2Ipv6))
        {
            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipv6Flags);
        }
    }
}

_Use_decl_annotations_
void
NxOffloadManager::GetUsoDefaultCapabilities(
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * DefaultCapabilities
) const
{
    // Initialize the default capabilities to the software capabilities
    GetGsoSoftwareCapabilities(DefaultCapabilities);

    auto const ipv4Flags = NetClientAdapterOffloadLayer3FlagIPv4NoOptions |
        NetClientAdapterOffloadLayer3FlagIPv4WithOptions;

    auto const ipv6Flags = NetClientAdapterOffloadLayer3FlagIPv6NoExtensions |
        NetClientAdapterOffloadLayer3FlagIPv6WithExtensions;

    DefaultCapabilities->Layer4Flags &= NetClientAdapterOffloadLayer4FlagUdp;

    // The hardware capabilities supported by the client driver are turned ON by default,
    // unless there is a registry keyword which specifies that they need to be turned off.

    if (WI_IsFlagSet(DefaultCapabilities->Layer4Flags, NetClientAdapterOffloadLayer4FlagUdp))
    {
        DECLARE_CONST_UNICODE_STRING(usoIpv4, L"*UsoIPv4");
        if (IsKeywordSettingDisabled(usoIpv4))
        {
            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipv4Flags);
        }

        DECLARE_CONST_UNICODE_STRING(usoIpv6, L"*UsoIPv6");
        if (IsKeywordSettingDisabled(usoIpv6))
        {
            WI_ClearAllFlags(DefaultCapabilities->Layer3Flags, ipv6Flags);
        }
    }
}

_Use_decl_annotations_
void
NxOffloadManager::SetGsoActiveCapabilities(
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const * LsoActiveCapabilities,
    NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const * UsoActiveCapabilities
)
{
    NET_OFFLOAD_GSO_CAPABILITIES netAdapterCapabilities = {
        {
        sizeof(NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES),
        static_cast<NET_ADAPTER_OFFLOAD_LAYER3_FLAGS>(LsoActiveCapabilities->Layer3Flags),
        static_cast<NET_ADAPTER_OFFLOAD_LAYER4_FLAGS>(LsoActiveCapabilities->Layer4Flags),
        LsoActiveCapabilities->Layer4HeaderOffsetLimit,
        LsoActiveCapabilities->MaximumOffloadSize,
        LsoActiveCapabilities->MinimumSegmentCount,
        nullptr,
        },
        {
        sizeof(NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES),
        static_cast<NET_ADAPTER_OFFLOAD_LAYER3_FLAGS>(UsoActiveCapabilities->Layer3Flags),
        static_cast<NET_ADAPTER_OFFLOAD_LAYER4_FLAGS>(UsoActiveCapabilities->Layer4Flags),
        UsoActiveCapabilities->Layer4HeaderOffsetLimit,
        UsoActiveCapabilities->MaximumOffloadSize,
        UsoActiveCapabilities->MinimumSegmentCount,
        nullptr,
        }
    };

    SetActiveCapabilities(netAdapterCapabilities, OffloadType::Gso);
}

bool
NxOffloadManager::IsLsoSoftwareFallbackEnabled(
    void
) const
{
    DECLARE_CONST_UNICODE_STRING(lsoV1Ipv4, L"*LsoV1IPv4");
    DECLARE_CONST_UNICODE_STRING(lsoV2Ipv4, L"*LsoV2IPv4");
    DECLARE_CONST_UNICODE_STRING(lsoV2Ipv6, L"*LsoV2IPv6");

    // Enable software offload if any of the keywords are present.
    if (IsKeywordPresent(lsoV1Ipv4) ||
        IsKeywordPresent(lsoV2Ipv4) ||
        IsKeywordPresent(lsoV2Ipv6))
    {
        return true;
    }

    return false;
}

bool
NxOffloadManager::IsUsoSoftwareFallbackEnabled(
    void
) const
{
    DECLARE_CONST_UNICODE_STRING(usoIpv4, L"*UsoIPv4");
    DECLARE_CONST_UNICODE_STRING(usoIpv6, L"*UsoIPv6");

    // Enable software offload if any of the keywords are present.
    if (IsKeywordPresent(usoIpv4) ||
        IsKeywordPresent(usoIpv6))
    {
        return true;
    }

    return false;
}

_Use_decl_annotations_
NTSTATUS
NxOffloadManager::RegisterExtensions(
    void
)
{
    for (auto const &offload : this->m_offloads)
    {
        if (offload->HasHardwareSupport())
        {
            auto const offloadType = offload->GetOffloadType();
            auto const isChecksumOffload = offloadType == OffloadType::Checksum ||
                offloadType == OffloadType::TxChecksum ||
                offloadType == OffloadType::RxChecksum;

            if (isChecksumOffload)
            {
                if (m_isChecksumRegistered == false)
                {
                    m_isChecksumRegistered = true;
                }
                else
                {
                    continue;
                }
            }

            CX_RETURN_IF_NOT_NT_SUCCESS(m_offloadFacade->RegisterExtension(
                offloadType,
                offload.get()));
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxOffloadManager::SetRscHardwareCapabilities(
    NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES const * HardwareCapabilities
)
{
    SetHardwareCapabilities(HardwareCapabilities, OffloadType::Rsc, HardwareCapabilities->EvtAdapterOffloadSetRsc);
}

_Use_decl_annotations_
void
NxOffloadManager::GetRscHardwareCapabilities(
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * HardwareCapabilities
) const
{
    auto const capabilities = GetHardwareCapabilities<NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES>(OffloadType::Rsc);

    HardwareCapabilities->Size = sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES);
    HardwareCapabilities->IPv4 = WI_IsFlagSet(capabilities->Layer3Flags, NetAdapterOffloadLayer3FlagIPv4NoOptions);
    HardwareCapabilities->IPv6 = WI_IsFlagSet(capabilities->Layer3Flags, NetAdapterOffloadLayer3FlagIPv6NoExtensions);
    HardwareCapabilities->TcpTimestampOption = capabilities->TcpTimestampOption;
}

_Use_decl_annotations_
void
NxOffloadManager::GetRscSoftwareCapabilities(
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * SoftwareCapabilities
) const
{
    *SoftwareCapabilities = {};
    SoftwareCapabilities->Size = sizeof(NET_CLIENT_OFFLOAD_RSC_CAPABILITIES);

    // Enable software offload if any of the keywords are present.
    if (IsRscSoftwareFallbackEnabled())
    {
        SoftwareCapabilities->IPv4 = TRUE;
        SoftwareCapabilities->IPv6 = TRUE;
        SoftwareCapabilities->TcpTimestampOption = TRUE;
    }
}

_Use_decl_annotations_
void
NxOffloadManager::GetRscDefaultCapabilities(
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * DefaultCapabilities
) const
{
    GetRscSoftwareCapabilities(DefaultCapabilities);

    //
    // The hardware capabilities supported by the client driver are turned ON by default
    // Unless there is a registry keyword which specifies that they need to be turned off
    //
    DECLARE_CONST_UNICODE_STRING(rscIpv4, L"*RscIPv4");
    DefaultCapabilities->IPv4 = DefaultCapabilities->IPv4 && !IsKeywordSettingDisabled(rscIpv4);

    DECLARE_CONST_UNICODE_STRING(rscIpv6, L"*RscIPv6");
    DefaultCapabilities->IPv6 = DefaultCapabilities->IPv6 && !IsKeywordSettingDisabled(rscIpv6);

    DECLARE_CONST_UNICODE_STRING(rscTimestamp, L"*RscTimestamp");
    DefaultCapabilities->TcpTimestampOption = DefaultCapabilities->TcpTimestampOption && !IsKeywordSettingDisabled(rscTimestamp);
}

_Use_decl_annotations_
void
NxOffloadManager::SetRscActiveCapabilities(
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const * ActiveCapabilities
)
{
    NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES netAdapterCapabilities = {};
    netAdapterCapabilities.Size = sizeof(NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES);
    netAdapterCapabilities.TcpTimestampOption = ActiveCapabilities->TcpTimestampOption;
    netAdapterCapabilities.EvtAdapterOffloadSetRsc = nullptr;

    if (ActiveCapabilities->IPv4)
    {
        netAdapterCapabilities.Layer3Flags |= NetAdapterOffloadLayer3FlagIPv4NoOptions;
        netAdapterCapabilities.Layer4Flags |= NetAdapterOffloadLayer4FlagTcpNoOptions;
    }

    if (ActiveCapabilities->IPv6)
    {
        netAdapterCapabilities.Layer3Flags |= NetAdapterOffloadLayer3FlagIPv6NoExtensions;
        netAdapterCapabilities.Layer4Flags |= NetAdapterOffloadLayer4FlagTcpNoOptions;
    }

    SetActiveCapabilities(netAdapterCapabilities, OffloadType::Rsc);
}

bool
NxOffloadManager::IsRscSoftwareFallbackEnabled(
    void
) const
{
    DECLARE_CONST_UNICODE_STRING(rscIpv4, L"*RscIPv4");
    DECLARE_CONST_UNICODE_STRING(rscIpv6, L"*RscIPv6");

    if (IsKeywordPresent(rscIpv4) ||
        IsKeywordPresent(rscIpv6))
    {
        return true;
    }

    return false;
}
