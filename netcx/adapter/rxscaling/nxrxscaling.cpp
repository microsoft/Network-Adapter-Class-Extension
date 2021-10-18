// Copyright (C) Microsoft Corporation. All rights reserved.

#ifdef _KERNEL_MODE

#include <ntddk.h>
#include <ntassert.h>
#include <nt/rtl/integermacro.h>

#else

#include <windows.h>
#include <ntstatus.h>
#include <ntintsafe.h>

#include <ifdef.h>

#endif

#include <wdf.h>
#include <preview/netadaptercx.h>
#include <NetClientBuffer.h>
#include <NetClientAdapter.h>
#include <NxTrace.hpp>

#include "NxRxScaling.hpp"

#include "NxQueue.hpp"

#include "idle/IdleStateMachine.hpp"
#include "WdfObjectCallback.hpp"

static
NDIS_RECEIVE_SCALE_CAPABILITIES const
TranslateCapabilities(
    _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES & Capabilities
)
{
    NDIS_RSS_CAPS_FLAGS rssCapsFlags = {
        NDIS_RSS_CAPS_SUPPORTS_INDEPENDENT_ENTRY_MOVE
    };

    if (Capabilities.ReceiveScalingHashTypes & NetAdapterReceiveScalingHashTypeToeplitz)
    {
        rssCapsFlags |= NdisHashFunctionToeplitz;
    }

    if ((Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv4) ||
        ((Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv4) &&
        (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv4Options)))
    {
        if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeTcp)
        {
            rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV4;
        }

        if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeUdp)
        {
            rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_UDP_IPV4;
        }
    }

    if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv6)
    {
        if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeTcp)
        {
            rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV6;
            if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv6Extensions)
            {
                rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV6_EX;
            }
        }

        if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeUdp)
        {
            rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_UDP_IPV6;
            if (Capabilities.ReceiveScalingProtocolTypes & NetAdapterReceiveScalingProtocolTypeIPv6Extensions)
            {
                rssCapsFlags |= NDIS_RSS_CAPS_HASH_TYPE_UDP_IPV6_EX;
            }
        }
    }

    NDIS_RECEIVE_SCALE_CAPABILITIES const capabilities = {
        {
            NDIS_OBJECT_TYPE_RSS_CAPABILITIES,
            NDIS_RECEIVE_SCALE_CAPABILITIES_REVISION_3,
            NDIS_SIZEOF_RECEIVE_SCALE_CAPABILITIES_REVISION_3,
        },
        rssCapsFlags,
        1, // NumberOfInterruptMessages, not used internally
        static_cast<ULONG>(Capabilities.NumberOfQueues),
        static_cast<USHORT>(Capabilities.IndirectionTableSize),
    };

    return capabilities;
}

_Use_decl_annotations_
NxRxScaling::NxRxScaling(
    NETADAPTER AdapterHandle
)
    : m_adapter(AdapterHandle)
{
}

_Use_decl_annotations_
void
NxRxScaling::SetCapabilities(
    NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const & Capabilities
)
{
    NT_FRE_ASSERT(sizeof(m_capabilities) >= Capabilities.Size);
    RtlCopyMemory(&m_capabilities, &Capabilities, Capabilities.Size);

    m_recvScaleCapabilities = TranslateCapabilities(m_capabilities);

    // Indicate the *NumRssQueues to NDIS if it's valid
    m_recvScaleCapabilities.NumberOfReceiveQueues = 
        (DefaultNumberOfQueues <= m_recvScaleCapabilities.NumberOfReceiveQueues &&
        DefaultNumberOfQueues > 0)
        ? DefaultNumberOfQueues
        : m_recvScaleCapabilities.NumberOfReceiveQueues;

    if (RssKeywordEnabled)
    {
        RecvScaleCapabilities = &m_recvScaleCapabilities;
    }
}

_Use_decl_annotations_
void
NxRxScaling::GetCapabilities(
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_CAPABILITIES * Capabilities
) const
{
    *Capabilities = {
        m_recvScaleCapabilities.NumberOfReceiveQueues,
        m_capabilities.IndirectionTableSize,
    };
}

_Use_decl_annotations_
NTSTATUS
NxRxScaling::Enable(
    void
)
{
    if (m_enabled)
    {
        return STATUS_SUCCESS;
    }

    auto const status = InvokePoweredCallback(
            m_capabilities.EvtAdapterReceiveScalingEnable,
            m_adapter,
            m_hashType,
            m_protocolType);

    if (status != STATUS_SUCCESS)
    {
        return status;
    }

    m_enabled = true;

    return status;
}

