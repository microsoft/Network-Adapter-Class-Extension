#include "Nx.hpp"
#include "NxAdapter.hpp"
#include "NxConfiguration.hpp"
#include "NxOffload.tmh"
#include "NxOffload.hpp"

#include <net/checksumtypes_p.h>
#include <net/lsotypes_p.h>
#include <net/rsctypes_p.h>

_Use_decl_annotations_
NxOffloadBase::NxOffloadBase(
    OffloadType OffloadType
) : m_OffloadType(OffloadType)
{
}

_Use_decl_annotations_
OffloadType
NxOffloadBase::GetOffloadType(
    void
) const
{
    return this->m_OffloadType;
}

_Use_decl_annotations_
template <typename T>
NxOffload<T>::NxOffload(
    OffloadType OffloadType,
    T const &DefaultHardwareCapabilities
) : NxOffloadBase(OffloadType)
{
    RtlCopyMemory(&m_HardwareCapabilities, &DefaultHardwareCapabilities, sizeof(DefaultHardwareCapabilities));
}

_Use_decl_annotations_
template <typename T>
void
NxOffload<T>::SetHardwareCapabilities(
    T const * HardwareCapabilities,
    NxOffloadCallBack<T> OffloadCallback
)
{
    RtlCopyMemory(&m_HardwareCapabilities, HardwareCapabilities, sizeof(*HardwareCapabilities));
    m_NxOffloadCallback = OffloadCallback;

    //
    // Set a flag to indicate that the offload is supported by hardware
    // Packet extensions need to be registered later for offloads whose
    // capabilities are explicitly set by the client driver
    //
    m_HardwareSupport = true;
}

_Use_decl_annotations_
template <typename T>
NxOffloadCallBack<T>
NxOffload<T>::GetOffloadCallback(
    void
) const
{
    return m_NxOffloadCallback;
}

_Use_decl_annotations_
template <typename T>
T const *
NxOffload<T>::GetHardwareCapabilities(
    void
) const
{
    return &m_HardwareCapabilities;
}

_Use_decl_annotations_
template <typename T>
bool
NxOffload<T>::HasHardwareSupport(
    void
) const
{
    return m_HardwareSupport;
}

_Use_decl_annotations_
NxOffloadFacade::NxOffloadFacade(
    NxAdapter& nxAdapter
) : m_NxAdapter(nxAdapter)
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
    NTSTATUS status = NxConfiguration::_Create(
        m_NxAdapter.GetPrivateGlobals(),
        &m_NxAdapter,
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
        extensionPrivate.Name = NET_PACKET_EXTENSION_CHECKSUM_NAME;
        extensionPrivate.Size = NET_PACKET_EXTENSION_CHECKSUM_VERSION_1_SIZE;
        extensionPrivate.Version = NET_PACKET_EXTENSION_CHECKSUM_VERSION_1;
        extensionPrivate.NonWdfStyleAlignment = alignof(NET_PACKET_CHECKSUM);
        extensionPrivate.Type = NetExtensionTypePacket;
        break;

    case OffloadType::Lso:
        extensionPrivate.Name = NET_PACKET_EXTENSION_LSO_NAME;
        extensionPrivate.Size = NET_PACKET_EXTENSION_LSO_VERSION_1_SIZE;
        extensionPrivate.Version = NET_PACKET_EXTENSION_LSO_VERSION_1;
        extensionPrivate.NonWdfStyleAlignment = alignof(NET_PACKET_LSO);
        extensionPrivate.Type = NetExtensionTypePacket;
        break;

    case OffloadType::Rsc:
        extensionPrivate.Name = NET_PACKET_EXTENSION_RSC_NAME;
        extensionPrivate.Size = NET_PACKET_EXTENSION_RSC_VERSION_1_SIZE;
        extensionPrivate.Version = NET_PACKET_EXTENSION_RSC_VERSION_1;
        extensionPrivate.NonWdfStyleAlignment = alignof(NET_PACKET_RSC);
        extensionPrivate.Type = NetExtensionTypePacket;

        auto const nxOffload = static_cast<NxOffload<NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES> const *>(offload);
        auto const capabilities = nxOffload->GetHardwareCapabilities();

        if (capabilities->Timestamp)
        {
            CX_RETURN_IF_NOT_NT_SUCCESS(m_NxAdapter.RegisterExtension(extensionPrivate));
            extensionPrivate.Name = NET_PACKET_EXTENSION_RSC_TIMESTAMP_NAME;
            extensionPrivate.Size = NET_PACKET_EXTENSION_RSC_TIMESTAMP_VERSION_1_SIZE;
            extensionPrivate.Version = NET_PACKET_EXTENSION_RSC_TIMESTAMP_VERSION_1;
            extensionPrivate.NonWdfStyleAlignment = alignof(NET_PACKET_RSC_TIMESTAMP);
            extensionPrivate.Type = NetExtensionTypePacket;
        }
        break;
    }

    return m_NxAdapter.RegisterExtension(extensionPrivate);
}

