// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxAdapterExtension.tmh"
#include "NxAdapterExtension.hpp"

#include "NxAdapter.hpp"
#include "verifier.hpp"
#include "version.hpp"

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NETADAPTER
NETEXPORT(NetAdapterInitGetCreatedAdapter)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER_INIT * AdapterInit
)
/*++
Routine Description:

    Returns the NETADAPTER handle created using the AdapterInit object.

Arguments:

    Globals - Client driver's globals
    AdapterInit - An adapter init object

Return Value:

    The NETADAPTER handle created using the init object

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, AdapterInit);

    auto adapterInit = GetAdapterInitFromHandle(nxPrivateGlobals, AdapterInit);
    return adapterInit->CreatedAdapter;
}

__inline
AdapterExtensionInit *
GetAdapterExtensionInitFromHandle(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NETADAPTEREXT_INIT * Handle
)
{
    auto adapterExtensionInit = reinterpret_cast<AdapterExtensionInit *>(Handle);

    Verifier_VerifyAdapterExtensionInitSignature(PrivateGlobals, adapterExtensionInit);

    return adapterExtensionInit;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NETADAPTEREXT_INIT *
NETEXPORT(NetAdapterExtensionInitAllocate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER_INIT * AdapterInit
)
/*++
Routine Description:

    NetAdapter extension drivers call this API to allocate an opaque extension init
    object. This API is not available for NetAdapterCx client drivers.

Arguments:

    Globals - Extension driver's globals
    AdapterInit - An adapter init handle.

Return Value:

    An pointer to an extension init object.

Remarks

    The extension init object's lifetime is tied to the AdapterInit parameter.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    nxPrivateGlobals->ExtensionType = MediaExtensionTypeFromClientGlobals(nxPrivateGlobals->ClientDriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, AdapterInit);

    auto adapterInit = GetAdapterInitFromHandle(nxPrivateGlobals, AdapterInit);

    Verifier_VerifyAdapterInitNotUsed(nxPrivateGlobals, adapterInit);

    AdapterExtensionInit extensionConfig;
    extensionConfig.PrivateGlobals = nxPrivateGlobals;

    if (!adapterInit->AdapterExtensions.append(extensionConfig))
        return nullptr;

    auto numberOfExtensions = adapterInit->AdapterExtensions.count();
    NT_ASSERT(numberOfExtensions > 0);
    auto lastExtensionIndex = numberOfExtensions - 1;

    return reinterpret_cast<NETADAPTEREXT_INIT *>(&adapterInit->AdapterExtensions[lastExtensionIndex]);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetAdapterExtensionInitSetOidRequestPreprocessCallback)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTEREXT_INIT * AdapterExtensionInit,
    _In_ PFN_NET_ADAPTER_PRE_PROCESS_OID_REQUEST PreprocessOidRequest
)
/*++
Routine Description:

    Extension drivers call this API to set a NETREQUEST preprocessing callback.

Arguments:

    Globals - Client driver's globals
    AdapterExtensionInit - Handle allocated with a call to NetAdapterExtensionInitAllocate
    PreprocessOidRequest - Pointer to a EVT_NET_ADAPTER_PRE_PROCESS_OID_REQUEST function

Return Value:

    None

Remarks

    None

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, AdapterExtensionInit);
    Verifier_VerifyNotNull(nxPrivateGlobals, PreprocessOidRequest);

    auto adapterExtensionInit = GetAdapterExtensionInitFromHandle(nxPrivateGlobals, AdapterExtensionInit);
    adapterExtensionInit->OidRequestPreprocessCallback = PreprocessOidRequest;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetAdapterDispatchPreprocessedOidRequest)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NDIS_OID_REQUEST * Request,
    _In_ WDFCONTEXT Context
)
/*++
Routine Description:

    Extension drivers call this API to return a preprocessed OID request to NetAdapterCx.

Arguments:

    Globals - Client driver's globals
    Adapter - NETADAPTER that received the OID for preprocessing
    Request - NDIS OID request
    DispatchContext - Internal framework context, opaque to client driver

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, Adapter);
    Verifier_VerifyNotNull(nxPrivateGlobals, Request);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    auto dispatchContext = static_cast<DispatchContext *>(Context);

    nxAdapter->DispatchOidRequest(Request, dispatchContext);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
WDFOBJECT
NETEXPORT(NetAdapterGetParent)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter
)
/*++
Routine Description:

    Gets the parent of the given NETADAPTER.

Arguments:

    Globals - Client driver's globals
    Adapter - A NETADAPTER object handle

Return Value:

    None

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, Adapter);

    // At the moment a NETADAPTER's parent is always a WDFDEVICE
    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    return nxAdapter->GetDevice();
}

WDFAPI
_IRQL_requires_(PASSIVE_LEVEL)
ULONG
NETEXPORT(NetAdapterGetLinkLayerMtuSize)(
    _In_ NET_DRIVER_GLOBALS *                   DriverGlobals,
    _In_ NETADAPTER                             Adapter
)
/*++
Routine Description:

    Get the Mtu of the given NETADAPTER.

Arguments:

    Globals - Client driver's globals
    Adapter - A NETADAPTER object handle

Returns:
    The Mtu of the given NETADAPTER
  --*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, Adapter);

    auto const nxAdapter = GetNxAdapterFromHandle(Adapter);

    return nxAdapter->GetMtuSize();
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterExtensionInitSetNdisPmCapabilities)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _Inout_ NETADAPTEREXT_INIT* AdapterExtensionInit,
    _In_ CONST NET_ADAPTER_NDIS_PM_CAPABILITIES* Capabilities
    )
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Capabilities);
    Verifier_VerifyNdisPmCapabilities(nxPrivateGlobals, Capabilities);

    auto adapterExtensionInit = GetAdapterExtensionInitFromHandle(nxPrivateGlobals, AdapterExtensionInit);

    RtlCopyMemory(
        &adapterExtensionInit->NdisPmCapabilities,
        Capabilities,
        Capabilities->Size);
}
