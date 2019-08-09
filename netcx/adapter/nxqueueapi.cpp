// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>
#include <net/ring.h>
#include <net/packet.h>

#include "NxQueueApi.tmh"

#include "NxAdapter.hpp"
#include "NxQueue.hpp"
#include "verifier.hpp"
#include "version.hpp"

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetTxQueueCreate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETTXQUEUE_INIT * NetTxQueueInit,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * TxQueueAttributes,
    _In_ NET_PACKET_QUEUE_CONFIG * Configuration,
    _Out_ NETPACKETQUEUE* TxQueue
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto & InitContext = *reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetTxQueueInit);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyQueueInitContext(nxPrivateGlobals, &InitContext);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Configuration);

    // A NETPACKETQUEUE's parent is always the NETADAPTER
    Verifier_VerifyObjectAttributesParentIsNull(nxPrivateGlobals, TxQueueAttributes);
    Verifier_VerifyObjectAttributesContextSize(nxPrivateGlobals, TxQueueAttributes, MAXULONG);
    Verifier_VerifyNetPacketQueueConfiguration(nxPrivateGlobals, Configuration);

    *TxQueue = nullptr;

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    CFX_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes, NxTxQueue);
    objectAttributes.ParentObject = InitContext.Adapter->GetFxObject();

    wil::unique_wdf_object object;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(&objectAttributes, &object),
        "WdfObjectCreate for NxTxQueue failed.");

    NxTxQueue * txQueue = new (NxTxQueue::_FromFxBaseObject(object.get()))
        NxTxQueue (object.get(), *nxPrivateGlobals, InitContext, InitContext.QueueId, *Configuration);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        txQueue->Initialize(InitContext),
        "Tx queue creation failed. NxPrivateGlobals=%p", nxPrivateGlobals);

    if (TxQueueAttributes != WDF_NO_OBJECT_ATTRIBUTES)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfObjectAllocateContext(object.get(), TxQueueAttributes, NULL),
            "Failed to allocate client context. NxQueue=%p", txQueue);
    }

    // Note: We cannot have failure points after we allocate the client's context, otherwise
    // they might get their EvtCleanupContext callback even for a failed queue creation

    InitContext.CreatedQueueObject = wistd::move(object);
    *TxQueue = static_cast<NETPACKETQUEUE>(
        InitContext.CreatedQueueObject.get());

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(HIGH_LEVEL)
WDFAPI
void
NETEXPORT(NetTxQueueNotifyMoreCompletedPacketsAvailable)(
    _In_
    NET_DRIVER_GLOBALS * DriverGlobals,
    _In_
    NETPACKETQUEUE TxQueue
)
{
    Verifier_VerifyPrivateGlobals(GetPrivateGlobals(DriverGlobals));
    Verifier_VerifyTxQueueHandle(GetPrivateGlobals(DriverGlobals), TxQueue);

    GetTxQueueFromHandle(TxQueue)->NotifyMorePacketsAvailable();
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetTxQueueInitGetQueueId)(
    _In_
    NET_DRIVER_GLOBALS * DriverGlobals,
    _In_
    NETTXQUEUE_INIT * NetTxQueueInit
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto InitContext = reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetTxQueueInit);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyQueueInitContext(nxPrivateGlobals, InitContext);

    return InitContext->QueueId;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NET_RING_COLLECTION const *