_Use_decl_annotations_
NETADAPTER
NxOffloadFacade::GetNetAdapter(
    void
) const
{
    return m_NxAdapter.GetFxObject();
}

_Use_decl_annotations_
NxOffloadManager::NxOffloadManager(
    INxOffloadFacade *NxOffloadFacade
)
{
    m_NxOffloadFacade = wistd::unique_ptr<INxOffloadFacade>{NxOffloadFacade};
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

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_NxOffloads.append(wistd::move(checksumOffload)));

    const NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES lsoCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES),
        FALSE,
        FALSE,
        0,
        0
    };

    auto lsoOffload = wil::make_unique_nothrow<NxOffload<NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES>>(OffloadType::Lso, lsoCapabilities);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !lsoOffload);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_NxOffloads.append(wistd::move(lsoOffload)));

    const NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES rscCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES),
        FALSE,
        FALSE,
        FALSE
    };

    auto rscOffload = wil::make_unique_nothrow<NxOffload<NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES>>(OffloadType::Rsc, rscCapabilities);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !rscOffload);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !m_NxOffloads.append(wistd::move(rscOffload)));

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







    auto offloadCallback = static_cast<NxOffload<T> *>(nxOffload)->GetOffloadCallback();

    if (offloadCallback)
    {
        offloadCallback(m_NxOffloadFacade->GetNetAdapter(),
            reinterpret_cast<NETOFFLOAD>(const_cast<T *>(&ActiveCapabilities)));
    }
}

