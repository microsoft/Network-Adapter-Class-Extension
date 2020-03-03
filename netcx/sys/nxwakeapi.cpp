// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the "C" interface for the NxWake object.

--*/

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxWakeApi.tmh"

#include "NxWake.hpp"
#include "verifier.hpp"
#include "version.hpp"

WDFAPI
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
NETEXPORT(NetPowerSettingsGetEnabledWakePatternFlags)(
    _In_     NET_DRIVER_GLOBALS *   Globals,
    _In_     NETPOWERSETTINGS       NetPowerSettings
)
/*++
Routine Description:
    Obtain the EnabledWoLPacketPatterns associated with the adapater. This
    API must only be called during a power transition.

Arguments:
     NetPowerSettings - The NetPowerSettings object

Returns:
    A bitmap flags represting which Wake patterns need
    to be enabled in the hardware for arming the device for wake.
    Refer to the documentation of NDIS_PM_PARAMETERS for more details

    This API must only be called during a power transition.
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);

    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    return nxWake->GetEnabledWakePacketPatterns();
}

WDFAPI
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
NETEXPORT(NetPowerSettingsGetEnabledProtocolOffloadFlags)(
    _In_     NET_DRIVER_GLOBALS *                        Globals,
    _In_     NETPOWERSETTINGS                             Adapter
)
/*++
Routine Description:
    Returns a bitmap flag representing the enabled protocol offloads.

    Refer to the documentation of NDIS_PM_PARAMETERS EnabledProtocolOffloads
    for more details

Arguments:
    NetPowerSettings - The NetPowerSettings object

Returns:
    A bitmap flag representing EnabledProtocolOffloads
    Refer to the documentation of NDIS_PM_PARAMETERS for more details

    This API must only be called during a power transition.
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(Adapter);





    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    return  nxWake->GetEnabledProtocolOffloads();
}

WDFAPI
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
NETEXPORT(NetPowerSettingsGetEnabledMediaSpecificWakeUpEvents)(
    _In_     NET_DRIVER_GLOBALS *   Globals,
    _In_     NETPOWERSETTINGS        NetPowerSettings
)
/*++
Routine Description:
    Returns a ULONG value that contains a bitwise OR of flags.
    These flags specify the media-specific wake-up events that a network adapter
    supports. Refer to the documentation of NDIS_PM_PARAMETERS for more details

    This API must only be called during a power transition.
Arguments:
    NetPowerSettings - The NetPowerSettings object

Returns:
    A bitmap flags represting which media-specific wake-up events need
    to be enabled in the hardware for arming the device for wake.

    Refer to the documentation of NDIS_PM_PARAMETERS for more details

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);
    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    return nxWake->GetEnabledMediaSpecificWakeUpEvents();
}

WDFAPI
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
NETEXPORT(NetPowerSettingsGetEnabledWakeUpFlags)(
    _In_     NET_DRIVER_GLOBALS *   Globals,
    _In_     NETPOWERSETTINGS        NetPowerSettings
)
/*++
Routine Description:
    Returns a ULONG value that contains a bitwise OR of NDIS_PM_WAKE_ON_ Xxx flags.
    This API must only be called during a power transition.

Arguments:
    NetPowerSettings - The NetPowerSettings object

Returns:
    A bitmap flags represting the WakeUp flags need
    to be enabled in the hardware for arming the device for wake.
    Refer to the documentation of WakeUpFlags field of NDIS_PM_PARAMETERS
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);

    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    return  nxWake->GetEnabledWakeUpFlags();
}

WDFAPI
_IRQL_requires_max_(PASSIVE_LEVEL)
PNDIS_PM_WOL_PATTERN
NETEXPORT(NetPowerSettingsGetWakePattern)(
    _In_     NET_DRIVER_GLOBALS *   Globals,
    _In_     NETPOWERSETTINGS       NetPowerSettings,
    _In_     ULONG                  Index
)
/*++
Routine Description:
    Returns a PNDIS_PM_WOL_PATTERN structure at Index (0 based).

    This API must only be called during a power transition or from the
    EvtPreviewWakePattern callback. In both cases, the driver should only access/examine
    the  PNDIS_PM_WOL_PATTERN (obtained from this API) and should NOT cache or retain
    a reference to the WoL pattern(s). This is because the Cx will automatically
    release it while handling WOL pattern removal.

Arguments:
    NetPowerSettings - The NetPowerSettings object
    Index - 0 based index. This value must be < NetPowerSettingsGetWoLPatternCount

Returns:
    PNDIS_PM_WOL_PATTERN structure at Index. Returns NULL if Index is invalid.

--*/
{
    PNX_NET_POWER_ENTRY nxWakePatternEntry;

    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);
    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    nxWakePatternEntry = nxWake->GetEntryAtIndex(Index,
                                                NxPowerEntryTypeWakePattern);
    Verifier_VerifyNotNull(pNxPrivateGlobals, nxWakePatternEntry);

    return nxWakePatternEntry ? &(nxWakePatternEntry->NdisWoLPattern) : NULL;
}

