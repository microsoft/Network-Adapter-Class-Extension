// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NxPowerOffload.tmh"
#include "NxPowerOffload.hpp"

_Use_decl_annotations_
NxPowerOffload::NxPowerOffload(
    NET_POWER_OFFLOAD_TYPE Type,
    NETADAPTER Adapter,
    ULONG Id
)
    : NxPowerEntry(Adapter)
    , m_type(Type)
    , m_id(Id)
{
}

NETPOWEROFFLOAD
NxPowerOffload::GetHandle(
    void
)
{
    return reinterpret_cast<NETPOWEROFFLOAD>(this);
}

NET_POWER_OFFLOAD_TYPE
NxPowerOffload::GetType(
    void
) const
{
    return m_type;
}

ULONG
NxPowerOffload::GetID(
    void
) const
{
    return m_id;
}

_Use_decl_annotations_
NTSTATUS
NxArpOffload::CreateFromNdisPmOffload(
    NETADAPTER Adapter,
    NDIS_PM_PROTOCOL_OFFLOAD const * NdisPmOffload,
    NxPowerOffload ** ArpOffload
)
{
    *ArpOffload = nullptr;

    NT_FRE_ASSERT(NdisPmOffload->ProtocolOffloadType == NdisPMProtocolOffloadIdIPv4ARP);

    auto const & ndisArpParameters = NdisPmOffload->ProtocolOffloadParameters.IPv4ARPParameters;

    //
    // Compile time asserts to ensure the NetAdapterCx types are compatible with the NDIS types
    //
    static_assert(sizeof(NET_POWER_OFFLOAD_ARP_PARAMETERS::RemoteIPv4Address) == sizeof(ndisArpParameters.RemoteIPv4Address));
    static_assert(sizeof(NET_POWER_OFFLOAD_ARP_PARAMETERS::HostIPv4Address) == sizeof(ndisArpParameters.HostIPv4Address));
    static_assert(sizeof(NET_POWER_OFFLOAD_ARP_PARAMETERS::LinkLayerAddress.Address) >= sizeof(ndisArpParameters.MacAddress));

    auto const & remoteIPv4Address = *reinterpret_cast<NET_IPV4_ADDRESS const *>(&ndisArpParameters.RemoteIPv4Address[0]);
    auto const & hostIPv4Address = *reinterpret_cast<NET_IPV4_ADDRESS const *>(&ndisArpParameters.HostIPv4Address[0]);

    NET_ADAPTER_LINK_LAYER_ADDRESS linkLayerAddress;
    NET_ADAPTER_LINK_LAYER_ADDRESS_INIT(
        &linkLayerAddress,
        sizeof(ndisArpParameters.MacAddress),
        &NdisPmOffload->ProtocolOffloadParameters.IPv4ARPParameters.MacAddress[0]);

    auto arpOffload = wil::make_unique_nothrow<NxArpOffload>(
        Adapter,
        NdisPmOffload->ProtocolOffloadId,
        remoteIPv4Address,
        hostIPv4Address,
        linkLayerAddress);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !arpOffload);

    *ArpOffload = arpOffload.release();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxArpOffload::NxArpOffload(
    NETADAPTER Adapter,
    ULONG Id,
    NET_IPV4_ADDRESS RemoteIPv4Address,
    NET_IPV4_ADDRESS HostIPv4Address,
    NET_ADAPTER_LINK_LAYER_ADDRESS LinkLayerAddress
)
    : NxPowerOffload(NetPowerOffloadTypeArp, Adapter, Id)
    , m_remoteIPv4Address(RemoteIPv4Address)
    , m_hostIPv4Address(HostIPv4Address)
    , m_linkLayerAddress(LinkLayerAddress)
{
}

_Use_decl_annotations_
void
NxArpOffload::GetParameters(
    NET_POWER_OFFLOAD_ARP_PARAMETERS * Parameters
) const
{
    Parameters->Id = m_id;
    Parameters->RemoteIPv4Address = m_remoteIPv4Address;
    Parameters->HostIPv4Address = m_hostIPv4Address;
    Parameters->LinkLayerAddress = m_linkLayerAddress;
}

