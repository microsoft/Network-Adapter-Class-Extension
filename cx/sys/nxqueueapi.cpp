//Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

// Tracing support
extern "C" {
#include "NxQueueApi.tmh"
}

//
// extern the whole file
//
extern "C" {

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetTxQueueCreate)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _Inout_ PNETTXQUEUE_INIT NetTxQueueInit,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES TxQueueAttributes,
    _In_ PNET_TXQUEUE_CONFIG Configuration,
    _Out_ NETTXQUEUE* TxQueue
    )
{
    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    auto InitContext = reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetTxQueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, InitContext);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, Configuration);
    
    // A NETTXQUEUE's parent is always the NETADAPTER
    Verifier_VerifyObjectAttributesParentIsNull(pNxPrivateGlobals, TxQueueAttributes);
    Verifier_VerifyObjectAttributesContextSize(pNxPrivateGlobals, TxQueueAttributes, MAXULONG);
    Verifier_VerifyNetTxQueueConfiguration(pNxPrivateGlobals, Configuration);

    *TxQueue = nullptr;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxTxQueue::_Create(InitContext, TxQueueAttributes, Configuration),
        "Tx queue creation failed. NxPrivateGlobals=%p", pNxPrivateGlobals);

    *TxQueue = (NETTXQUEUE)InitContext->CreatedQueueObject.get();

    FuncExit(FLAG_NET_QUEUE);
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(HIGH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetTxQueueNotifyMoreCompletedPacketsAvailable)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETTXQUEUE TxQueue
    )
{
    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);

    auto const txQueue = GetTxQueueFromHandle(TxQueue);
    txQueue->NotifyMorePacketsAvailable();

    FuncExit(FLAG_NET_QUEUE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetTxQueueInitGetQueueId)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    PNETTXQUEUE_INIT NetTxQueueInit
    )
{
    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    auto InitContext = reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetTxQueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, InitContext);

    FuncExit(FLAG_NET_QUEUE);
    return InitContext->QueueId;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
PNET_RING_BUFFER
NETEXPORT(NetTxQueueGetRingBuffer)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETTXQUEUE NetTxQueue
    )
{
    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto const txQueue = GetTxQueueFromHandle(NetTxQueue);

    FuncExit(FLAG_NET_QUEUE);
    return txQueue->m_info.RingBuffer;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRxQueueCreate)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _Inout_ PNETRXQUEUE_INIT NetRxQueueInit,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES RxQueueAttributes,
    _In_ PNET_RXQUEUE_CONFIG Configuration,
    _Out_ NETRXQUEUE* RxQueue
    )
{
    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    auto InitContext = reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetRxQueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, InitContext);
    Verifier_VerifyTypeSize(pNxPrivateGlobals, Configuration);

    // A NETRXQUEUE's parent is always the NETADAPTER
    Verifier_VerifyObjectAttributesParentIsNull(pNxPrivateGlobals, RxQueueAttributes);
    Verifier_VerifyObjectAttributesContextSize(pNxPrivateGlobals, RxQueueAttributes, MAXULONG);
    Verifier_VerifyNetRxQueueConfiguration(pNxPrivateGlobals, Configuration);

    *RxQueue = nullptr;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxRxQueue::_Create(InitContext, RxQueueAttributes, Configuration),
        "Rx queue creation failed. NxPrivateGlobals=%p", pNxPrivateGlobals);

    *RxQueue = (NETRXQUEUE)InitContext->CreatedQueueObject.get();

    FuncExit(FLAG_NET_QUEUE);
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(HIGH_LEVEL)
WDFAPI
VOID
NETEXPORT(NetRxQueueNotifyMoreReceivedPacketsAvailable)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETRXQUEUE RxQueue
    )
{
    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);

    auto const rxQueue = GetRxQueueFromHandle(RxQueue);
    rxQueue->NotifyMorePacketsAvailable();

    FuncExit(FLAG_NET_QUEUE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
ULONG
NETEXPORT(NetRxQueueInitGetQueueId)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    PNETRXQUEUE_INIT NetRxQueueInit
    )
{
    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    auto InitContext = reinterpret_cast<QUEUE_CREATION_CONTEXT*>(NetRxQueueInit);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueueInitContext(pNxPrivateGlobals, InitContext);

    FuncExit(FLAG_NET_QUEUE);
    return InitContext->QueueId;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetRxQueueConfigureDmaAllocator)(
    _In_ PNET_DRIVER_GLOBALS DriverGlobals,
    _In_ NETRXQUEUE RxQueue,
    _In_ WDFDMAENABLER Enabler
    )
{
    UNREFERENCED_PARAMETER((RxQueue, Enabler));

    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyNotNull(pNxPrivateGlobals, Enabler);

    auto rxQueue = GetRxQueueFromHandle(RxQueue);
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        rxQueue->ConfigureDmaAllocator(Enabler),
        "Failed to configure DMA allocator.");

    FuncExit(FLAG_NET_QUEUE);
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
PNET_RING_BUFFER
NETEXPORT(NetRxQueueGetRingBuffer)(
    _In_
    PNET_DRIVER_GLOBALS DriverGlobals,
    _In_
    NETRXQUEUE NetRxQueue
    )
{
    FuncEntry(FLAG_NET_QUEUE);

    PNX_PRIVATE_GLOBALS pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto rxQueue = GetRxQueueFromHandle(NetRxQueue);

    FuncExit(FLAG_NET_QUEUE);
    return rxQueue->m_info.RingBuffer;
}

} // extern "C"
