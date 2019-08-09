// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxPowerPolicyApi.tmh"

#include "NxDevice.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "powerpolicy/NxPowerList.hpp"
#include "verifier.hpp"
#include "version.hpp"

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NET_POWER_OFFLOAD_TYPE
NTAPI
NETEXPORT(NetPowerOffloadGetType)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETPOWEROFFLOAD PowerOffload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto const powerEntry = reinterpret_cast<NX_NET_POWER_ENTRY *>(PowerOffload);

    switch (powerEntry->NdisProtocolOffload.ProtocolOffloadType)
    {
        case NdisPMProtocolOffloadIdIPv4ARP:
            return NetPowerOffloadTypeArp;
        case NdisPMProtocolOffloadIdIPv6NS:
            return NetPowerOffloadTypeNS;
    }

    NT_FRE_ASSERTMSG(
        "Internal NetCx error. This code path should not be reached",
        false);

    return static_cast<NET_POWER_OFFLOAD_TYPE>(0);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetPowerOffloadGetArpParameters)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETPOWEROFFLOAD PowerOffload,
    _Inout_ NET_POWER_OFFLOAD_ARP_PARAMETERS * Parameters
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Parameters);

    auto const powerEntry = reinterpret_cast<NX_NET_POWER_ENTRY *>(PowerOffload);

    NT_FRE_ASSERT(powerEntry->NdisProtocolOffload.ProtocolOffloadType == NdisPMProtocolOffloadIdIPv4ARP);

    auto const & remoteIPv4Address = powerEntry->NdisProtocolOffload.ProtocolOffloadParameters.IPv4ARPParameters.RemoteIPv4Address;
    auto const & hostIPv4Address = powerEntry->NdisProtocolOffload.ProtocolOffloadParameters.IPv4ARPParameters.HostIPv4Address;
    auto const & linkLayerAddress = powerEntry->NdisProtocolOffload.ProtocolOffloadParameters.IPv4ARPParameters.MacAddress;

    static_assert(sizeof(Parameters->RemoteIPv4Address) == sizeof(remoteIPv4Address));
    static_assert(sizeof(Parameters->HostIPv4Address) == sizeof(hostIPv4Address));
    static_assert(sizeof(Parameters->LinkLayerAddress.Address) >= sizeof(linkLayerAddress));

    Parameters->Id = powerEntry->NdisProtocolOffload.ProtocolOffloadId;

    RtlCopyMemory(
        &Parameters->RemoteIPv4Address,
        &remoteIPv4Address[0],
        sizeof(remoteIPv4Address));

    RtlCopyMemory(
        &Parameters->HostIPv4Address,
        &hostIPv4Address[0],
        sizeof(hostIPv4Address));

    RtlCopyMemory(
        &Parameters->LinkLayerAddress.Address[0],
        &linkLayerAddress[0],
        sizeof(linkLayerAddress));

    Parameters->LinkLayerAddress.Length = sizeof(linkLayerAddress);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetPowerOffloadGetNSParameters)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETPOWEROFFLOAD PowerOffload,
    _Inout_ NET_POWER_OFFLOAD_NS_PARAMETERS * Parameters
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Parameters);

    auto const powerEntry = reinterpret_cast<NX_NET_POWER_ENTRY *>(PowerOffload);

    NT_FRE_ASSERT(powerEntry->NdisProtocolOffload.ProtocolOffloadType == NdisPMProtocolOffloadIdIPv6NS);

    auto const & remoteIPv6Address = powerEntry->NdisProtocolOffload.ProtocolOffloadParameters.IPv6NSParameters.RemoteIPv6Address;
    auto const & solicitedNodeIPv6Address = powerEntry->NdisProtocolOffload.ProtocolOffloadParameters.IPv6NSParameters.SolicitedNodeIPv6Address;
    auto const & linkLayerAddress = powerEntry->NdisProtocolOffload.ProtocolOffloadParameters.IPv6NSParameters.MacAddress;
    auto const & targetIPv6Addresses = powerEntry->NdisProtocolOffload.ProtocolOffloadParameters.IPv6NSParameters.TargetIPv6Addresses;

    static_assert(sizeof(Parameters->RemoteIPv6Address) == sizeof(remoteIPv6Address));
    static_assert(sizeof(Parameters->SolicitedNodeIPv6Address) == sizeof(solicitedNodeIPv6Address));
    static_assert(sizeof(Parameters->LinkLayerAddress.Address) >= sizeof(linkLayerAddress));
    static_assert(sizeof(Parameters->TargetIPv6Addresses) == sizeof(targetIPv6Addresses));

    Parameters->Id = powerEntry->NdisProtocolOffload.ProtocolOffloadId;

    RtlCopyMemory(
        &Parameters->RemoteIPv6Address,
        &remoteIPv6Address[0],
        sizeof(remoteIPv6Address));

    RtlCopyMemory(
        &Parameters->SolicitedNodeIPv6Address,
        &solicitedNodeIPv6Address[0],
        sizeof(solicitedNodeIPv6Address));

    RtlCopyMemory(
        &Parameters->TargetIPv6Addresses[0],
        &targetIPv6Addresses[0],
        sizeof(targetIPv6Addresses));

    RtlCopyMemory(
        &Parameters->LinkLayerAddress.Address[0],
        &linkLayerAddress[0],
        sizeof(linkLayerAddress));

    Parameters->LinkLayerAddress.Length = sizeof(linkLayerAddress);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NET_WAKE_SOURCE_TYPE