_Use_decl_annotations_
NTSTATUS
NxNSOffload::CreateFromNdisPmOffload(
    NETADAPTER Adapter,
    NDIS_PM_PROTOCOL_OFFLOAD const * NdisPmOffload,
    NxPowerOffload ** NSOffload
)
{
    *NSOffload = nullptr;

    NT_FRE_ASSERT(NdisPmOffload->ProtocolOffloadType == NdisPMProtocolOffloadIdIPv6NS);

    auto const & ndisNSParameters = NdisPmOffload->ProtocolOffloadParameters.IPv6NSParameters;

    //
    // Compile time asserts to ensure the NetAdapterCx types are compatible with the NDIS types
    //
    static_assert(sizeof(NET_POWER_OFFLOAD_NS_PARAMETERS::RemoteIPv6Address) == sizeof(ndisNSParameters.RemoteIPv6Address));
    static_assert(sizeof(NET_POWER_OFFLOAD_NS_PARAMETERS::SolicitedNodeIPv6Address) == sizeof(ndisNSParameters.SolicitedNodeIPv6Address));
    static_assert(sizeof(NET_POWER_OFFLOAD_NS_PARAMETERS::LinkLayerAddress.Address) >= sizeof(ndisNSParameters.MacAddress));
    static_assert(sizeof(NET_POWER_OFFLOAD_NS_PARAMETERS::TargetIPv6Addresses) == sizeof(ndisNSParameters.TargetIPv6Addresses));

    auto const & remoteIPv6Address = *reinterpret_cast<NET_IPV6_ADDRESS const *>(&ndisNSParameters.RemoteIPv6Address[0]);
    auto const & solicitedNodeIPv6Address = *reinterpret_cast<NET_IPV6_ADDRESS const *>(&ndisNSParameters.SolicitedNodeIPv6Address[0]);
    auto const & targetIPv6Addresses0 = *reinterpret_cast<NET_IPV6_ADDRESS const *>(&ndisNSParameters.TargetIPv6Addresses[0][0]);
    auto const & targetIPv6Addresses1 = *reinterpret_cast<NET_IPV6_ADDRESS const *>(&ndisNSParameters.TargetIPv6Addresses[1][0]);

    NET_ADAPTER_LINK_LAYER_ADDRESS linkLayerAddress;
    NET_ADAPTER_LINK_LAYER_ADDRESS_INIT(
        &linkLayerAddress,
        sizeof(ndisNSParameters.MacAddress),
        &ndisNSParameters.MacAddress[0]);

    auto nsOffload = wil::make_unique_nothrow<NxNSOffload>(
        Adapter,
        NdisPmOffload->ProtocolOffloadId,
        remoteIPv6Address,
        solicitedNodeIPv6Address,
        targetIPv6Addresses0,
        targetIPv6Addresses1,
        linkLayerAddress);

    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !nsOffload);

    *NSOffload = nsOffload.release();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxNSOffload::NxNSOffload(
    NETADAPTER Adapter,
    ULONG Id,
    NET_IPV6_ADDRESS RemoteIPv6Address,
    NET_IPV6_ADDRESS SolicitedNodeIPv6Address,
    NET_IPV6_ADDRESS TargetIPv6Addresses0,
    NET_IPV6_ADDRESS TargetIPv6Addresses1,
    NET_ADAPTER_LINK_LAYER_ADDRESS LinkLayerAddress
)
    : NxPowerOffload(NetPowerOffloadTypeNS, Adapter, Id)
    , m_remoteIPv6Address(RemoteIPv6Address)
    , m_solicitedNodeIPv6Address(SolicitedNodeIPv6Address)
    , m_linkLayerAddress(LinkLayerAddress)
    , m_targetIPv6Addresses{ TargetIPv6Addresses0, TargetIPv6Addresses1 }
{
}

_Use_decl_annotations_
void
NxNSOffload::GetParameters(
    _Inout_ NET_POWER_OFFLOAD_NS_PARAMETERS * Parameters
) const
{
    Parameters->Id = m_id;
    Parameters->RemoteIPv6Address = m_remoteIPv6Address;
    Parameters->SolicitedNodeIPv6Address = m_solicitedNodeIPv6Address;
    Parameters->TargetIPv6Addresses[0] = m_targetIPv6Addresses[0];
    Parameters->TargetIPv6Addresses[1] = m_targetIPv6Addresses[1];
    Parameters->LinkLayerAddress = m_linkLayerAddress;
}