NETEXPORT(NetTxQueueGetRingCollection)(
    _In_
    NET_DRIVER_GLOBALS * DriverGlobals,
    _In_
    NETPACKETQUEUE NetTxQueue
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTxQueueHandle(nxPrivateGlobals, NetTxQueue);

    return GetTxQueueFromHandle(NetTxQueue)->GetRingCollection();
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRxQueueCreate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETRXQUEUE_INIT * NetRxQueueInit,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * RxQueueAttributes,
    _In_ NET_PACKET_QUEUE_CONFIG * Configuration,
    _Out_ NETPACKETQUEUE * RxQueue
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    auto & InitContext = *reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetRxQueueInit);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyQueueInitContext(nxPrivateGlobals, &InitContext);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Configuration);

    // A NETPACKETQUEUE's parent is always the NETADAPTER
    Verifier_VerifyObjectAttributesParentIsNull(nxPrivateGlobals, RxQueueAttributes);
    Verifier_VerifyObjectAttributesContextSize(nxPrivateGlobals, RxQueueAttributes, MAXULONG);
    Verifier_VerifyNetPacketQueueConfiguration(nxPrivateGlobals, Configuration);

    *RxQueue = nullptr;

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    CFX_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes, NxRxQueue);
    objectAttributes.ParentObject = InitContext.Adapter->GetFxObject();

    wil::unique_wdf_object object;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(&objectAttributes, &object),
        "WdfObjectCreate for NxTxQueue failed.");

    NxRxQueue * rxQueue = new (NxRxQueue::_FromFxBaseObject(object.get()))
        NxRxQueue(object.get(), *nxPrivateGlobals, InitContext, InitContext.QueueId, *Configuration);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        rxQueue->Initialize(InitContext),
        "Rx queue creation failed. NxPrivateGlobals=%p", nxPrivateGlobals);

    if (RxQueueAttributes != WDF_NO_OBJECT_ATTRIBUTES)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfObjectAllocateContext(object.get(), RxQueueAttributes, NULL),
            "Failed to allocate client context. NxQueue=%p", rxQueue);
    }

    // Note: We cannot have failure points after we allocate the client's context, otherwise
    // they might get their EvtCleanupContext callback even for a failed queue creation

    InitContext.CreatedQueueObject = wistd::move(object);
    *RxQueue = static_cast<NETPACKETQUEUE>(
        InitContext.CreatedQueueObject.get());

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(HIGH_LEVEL)
WDFAPI
void
NETEXPORT(NetRxQueueNotifyMoreReceivedPacketsAvailable)(
    _In_
    NET_DRIVER_GLOBALS * DriverGlobals,
    _In_
    NETPACKETQUEUE RxQueue
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyRxQueueHandle(nxPrivateGlobals, RxQueue);

    GetRxQueueFromHandle(RxQueue)->NotifyMorePacketsAvailable();
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetRxQueueInitGetQueueId)(
    _In_
    NET_DRIVER_GLOBALS * DriverGlobals,
    _In_
    NETRXQUEUE_INIT * NetRxQueueInit
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto InitContext = reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetRxQueueInit);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyQueueInitContext(nxPrivateGlobals, InitContext);

    return InitContext->QueueId;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NET_RING_COLLECTION const *
NETEXPORT(NetRxQueueGetRingCollection)(
    _In_
    NET_DRIVER_GLOBALS * DriverGlobals,
    _In_
    NETPACKETQUEUE NetRxQueue
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyRxQueueHandle(nxPrivateGlobals, NetRxQueue);

    return GetRxQueueFromHandle(NetRxQueue)->GetRingCollection();
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetTxQueueGetExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETPACKETQUEUE NetTxQueue,
    _In_ NET_EXTENSION_QUERY const * Query,
    _Out_ NET_EXTENSION* Extension
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    RtlZeroMemory(Extension, sizeof(*Extension));

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Query);
    Verifier_VerifyNetExtensionQuery(nxPrivateGlobals, Query);
    Verifier_VerifyTxQueueHandle(nxPrivateGlobals, NetTxQueue);

    NxTxQueue *txQueue = GetTxQueueFromHandle(NetTxQueue);

    NET_EXTENSION_PRIVATE const extension = {
        Query->Name,
        0,
        Query->Version,
        0,
        Query->Type,
    };

    txQueue->GetExtension(&extension, Extension);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetRxQueueGetExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETPACKETQUEUE NetRxQueue,
    _In_ NET_EXTENSION_QUERY const * Query,
    _Out_ NET_EXTENSION* Extension
)
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    RtlZeroMemory(Extension, sizeof(*Extension));

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Query);
    Verifier_VerifyNetExtensionQuery(nxPrivateGlobals, Query);
    Verifier_VerifyRxQueueHandle(nxPrivateGlobals, NetRxQueue);

    NxRxQueue *rxQueue = GetRxQueueFromHandle(NetRxQueue);

    NET_EXTENSION_PRIVATE const extension = {
        Query->Name,
        0,
        Query->Version,
        0,
        Query->Type,
    };

    rxQueue->GetExtension(&extension, Extension);
}
