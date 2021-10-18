// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxDevice.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "powerpolicy/NxPowerList.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include "NxPowerPolicyApi.tmh"

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

    auto powerOffload = reinterpret_cast<NxPowerOffload const *>(PowerOffload);
    return powerOffload->GetType();
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NETADAPTER
NTAPI
NETEXPORT(NetPowerOffloadGetAdapter)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETPOWEROFFLOAD PowerOffload
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto powerOffload = reinterpret_cast<NxPowerOffload const *>(PowerOffload);
    return powerOffload->GetAdapter();
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

    auto arpOffload = reinterpret_cast<NxArpOffload const *>(PowerOffload);
    arpOffload->GetParameters(Parameters);
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

    auto nsOffload = reinterpret_cast<NxNSOffload const *>(PowerOffload);
    nsOffload->GetParameters(Parameters);
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

    auto wakeSource = reinterpret_cast<NxWakeSource const *>(WakeSource);
    return wakeSource->GetType();
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

    auto wakeSource = reinterpret_cast<NxWakeSource const *>(WakeSource);
    return wakeSource->GetAdapter();
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
    auto powerList = new (List) NxPowerOffloadList();

    nxDevice->GetPowerOffloadList(powerList);
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

    auto powerOffload = powerList->GetPowerOffloadByIndex(Index);
    return powerOffload->GetHandle();
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
    auto powerList = new (List) NxWakeSourceList();

    nxDevice->GetWakeSourceList(powerList);
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

    auto powerList = GetNxPowerListFromWakeList(List);

    auto wakeSource = powerList->GetWakeSourceByIndex(Index);
    return wakeSource->GetHandle();
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

    auto bitmapPattern = reinterpret_cast<NxWakeBitmapPattern *>(WakeSource);
    bitmapPattern->GetParameters(Parameters);
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

    auto mediaChange = reinterpret_cast<NxWakeMediaChange *>(WakeSource);
    mediaChange->GetParameters(Parameters);
}
