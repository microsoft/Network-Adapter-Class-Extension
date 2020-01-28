// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxPowerEntry.hpp"

class NxPowerOffload : public NxPowerEntry
{

public:

    NxPowerOffload(
        _In_ NET_POWER_OFFLOAD_TYPE Type,
        _In_ NETADAPTER Adapter,
        _In_ ULONG Id
    );

    NETPOWEROFFLOAD
    GetHandle(
        void
    );

    NET_POWER_OFFLOAD_TYPE
    GetType(
        void
    ) const;

    ULONG
    GetID(
        void
    ) const;

protected:

    NET_POWER_OFFLOAD_TYPE const
        m_type;

    ULONG const
        m_id;
};

class NxArpOffload : public NxPowerOffload
{

public:

    static
    NTSTATUS
    CreateFromNdisPmOffload(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_PM_PROTOCOL_OFFLOAD const * NdisPmOffload,
        _Outptr_result_nullonfailure_ NxPowerOffload ** ArpOffload
    );

public:

    NxArpOffload(
        _In_ NETADAPTER Adapter,
        _In_ ULONG Id,
        _In_ NET_IPV4_ADDRESS RemoteIPv4Address,
        _In_ NET_IPV4_ADDRESS HostIPv4Address,
        _In_ NET_ADAPTER_LINK_LAYER_ADDRESS LinkLayerAddress
    );

    void
    GetParameters(
        _Inout_ NET_POWER_OFFLOAD_ARP_PARAMETERS * Parameters
    ) const;

private:

    NET_IPV4_ADDRESS const
        m_remoteIPv4Address;

    NET_IPV4_ADDRESS const
        m_hostIPv4Address;

    NET_ADAPTER_LINK_LAYER_ADDRESS const
        m_linkLayerAddress;

};

class NxNSOffload : public NxPowerOffload
{

public:

    static
    NTSTATUS
    CreateFromNdisPmOffload(
        _In_ NETADAPTER Adapter,
        _In_ NDIS_PM_PROTOCOL_OFFLOAD const * NdisPmOffload,
        _Outptr_result_nullonfailure_ NxPowerOffload ** NSOffload
    );

public:

    NxNSOffload(
        _In_ NETADAPTER Adapter,
        _In_ ULONG Id,
        _In_ NET_IPV6_ADDRESS RemoteIPv6Address,
        _In_ NET_IPV6_ADDRESS SolicitedNodeIPv6Address,
        _In_ NET_IPV6_ADDRESS TargetIPv6Addresses0,
        _In_ NET_IPV6_ADDRESS TargetIPv6Addresses1,
        _In_ NET_ADAPTER_LINK_LAYER_ADDRESS LinkLayerAddress
    );

    void
    GetParameters(
        _Inout_ NET_POWER_OFFLOAD_NS_PARAMETERS * Parameters
    ) const;

private:

    NET_IPV6_ADDRESS const
        m_remoteIPv6Address;

    NET_IPV6_ADDRESS const
        m_solicitedNodeIPv6Address;

    NET_IPV6_ADDRESS const
        m_targetIPv6Addresses[2];

    NET_ADAPTER_LINK_LAYER_ADDRESS const
        m_linkLayerAddress;
};
