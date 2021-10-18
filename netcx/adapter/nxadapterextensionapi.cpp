// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxAdapterExtension.hpp"
#include "NxDriver.hpp"
#include "NxAdapter.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include "NxAdapterExtensionApi.tmh"

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NTAPI
NETEXPORT(NetDriverExtensionInitialize)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ WDFDRIVER Driver,
    _In_ const NET_DRIVER_EXTENSION_CONFIG * Config
    )
{
    auto privateGlobals = GetPrivateGlobals(DriverGlobals);
    auto const extensionType = MediaExtensionTypeFromClientGlobals(privateGlobals->ClientDriverGlobals);

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_ACCESS_DENIED,
        extensionType == MediaExtensionType::None,
        "Only media extensions are allowed to invoke this API");

    privateGlobals->ExtensionType = extensionType;

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, Config);
    Verifier_VerifyTypeSize(privateGlobals, Config);

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_ALREADY_REGISTERED,
        GetNxDriverFromWdfDriver(Driver) != nullptr,
        "Extension already initialized. WDFDRIVER=%p", Driver);

    //
    // If the media extension wants to be able to create NETADAPTERs we need to register with NDIS
    //

    if (Config->AllowNetAdapterCreation)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NxDriver::_CreateAndRegisterIfNeeded(privateGlobals),
            "Failed to register NDIS driver for media extension. WDFDRIVER=%p", Driver);
    }
    else
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            NxDriver::_CreateIfNeeded(Driver, privateGlobals),
            "Failed to create NxDriver context. WDFDRIVER=%p", Driver);
    }

    return STATUS_SUCCESS;
}

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

    if (!nxPrivateGlobals->IsClientVersionGreaterThanOrEqual(2, 1))
    {
        nxPrivateGlobals->ExtensionType = MediaExtensionTypeFromClientGlobals(nxPrivateGlobals->ClientDriverGlobals);
    }

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

    Extension drivers call this API to set a NDIS_OID_REQUEST preprocessing callback.

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
    Verifier_VerifyOidRequestDispatchSignature(nxPrivateGlobals, Context, OidSignature);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    auto dispatchContext = static_cast<DispatchContext *>(Context);

    nxAdapter->DispatchOidRequest(Request, dispatchContext);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetAdapterExtensionInitSetDirectOidRequestPreprocessCallback)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTEREXT_INIT * AdapterExtensionInit,
    _In_ PFN_NET_ADAPTER_PRE_PROCESS_DIRECT_OID_REQUEST PreprocessDirectOidRequest
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, AdapterExtensionInit);
    Verifier_VerifyNotNull(nxPrivateGlobals, PreprocessDirectOidRequest);

    auto adapterExtensionInit = GetAdapterExtensionInitFromHandle(nxPrivateGlobals, AdapterExtensionInit);
    adapterExtensionInit->DirectOidRequestPreprocessCallback = PreprocessDirectOidRequest;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
void
NETEXPORT(NetAdapterDispatchPreprocessedDirectOidRequest)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NDIS_OID_REQUEST * Request,
    _In_ WDFCONTEXT Context
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, Adapter);
    Verifier_VerifyNotNull(nxPrivateGlobals, Request);
    Verifier_VerifyOidRequestDispatchSignature(nxPrivateGlobals, Context, DirectOidSignature);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    auto dispatchContext = reinterpret_cast<DispatchContext *>(Context);

    nxAdapter->DispatchDirectOidRequest(Request, dispatchContext);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetExAdapterInitSetDirectOidPreprocessCallback)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTEREXT_INIT * AdapterExtensionInit,
    _In_ PFN_NETEX_ADAPTER_PREPROCESS_DIRECT_OID PreprocessDirectOid
)
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, AdapterExtensionInit);
    Verifier_VerifyNotNull(privateGlobals, PreprocessDirectOid);

    auto adapterExtensionInit = GetAdapterExtensionInitFromHandle(privateGlobals, AdapterExtensionInit);
    adapterExtensionInit->DirectOidPreprocessCallbackEx = PreprocessDirectOid;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
NTAPI
NETEXPORT(NetExAdapterDispatchPreprocessedDirectOid)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ NDIS_OID_REQUEST * Request,
    _In_ WDFCONTEXT Context
    )
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, Adapter);
    Verifier_VerifyNotNull(privateGlobals, Request);
    Verifier_VerifyOidRequestDispatchSignature(privateGlobals, Context, DirectOidSignature);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    auto dispatchContext = reinterpret_cast<DispatchContext *>(Context);

    return nxAdapter->DispatchDirectOidRequest(Request, dispatchContext);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetAdapterExtensionInitSetTxPeerDemuxCallback)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTEREXT_INIT * AdapterExtensionInit,
    _In_ PFN_NET_ADAPTER_TX_PEER_DEMUX TxPeerDemux
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, AdapterExtensionInit);
    Verifier_VerifyNotNull(nxPrivateGlobals, TxPeerDemux);

    auto adapterExtensionInit = GetAdapterExtensionInitFromHandle(nxPrivateGlobals, AdapterExtensionInit);
    adapterExtensionInit->WifiTxPeerDemux = TxPeerDemux;
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
    Verifier_VerifyNdisPmCapabilities(nxPrivateGlobals, Capabilities);

    auto adapterExtensionInit = GetAdapterExtensionInitFromHandle(nxPrivateGlobals, AdapterExtensionInit);

    RtlCopyMemory(
        &adapterExtensionInit->NdisPmCapabilities,
        Capabilities,
        Capabilities->Size);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterExtensionSetNdisPmCapabilities)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER Adapter,
    _In_ CONST NET_ADAPTER_NDIS_PM_CAPABILITIES * Capabilities
    )
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNdisPmCapabilities(privateGlobals, Capabilities);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);
    Verifier_VerifyAdapterNotStarted(privateGlobals, nxAdapter);

    auto adapterExtension = nxAdapter->GetExtension(privateGlobals);
    adapterExtension->SetNdisPmCapabilities(Capabilities);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NTAPI
NETEXPORT(NetAdapterInitAllocateContext)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTER_INIT * AdapterInit,
    _In_ WDF_OBJECT_ATTRIBUTES * Attributes,
    _Outptr_opt_ void ** Context
)
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, Attributes);

    auto adapterInit = GetAdapterInitFromHandle(privateGlobals, AdapterInit);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfObjectAllocateContext(
            adapterInit->Object.get(),
            Attributes,
            Context));

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void *
NTAPI
NETEXPORT(NetAdapterInitGetTypedContextWorker)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETADAPTER_INIT * AdapterInit,
    _In_ CONST WDF_OBJECT_CONTEXT_TYPE_INFO * TypeInfo
    )
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyNotNull(privateGlobals, TypeInfo);

    auto adapterInit = GetAdapterInitFromHandle(privateGlobals, AdapterInit);

    return WdfObjectGetTypedContextWorker(
        adapterInit->Object.get(),
        TypeInfo);
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetAdapterExtensionInitSetPowerPolicyCallbacks)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETADAPTEREXT_INIT * AdapterExtensionInit,
    _In_ NET_ADAPTER_EXTENSION_POWER_POLICY_CALLBACKS * Callbacks
)
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyExtensionGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyTypeSize(privateGlobals, Callbacks);

    auto adapterExtensionInit = GetAdapterExtensionInitFromHandle(privateGlobals, AdapterExtensionInit);

    RtlCopyMemory(
        &adapterExtensionInit->PowerPolicyCallbacks,
        Callbacks,
        Callbacks->Size);
}
