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
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, AdapterInit);

    auto adapterInit = GetAdapterInitFromHandle(privateGlobals, AdapterInit);
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
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIsMediaExtension(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, AdapterInit);

    privateGlobals->IsMediaExtension = true;

    auto adapterInit = GetAdapterInitFromHandle(privateGlobals, AdapterInit);

    Verifier_VerifyAdapterInitNotUsed(privateGlobals, adapterInit);

    AdapterExtensionInit extensionConfig;
    extensionConfig.PrivateGlobals = privateGlobals;

    if (!adapterInit->AdapterExtensions.append(extensionConfig))
        return nullptr;

    auto numberOfExtensions = adapterInit->AdapterExtensions.count();
    NT_ASSERT(numberOfExtensions > 0);
    auto lastExtensionIndex = numberOfExtensions - 1;

    return reinterpret_cast<NETADAPTEREXT_INIT *>(&adapterInit->AdapterExtensions[lastExtensionIndex]);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterExtensionInitSetNetRequestPreprocessCallback)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTEREXT_INIT * AdapterExtensionInit,
    _In_ PFN_NET_ADAPTER_PRE_PROCESS_NET_REQUEST PreprocessNetRequest
)
/*++
Routine Description:

    Extension drivers call this API to set a NETREQUEST preprocessing callback.

Arguments:

    Globals - Client driver's globals
    AdapterExtensionInit - Handle allocated with a call to NetAdapterExtensionInitAllocate
    PreprocessNetRequest - Pointer to a EVT_NET_ADAPTER_PRE_PROCESS_NET_REQUEST function

Return Value:

    None

Remarks

    None

--*/
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, AdapterExtensionInit);
    Verifier_VerifyNotNull(privateGlobals, PreprocessNetRequest);

    auto adapterExtensionInit = GetAdapterExtensionInitFromHandle(privateGlobals, AdapterExtensionInit);
    adapterExtensionInit->NetRequestPreprocessCallback = PreprocessNetRequest;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetAdapterDispatchPreprocessedNetRequest)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NETREQUEST Request
)
/*++
Routine Description:

    Extension drivers call this API to return a preprocessed NETREQUEST to NetAdapterCx.

Arguments:

    Globals - Client driver's globals
    Adapter - NETADAPTER that received the OID for preprocessing
    Request - NETREQUEST wrapping an OID request

Return Value:

    None

Remarks

    None

--*/
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, Adapter);
    Verifier_VerifyNotNull(privateGlobals, Request);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    nxAdapter->DispatchRequest(Request);
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
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, Adapter);

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
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, Adapter);

    auto const nxAdapter = GetNxAdapterFromHandle(Adapter);

    return nxAdapter->GetMtuSize();
}
