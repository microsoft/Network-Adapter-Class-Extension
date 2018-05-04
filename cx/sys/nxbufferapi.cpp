// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the "C" interface for the NxBuffer object.

--*/

#include "Nx.hpp"

#include "NxBufferApi.tmh"

//
// extern the whole file
//
extern "C" {

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PVOID
NETEXPORT(NetPacketGetTypedContext)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NET_DATAPATH_DESCRIPTOR const * Descriptor,
    _In_ NET_PACKET* NetPacket,
    _In_ PCNET_CONTEXT_TYPE_INFO TypeInfo
    )
{
    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);

    return NxBuffer::_GetTypedClientContext(Descriptor, NetPacket, TypeInfo);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PVOID
NETEXPORT(NetPacketGetContextFromToken)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NET_DATAPATH_DESCRIPTOR const * Descriptor,
    _In_ NET_PACKET* NetPacket,
    _In_ PNET_PACKET_CONTEXT_TOKEN Token
    )
{
    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(pNxPrivateGlobals);
    Verifier_VerifyNetPacketContextToken(pNxPrivateGlobals, Descriptor, NetPacket, Token);

    return NxBuffer::_GetClientContextFromToken(Descriptor, NetPacket, Token);
}

} // extern "C"
