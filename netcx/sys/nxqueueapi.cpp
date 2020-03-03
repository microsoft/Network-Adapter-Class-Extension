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
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto & InitContext = *reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetTxQueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, &InitContext);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, Configuration);

    // A NETPACKETQUEUE's parent is always the NETADAPTER
    Verifier_VerifyObjectAttributesParentIsNull(pNxPrivateGlobals, TxQueueAttributes);
    Verifier_VerifyObjectAttributesContextSize(pNxPrivateGlobals, TxQueueAttributes, MAXULONG);
    Verifier_VerifyNetPacketQueueConfiguration(pNxPrivateGlobals, Configuration);

    *TxQueue = nullptr;

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    CFX_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes, NxTxQueue);
    objectAttributes.ParentObject = InitContext.Adapter->GetFxObject();

    wil::unique_wdf_object object;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(&objectAttributes, &object),
        "WdfObjectCreate for NxTxQueue failed.");

    NxTxQueue * txQueue = new (NxTxQueue::_FromFxBaseObject(object.get()))
        NxTxQueue (object.get(), InitContext, InitContext.QueueId, *Configuration);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        txQueue->Initialize(InitContext),
        "Tx queue creation failed. NxPrivateGlobals=%p", pNxPrivateGlobals);

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
VOID
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
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto InitContext = reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetTxQueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, InitContext);

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
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTxQueueHandle(pNxPrivateGlobals, NetTxQueue);

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
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    auto & InitContext = *reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetRxQueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, &InitContext);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, Configuration);

    // A NETPACKETQUEUE's parent is always the NETADAPTER
    Verifier_VerifyObjectAttributesParentIsNull(pNxPrivateGlobals, RxQueueAttributes);
    Verifier_VerifyObjectAttributesContextSize(pNxPrivateGlobals, RxQueueAttributes, MAXULONG);
    Verifier_VerifyNetPacketQueueConfiguration(pNxPrivateGlobals, Configuration);

    *RxQueue = nullptr;

    WDF_OBJECT_ATTRIBUTES objectAttributes;
    CFX_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objectAttributes, NxRxQueue);
    objectAttributes.ParentObject = InitContext.Adapter->GetFxObject();

    wil::unique_wdf_object object;
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(&objectAttributes, &object),
        "WdfObjectCreate for NxTxQueue failed.");

    NxRxQueue * rxQueue = new (NxRxQueue::_FromFxBaseObject(object.get()))
    NxRxQueue(object.get(), InitContext, InitContext.QueueId, *Configuration);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        rxQueue->Initialize(InitContext),
        "Rx queue creation failed. NxPrivateGlobals=%p", pNxPrivateGlobals);

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
VOID
NETEXPORT(NetRxQueueNotifyMoreReceivedPacketsAvailable)(
    _In_
    NET_DRIVER_GLOBALS * DriverGlobals,
    _In_
    NETPACKETQUEUE RxQueue
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyRxQueueHandle(pNxPrivateGlobals, RxQueue);

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
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto InitContext = reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetRxQueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, InitContext);

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
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyRxQueueHandle(pNxPrivateGlobals, NetRxQueue);

    return GetRxQueueFromHandle(NetRxQueue)->GetRingCollection();
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetTxQueueInitAddPacketExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETTXQUEUE_INIT * QueueInit,
    _In_ const NET_PACKET_EXTENSION * ExtensionToAdd
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    QUEUE_CREATION_CONTEXT *queueCreationContext = reinterpret_cast<QUEUE_CREATION_CONTEXT *>(QueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, ExtensionToAdd);
    Verifier_VerifyNetPacketExtension(pNxPrivateGlobals, ExtensionToAdd);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, queueCreationContext);

    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = ExtensionToAdd->Name;
    extensionPrivate.Size = ExtensionToAdd->ExtensionSize;
    extensionPrivate.Version = ExtensionToAdd->Version;
    extensionPrivate.NonWdfStyleAlignment = ExtensionToAdd->Alignment + 1;

    return NxQueue::NetQueueInitAddPacketExtension(queueCreationContext, &extensionPrivate);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRxQueueInitAddPacketExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETRXQUEUE_INIT * QueueInit,
    _In_ NET_PACKET_EXTENSION const * ExtensionToAdd
)
{
    //
    // store the incoming NET_PACKET_EXTENSION into QUEUE_CREATION_CONTEXT,
    // the incoming extension memory (inc. the string memory) is caller-allocated and
    // expected to be valid until this call is over.
    // During queue creation (Net*QueueCreate), CX would allocate a new internal
    // NET_PACKET_EXTENSION_PRIVATE (apart from the one stored in adapter object)
    // and store it inside the queue object for offset queries
    //

    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    QUEUE_CREATION_CONTEXT *queueCreationContext = reinterpret_cast<QUEUE_CREATION_CONTEXT *>(QueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, ExtensionToAdd);
    Verifier_VerifyNetPacketExtension(pNxPrivateGlobals, ExtensionToAdd);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, queueCreationContext);

    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = ExtensionToAdd->Name;
    extensionPrivate.Size = ExtensionToAdd->ExtensionSize;
    extensionPrivate.Version = ExtensionToAdd->Version;
    extensionPrivate.NonWdfStyleAlignment = ExtensionToAdd->Alignment + 1;

    return NxQueue::NetQueueInitAddPacketExtension(queueCreationContext, &extensionPrivate);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetTxQueueGetExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETPACKETQUEUE NetTxQueue,
    _In_ const NET_PACKET_EXTENSION_QUERY* Query,
    _Out_ NET_EXTENSION* Extension
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    RtlZeroMemory(Extension, sizeof(*Extension));

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, Query);
    Verifier_VerifyNetPacketExtensionQuery(pNxPrivateGlobals, Query);
    Verifier_VerifyTxQueueHandle(pNxPrivateGlobals, NetTxQueue);

    NxTxQueue *txQueue = GetTxQueueFromHandle(NetTxQueue);

    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = Query->Name;
    extensionPrivate.Version = Query->Version;

    txQueue->GetExtension(&extensionPrivate, Extension);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRxQueueGetExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETPACKETQUEUE NetRxQueue,
    _In_ const NET_PACKET_EXTENSION_QUERY* Query,
    _Out_ NET_EXTENSION* Extension
)
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    RtlZeroMemory(Extension, sizeof(*Extension));

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, Query);
    Verifier_VerifyNetPacketExtensionQuery(pNxPrivateGlobals, Query);
    Verifier_VerifyRxQueueHandle(pNxPrivateGlobals, NetRxQueue);

    NxRxQueue *rxQueue = GetRxQueueFromHandle(NetRxQueue);

    NET_PACKET_EXTENSION_PRIVATE extensionPrivate = {};
    extensionPrivate.Name = Query->Name;
    extensionPrivate.Version = Query->Version;

    rxQueue->GetExtension(&extensionPrivate, Extension);
}
