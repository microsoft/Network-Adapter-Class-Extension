/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxBufferApi.cpp

Abstract:

    This module contains the "C" interface for the NxBuffer object.





Environment:

    kernel mode only

Revision History:

--*/

#include "Nx.hpp"

// Tracing support
extern "C" {
#include "NxBufferApi.tmh"
}

//
// extern the whole file
//
extern "C" {

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PVOID
NETEXPORT(NetPacketGetTypedContext)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NET_PACKET* NetPacket,
    _In_ PCNET_CONTEXT_TYPE_INFO TypeInfo
    )
{
    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyNetPacketUniqueType(pNxPrivateGlobals, NetPacket, TypeInfo);

    return NxBuffer::_GetDefaultClientContext(NetPacket);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG_PTR
NETEXPORT(NetPacketGet802_15_4Info)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NET_PACKET* NetPacket
    )
{
    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);

    return NxBuffer::_Get802_15_4Info(NetPacket);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
ULONG_PTR
NETEXPORT(NetPacketGet802_15_4Status)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NET_PACKET* NetPacket
    )
{
    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);

    return NxBuffer::_Get802_15_4Status(NetPacket);
}

} // extern "C"