_Use_decl_annotations_
NxOffloadBase *
NxOffloadManager::FindNxOffload(
    OffloadType OffloadType
) const
{
    for (auto const & offload : this->m_NxOffloads)
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
    NTSTATUS status = m_NxOffloadFacade->QueryStandardizedKeywordSetting((PCUNICODE_STRING)&Keyword, &value);

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
        if (IsKeywordSettingDisabled(NDIS_STRING_CONST("*IPChecksumOffloadIPv4")))
        {
            DefaultCapabilities->IPv4 = FALSE;
        }
    }

    if (DefaultCapabilities->Tcp)
    {
        if (IsKeywordSettingDisabled(NDIS_STRING_CONST("*TCPChecksumOffloadIPv4")) && IsKeywordSettingDisabled(NDIS_STRING_CONST("*TCPChecksumOffloadIPv6")))
        {
            DefaultCapabilities->Tcp = FALSE;
        }
    }

    if (DefaultCapabilities->Udp)
    {
        if (IsKeywordSettingDisabled(NDIS_STRING_CONST("*UDPChecksumOffloadIPv4")) && IsKeywordSettingDisabled(NDIS_STRING_CONST("*UDPChecksumOffloadIPv6")))
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
NxOffloadManager::SetLsoHardwareCapabilities(
    NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES const * offloadCapabilities
)
{
    SetHardwareCapabilities(offloadCapabilities, OffloadType::Lso, offloadCapabilities->EvtAdapterOffloadSetLso);
}

_Use_decl_annotations_
void
NxOffloadManager::GetLsoHardwareCapabilities(
    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES * HardwareCapabilities
) const
{
    auto const capabilities = GetHardwareCapabilities<NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES>(OffloadType::Lso);

    HardwareCapabilities->IPv4 = capabilities->IPv4;
    HardwareCapabilities->IPv6 = capabilities->IPv6;
    HardwareCapabilities->MaximumOffloadSize = capabilities->MaximumOffloadSize;
    HardwareCapabilities->MinimumSegmentCount = capabilities->MinimumSegmentCount;
}

_Use_decl_annotations_
void
NxOffloadManager::GetLsoDefaultCapabilities(
    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES * DefaultCapabilities
) const
{

    //
    // Initialize the default capabilities to the hardware capabilities
    //
    GetLsoHardwareCapabilities(DefaultCapabilities);

    //
    // The hardware capabilities supported by the client driver are turned ON by default
    // Unless there is a registry keyword which specifies that they need to be turned off
    //
    if (DefaultCapabilities->IPv4)
    {
        if (IsKeywordSettingDisabled(NDIS_STRING_CONST("*LsoV1IPv4")) && IsKeywordSettingDisabled(NDIS_STRING_CONST("*LsoV2IPv4")))
        {
            DefaultCapabilities->IPv4 = FALSE;
        }
    }

    if (DefaultCapabilities->IPv6)
    {
        if (IsKeywordSettingDisabled(NDIS_STRING_CONST("*LsoV2IPv6")))
        {
            DefaultCapabilities->IPv6 = FALSE;
        }
    }

    if (!DefaultCapabilities->IPv4 && !DefaultCapabilities->IPv6)
    {
        DefaultCapabilities->MaximumOffloadSize = 0;
        DefaultCapabilities->MinimumSegmentCount = 0;
    }
}

_Use_decl_annotations_
void
NxOffloadManager::SetLsoActiveCapabilities(
    NET_CLIENT_OFFLOAD_LSO_CAPABILITIES const * ActiveCapabilities
)
{
    NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES netAdapterCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES),
        ActiveCapabilities->IPv4,
        ActiveCapabilities->IPv6,
        ActiveCapabilities->MaximumOffloadSize,
        ActiveCapabilities->MinimumSegmentCount,
        nullptr,
    };

    SetActiveCapabilities(netAdapterCapabilities, OffloadType::Lso);
}

_Use_decl_annotations_
NTSTATUS
NxOffloadManager::RegisterExtensions(
    void
)
{
    for (auto const &offload : this->m_NxOffloads)
    {
        if (offload->HasHardwareSupport())
        {
            CX_RETURN_IF_NOT_NT_SUCCESS(m_NxOffloadFacade->RegisterExtension(
                offload->GetOffloadType(),
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

    HardwareCapabilities->IPv4 = capabilities->IPv4;
    HardwareCapabilities->IPv6 = capabilities->IPv6;
    HardwareCapabilities->Timestamp = capabilities->Timestamp;
}

_Use_decl_annotations_
void
NxOffloadManager::GetRscDefaultCapabilities(
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * DefaultCapabilities
) const
{
    //
    // Initialize the default capabilities to the hardware capabilities
    //
    GetRscHardwareCapabilities(DefaultCapabilities);

    //
    // The hardware capabilities supported by the client driver are turned ON by default
    // Unless there is a registry keyword which specifies that they need to be turned off
    //

    DefaultCapabilities->IPv4 = DefaultCapabilities->IPv4 && !IsKeywordSettingDisabled(NDIS_STRING_CONST("*RscIPv4"));
    DefaultCapabilities->IPv6 = DefaultCapabilities->IPv6 && !IsKeywordSettingDisabled(NDIS_STRING_CONST("*RscIPv6"));
    DefaultCapabilities->Timestamp = DefaultCapabilities->Timestamp && !IsKeywordSettingDisabled(NDIS_STRING_CONST("*RscTimestamp"));
}

_Use_decl_annotations_
void
NxOffloadManager::SetRscActiveCapabilities(
    NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const * ActiveCapabilities
)
{
    NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES netAdapterCapabilities = {
        sizeof(NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES),
        ActiveCapabilities->IPv4,
        ActiveCapabilities->IPv6,
        ActiveCapabilities->Timestamp,
        nullptr,
    };

    SetActiveCapabilities(netAdapterCapabilities, OffloadType::Rsc);
}