NTAPI
NETEXPORT(NetWakeSourceGetType)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETWAKESOURCE WakeSource
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto const powerEntry = reinterpret_cast<NX_NET_POWER_ENTRY *>(WakeSource);

    switch (powerEntry->NdisWoLPattern.WoLPacketType)
    {
        case NdisPMWoLPacketBitmapPattern:
            return NetWakeSourceTypeBitmapPattern;
        case NdisPMWoLPacketMagicPacket:
            return NetWakeSourceTypeMagicPacket;
    }

    NT_FRE_ASSERTMSG(
        "Internal NetCx error. This code path should not be reached",
        false);

    return static_cast<NET_WAKE_SOURCE_TYPE>(0);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NETADAPTER
NTAPI
NETEXPORT(NetWakeSourceGetAdapter)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETWAKESOURCE WakeSource
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto const powerEntry = reinterpret_cast<NX_NET_POWER_ENTRY const *>(WakeSource);
    return powerEntry->Adapter;
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetDeviceGetPowerOffloadList)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ WDFDEVICE Device,
    _Inout_ NET_POWER_OFFLOAD_LIST * List
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, List);

    auto nxDevice = GetNxDeviceFromHandle(Device);
    auto powerList = new (&List->Reserved[0]) NxPowerList(NxPowerEntryTypeProtocolOffload);

    nxDevice->GetPowerList(powerList);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
SIZE_T
NTAPI
NETEXPORT(NetPowerOffloadListGetCount)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ const NET_POWER_OFFLOAD_LIST * List
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, List);
    Verifier_VerifyPowerOffloadList(nxPrivateGlobals, List);

    auto powerList = GetNxPowerListFromOffloadList(List);
    return powerList->GetCount();
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NETPOWEROFFLOAD
NTAPI
NETEXPORT(NetPowerOffloadListGetElement)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ const NET_POWER_OFFLOAD_LIST * List,
    _In_ SIZE_T Index
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, List);
    Verifier_VerifyPowerOffloadList(nxPrivateGlobals, List);

    auto const powerList = GetNxPowerListFromOffloadList(List);

    return reinterpret_cast<NETPOWEROFFLOAD>(
        const_cast<NX_NET_POWER_ENTRY *>(
            powerList->GetElement(Index)));
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetDeviceGetWakeSourceList)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ WDFDEVICE Device,
    _Inout_ NET_WAKE_SOURCE_LIST * List
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, List);

    auto nxDevice = GetNxDeviceFromHandle(Device);
    auto powerList = new (&List->Reserved[0]) NxPowerList(NxPowerEntryTypeWakePattern);

    nxDevice->GetPowerList(powerList);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
SIZE_T
NTAPI
NETEXPORT(NetWakeSourceListGetCount)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ const NET_WAKE_SOURCE_LIST * List
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, List);
    Verifier_VerifyWakeSourceList(nxPrivateGlobals, List);

    auto powerList = GetNxPowerListFromWakeList(List);
    return powerList->GetCount();
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NETWAKESOURCE
NTAPI
NETEXPORT(NetWakeSourceListGetElement)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ const NET_WAKE_SOURCE_LIST * List,
    _In_ SIZE_T Index
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, List);
    Verifier_VerifyWakeSourceList(nxPrivateGlobals, List);

    auto const powerList = GetNxPowerListFromWakeList(List);

    return reinterpret_cast<NETWAKESOURCE>(
        const_cast<NX_NET_POWER_ENTRY *>(
            powerList->GetElement(Index)));
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetWakeSourceGetBitmapParameters)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETWAKESOURCE WakeSource,
    _Inout_ NET_WAKE_SOURCE_BITMAP_PARAMETERS * Parameters
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Parameters);

    auto const powerEntry = reinterpret_cast<NX_NET_POWER_ENTRY *>(WakeSource);
    NT_FRE_ASSERT(powerEntry->NdisWoLPattern.WoLPacketType == NdisPMWoLPacketBitmapPattern);

    FillBitmapParameters(
        powerEntry->NdisWoLPattern,
        *Parameters);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetWakeSourceGetMediaChangeParameters)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETWAKESOURCE WakeSource,
    _Inout_ NET_WAKE_SOURCE_MEDIA_CHANGE_PARAMETERS * Parameters
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Parameters);

    UNREFERENCED_PARAMETER(WakeSource);
}