WDFAPI
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
NETEXPORT(NetPowerSettingsGetWakePatternCount)(
    _In_     NET_DRIVER_GLOBALS *   Globals,
    _In_     NETPOWERSETTINGS       NetPowerSettings
)
/*++
Routine Description:
    Returns the number of WoL Patterns stored in the NETPOWERSETTINGS object.

    IMPORTANT: This includes both, wake patterns that are enabled and
    disabled. The driver can use the  NetPowerSettingsIsWakePatternEnabled
    API to check if a particular wake pattern is enabled.

    This API must only be called during a power transition or from the
    EvtPreviewWakePattern callback.

Arguments:
    NetPowerSettings - The NetPowerSettings object

Returns:
    Returns the number of WoL Patterns stored in the NETPOWERSETTINGS object.
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);

    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    return nxWake->GetWakePatternCount();
}

WDFAPI
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetPowerSettingsIsWakePatternEnabled)(
    _In_     NET_DRIVER_GLOBALS *   Globals,
    _In_     NETPOWERSETTINGS       NetPowerSettings,
    _In_     PNDIS_PM_WOL_PATTERN   NdisPmWolPattern
)
/*++
Routine Description:
    This API can be used to determine if the PNDIS_PM_WOL_PATTERN obtained from
    a prior call to NetPowerSettingsGetWoLPattern is enabled. If it is enabled
    the driver must program its hardware to enable the wake pattern during a
    power down transition.

    This API must only be called during a power transition or from the
    EvtPreviewWakePattern callback.

Arguments:
    NetPowerSettings - The NetPowerSettings object
    NdisPmWolPattern - Pointer to NDIS_PM_WOL_PATTERN structure that must be
                obtained by a prior call to NetPowerSettingsGetWoLPattern

Returns:
    Returns TRUE if the WoL pattern has been enabled and driver must enable it
        in its hardware. Returns FALSE if the pattern is not enabled.
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);
    PNX_NET_POWER_ENTRY nxWakeEntry;

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);

    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    nxWakeEntry = CONTAINING_RECORD(NdisPmWolPattern,
                                NX_NET_POWER_ENTRY,
                                NdisWoLPattern);
    return nxWakeEntry->Enabled;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetPowerSettingsGetProtocolOffloadCount)(
    _In_ NET_DRIVER_GLOBALS *   DriverGlobals,
    _In_ NETPOWERSETTINGS       NetPowerSettings
)
/*++
Routine Description:
    Returns the number of protocol offloads stored in the NETPOWERSETTINGS object.

    IMPORTANT: This includes both, offloads that are enabled and disabled.
    The driver can use the NetPowerSettingsIsProtocolOffloadEnabled API to check
    if a particular protocol offload is enabled.

    This API must only be called during a power transition or from the
    EvtPreviewProtocolOffload callback.

Arguments:
    NetPowerSettings - The NetPowerSettings object

Returns:
    Returns the number of protocol offloads stored in the NETPOWERSETTINGS object.
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);

    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    return nxWake->GetProtocolOffloadCount();
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
PNDIS_PM_PROTOCOL_OFFLOAD
NETEXPORT(NetPowerSettingsGetProtocolOffload)(
    _In_ NET_DRIVER_GLOBALS *   DriverGlobals,
    _In_ NETPOWERSETTINGS       NetPowerSettings,
    _In_ ULONG                  Index
)
/*++
Routine Description:
    Returns a PNDIS_PM_PROTOCOL_OFFLOAD structure at the provided Index.
    Note that the Index is 0 based.

    This API must only be called during a power transition or from the
    EvtPreviewProtocolOffload callback. In both cases, the driver should only
    access/examine the  PNDIS_PM_PROTOCOL_OFFLOAD (obtained from this API) and
    should NOT cache or retain a reference to the protocol offload.
    This is because the Cx will automatically release it while handling
    offload removal without notifying the driver

Arguments:
    NetPowerSettings - The NetPowerSettings object
    Index - 0 based index. This value must be < NetPowerSettingsGetProtocolOffloadCount

Returns:
    PNDIS_PM_PROTOCOL_OFFLOAD structure at Index. Returns NULL if Index is invalid.

--*/
{
    PNX_NET_POWER_ENTRY nxPowerEntry;

    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);
    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    nxPowerEntry = nxWake->GetEntryAtIndex(Index,  NxPowerEntryTypeProtocolOffload);
    Verifier_VerifyNotNull(pNxPrivateGlobals, (PVOID)nxPowerEntry);

    return nxPowerEntry ? &(nxPowerEntry->NdisProtocolOffload) : NULL;
}


WDFAPI
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NETEXPORT(NetPowerSettingsIsProtocolOffloadEnabled)(
    _In_ NET_DRIVER_GLOBALS *      DriverGlobals,
    _In_ NETPOWERSETTINGS          NetPowerSettings,
    _In_ PNDIS_PM_PROTOCOL_OFFLOAD ProtocolOffload
)
/*++
Routine Description:
    This API can be used to determine if the PNDIS_PM_PROTOCOL_OFFLOAD obtained
    from a prior call to NetPowerSettingsGetProtocolOffload is enabled.

    This API must only be called during a power transition or from the
    EvtPreviewProtocolOffload callback.

Arguments:
    NetPowerSettings - The NetPowerSettings object
    NdisOffload - Pointer to PNDIS_PM_PROTOCOL_OFFLOAD structure that must be
                obtained by a prior call to NetPowerSettingsGetProtocolOffload

Returns:
    Returns TRUE if the protocol offload has been enabled.
    Returns FALSE if the offload is not enabled.
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    PNX_NET_POWER_ENTRY nxPowerEntry;

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxWake = GetNxWakeFromHandle(NetPowerSettings);

    Verifier_VerifyNetPowerSettingsAccessible(pNxPrivateGlobals, nxWake);

    nxPowerEntry = CONTAINING_RECORD(ProtocolOffload,
                                NX_NET_POWER_ENTRY,
                                NdisProtocolOffload);
    return nxPowerEntry->Enabled;
}

