#include "Nx.hpp"
#include "NxAdapter.hpp"
#include "NxConfiguration.hpp"
#include "NxOffload.tmh"
#include "NxOffload.hpp"

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
    T *HardwareCapabilities,
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
NxOffloadFacade::RegisterPacketExtension(
    OffloadType type
    )
{
    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};

    switch (type)
    {
    case OffloadType::Checksum:
        extensionPrivate.Name = NET_PACKET_EXTENSION_CHECKSUM_NAME;
        extensionPrivate.Size = NET_PACKET_EXTENSION_CHECKSUM_VERSION_1_SIZE;
        extensionPrivate.Version = NET_PACKET_EXTENSION_CHECKSUM_VERSION_1;
        extensionPrivate.NonWdfStyleAlignment = sizeof(UINT32);
        break;

    case OffloadType::Lso:
        extensionPrivate.Name = NET_PACKET_EXTENSION_LSO_NAME;
        extensionPrivate.Size = NET_PACKET_EXTENSION_LSO_VERSION_1_SIZE;
        extensionPrivate.Version = NET_PACKET_EXTENSION_LSO_VERSION_1;
        extensionPrivate.NonWdfStyleAlignment = sizeof(UINT32);
        break;
    }

    return m_NxAdapter.RegisterPacketExtension(&extensionPrivate);
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

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
template <typename T>
void
NxOffloadManager::SetHardwareCapabilities(
    T * HardwareCapabilities, 
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
    T &ActiveCapabilities,
    OffloadType OffloadType
    )
{
    auto nxOffload = FindNxOffload(OffloadType);







    auto offloadCallback = static_cast<NxOffload<T> *>(nxOffload)->GetOffloadCallback();

    if (offloadCallback) 
    {
        offloadCallback(m_NxOffloadFacade->GetNetAdapter(), &ActiveCapabilities);
    }
}

_Use_decl_annotations_
NxOffloadBase *
NxOffloadManager::FindNxOffload(
    OffloadType OffloadType
    ) const
{
    for (auto const &offload : this->m_NxOffloads)
    {
        NxOffloadBase *offloadptr = offload.get();
        if (offloadptr->GetOffloadType() == OffloadType)
        {
            return offloadptr;
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
    PNET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES hardwareCapabilities,
    PFN_NET_ADAPTER_OFFLOAD_SET_CHECKSUM Callback
    )
{
    SetHardwareCapabilities<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES>(hardwareCapabilities, OffloadType::Checksum, Callback);
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
        ActiveCapabilities->Size,
        ActiveCapabilities->IPv4,
        ActiveCapabilities->Tcp,
        ActiveCapabilities->Udp
    };

    SetActiveCapabilities<NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES>(netAdapterCapabilities, OffloadType::Checksum);
}

_Use_decl_annotations_
void
NxOffloadManager::SetLsoHardwareCapabilities(
    PNET_ADAPTER_OFFLOAD_LSO_CAPABILITIES offloadCapabilities,
    PFN_NET_ADAPTER_OFFLOAD_SET_LSO Callback
    )
{
    SetHardwareCapabilities<NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES>(offloadCapabilities, OffloadType::Lso, Callback);
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
        ActiveCapabilities->Size,
        ActiveCapabilities->IPv4,
        ActiveCapabilities->IPv6,
        ActiveCapabilities->MaximumOffloadSize,
        ActiveCapabilities->MinimumSegmentCount
        };

    SetActiveCapabilities<NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES>(netAdapterCapabilities, OffloadType::Lso);
}

_Use_decl_annotations_
NTSTATUS
NxOffloadManager::RegisterPacketExtensions(
    void
    )
{
    for (auto const &offload : this->m_NxOffloads)
    {
        if (offload->HasHardwareSupport())
        {
            CX_RETURN_IF_NOT_NT_SUCCESS(m_NxOffloadFacade->RegisterPacketExtension(offload->GetOffloadType()));
        }
    }

    return STATUS_SUCCESS;
}