_Use_decl_annotations_
void
NxRxScaling::Disable(
    void
)
{
    if (! m_enabled)
    {
        return;
    }

    InvokePoweredCallback(
        m_capabilities.EvtAdapterReceiveScalingDisable,
        m_adapter);

    m_enabled = false;
}

_Use_decl_annotations_
NTSTATUS
NxRxScaling::SetHashSecretKey(
    _In_ NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const * HashSecretKey
)
{
    return InvokePoweredCallback(
        m_capabilities.EvtAdapterReceiveScalingSetHashSecretKey,
        m_adapter,
        reinterpret_cast<NET_ADAPTER_RECEIVE_SCALING_HASH_SECRET_KEY const*>(HashSecretKey));
}

_Use_decl_annotations_
NTSTATUS
NxRxScaling::SetIndirectionEntries(
    _Inout_ NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES * Entries
)
{
    auto table = reinterpret_cast<NET_ADAPTER_RECEIVE_SCALING_INDIRECTION_ENTRIES *>(Entries);

    for (size_t i = 0; i < Entries->Length; i++)
    {
        auto queue = reinterpret_cast<NxQueue *>(Entries->Entries[i].Queue);

        table->Entries[i].PacketQueue = reinterpret_cast<NETPACKETQUEUE>(queue->GetWdfObject());
    }

    return m_capabilities.EvtAdapterReceiveScalingSetIndirectionEntries(m_adapter, table);
}

_Use_decl_annotations_
NTSTATUS
NxRxScaling::SetHashInfo(
    NET_CLIENT_RECEIVE_SCALING_HASH_INFO const * HashInfo
)
{
    // To update RSS hash information when RSS is already enabled, we disable and re-enable RSS
    // with the new hash information. The protocol does not know that RSS was disabled on the
    // client adapter. If re-enabling RSS fails, the protocol and miniport RSS states will be
    // inconsistent. Bugcheck if this happens.
    Disable();

    auto const hashType = m_hashType;
    auto const protocolType = m_protocolType;

    m_hashType = static_cast<NET_ADAPTER_RECEIVE_SCALING_HASH_TYPE>(HashInfo->HashType);
    m_protocolType = static_cast<NET_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE>(HashInfo->ProtocolType);

    auto const status = Enable();

    if (status != STATUS_SUCCESS)
    {
        m_hashType = hashType;
        m_protocolType = protocolType;
        return status;
    }


    return status;
}

_Use_decl_annotations_
NTSTATUS
NxRxScaling::CheckHashInfoCapabilities(
    _In_ NET_CLIENT_RECEIVE_SCALING_HASH_INFO const * HashInfo
) const
{
    // Check if the hash information is supported by the client adapter
    if (WI_IsFlagClear(m_capabilities.ReceiveScalingHashTypes, NetAdapterReceiveScalingHashTypeToeplitz) &&
        WI_IsFlagSet(HashInfo->HashType, NetClientAdapterReceiveScalingHashTypeToeplitz))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(m_capabilities.ReceiveScalingProtocolTypes, NetAdapterReceiveScalingProtocolTypeIPv4) &&
        WI_IsFlagSet(HashInfo->ProtocolType, NetClientAdapterReceiveScalingProtocolTypeIPv4))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(m_capabilities.ReceiveScalingProtocolTypes, NetAdapterReceiveScalingProtocolTypeIPv6) &&
        WI_IsFlagSet(HashInfo->ProtocolType, NetClientAdapterReceiveScalingProtocolTypeIPv6))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(m_capabilities.ReceiveScalingProtocolTypes, NetAdapterReceiveScalingProtocolTypeIPv6Extensions) &&
        WI_IsFlagSet(HashInfo->ProtocolType, NetClientAdapterReceiveScalingProtocolTypeIPv6Extensions))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(m_capabilities.ReceiveScalingProtocolTypes, NetAdapterReceiveScalingProtocolTypeTcp) &&
        WI_IsFlagSet(HashInfo->ProtocolType, NetClientAdapterReceiveScalingProtocolTypeTcp))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (WI_IsFlagClear(m_capabilities.ReceiveScalingProtocolTypes, NetAdapterReceiveScalingProtocolTypeUdp) &&
        WI_IsFlagSet(HashInfo->ProtocolType, NetClientAdapterReceiveScalingProtocolTypeUdp))
    {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
bool
NxRxScaling::IsSupported(
    void
)
{
    if (m_recvScaleCapabilities.NumberOfReceiveQueues > 0)
    {
        return true;
    }

    return false;
}
