// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include "NxDefaultDatapath.hpp"
#include "NxApi.hpp"
#include "NxAdapter.hpp"

#include "NxDefaultDatapath.tmh"

static
EVT_NET_ADAPTER_CREATE_TXQUEUE
    DefaultCreateTxQueue;

static
EVT_NET_ADAPTER_CREATE_RXQUEUE
    DefaultCreateRxQueue;

static
EVT_PACKET_QUEUE_ADVANCE
    DefaultTxPacketQueueAdvance;

static
EVT_PACKET_QUEUE_ADVANCE
    DefaultRxPacketQueueAdvance;

static
EVT_PACKET_QUEUE_SET_NOTIFICATION_ENABLED
    DefaultPacketQueueSetNotificationEnabled;

static
EVT_PACKET_QUEUE_CANCEL
    DefaultTxPacketQueueCancel;

static
EVT_PACKET_QUEUE_CANCEL
    DefaultRxPacketQueueCancel;

NET_ADAPTER_DATAPATH_CALLBACKS
GetDefaultDatapathCallbacks(
    void
)
{
    NET_ADAPTER_DATAPATH_CALLBACKS datapathCallbacks;
    NET_ADAPTER_DATAPATH_CALLBACKS_INIT(
        &datapathCallbacks,
        DefaultCreateTxQueue,
        DefaultCreateRxQueue);

    return datapathCallbacks;
}

_Use_decl_annotations_
NTSTATUS
DefaultCreateTxQueue(
    NETADAPTER Adapter,
    NETTXQUEUE_INIT * QueueInit
)
{
    NET_PACKET_QUEUE_CONFIG queueConfig;
    NET_PACKET_QUEUE_CONFIG_INIT(
        &queueConfig,
        DefaultTxPacketQueueAdvance,
        DefaultPacketQueueSetNotificationEnabled,
        DefaultTxPacketQueueCancel);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    NETPACKETQUEUE txQueue;
    return NETEXPORT(NetTxQueueCreate)(
        nxAdapter->GetPublicGlobals(),
        QueueInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queueConfig,
        &txQueue);
}

_Use_decl_annotations_
NTSTATUS
DefaultCreateRxQueue(
    NETADAPTER Adapter,
    NETRXQUEUE_INIT * QueueInit
)
{
    NET_PACKET_QUEUE_CONFIG queueConfig;
    NET_PACKET_QUEUE_CONFIG_INIT(
        &queueConfig,
        DefaultRxPacketQueueAdvance,
        DefaultPacketQueueSetNotificationEnabled,
        DefaultRxPacketQueueCancel);

    auto nxAdapter = GetNxAdapterFromHandle(Adapter);

    NETPACKETQUEUE rxQueue;
    return NETEXPORT(NetRxQueueCreate)(
        nxAdapter->GetPublicGlobals(),
        QueueInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queueConfig,
        &rxQueue);
}

_Use_decl_annotations_
void
DefaultTxPacketQueueAdvance(
    NETPACKETQUEUE Queue
)
{
    auto txQueue = GetNxQueueFromHandle(Queue);
    auto rings = txQueue->GetRingCollection();

    // Complete any new transmit packets to avoid NDIS NBL leaking watchdog firing
    auto pr = NetRingCollectionGetPacketRing(rings);
    auto fr = NetRingCollectionGetFragmentRing(rings);

    pr->BeginIndex = pr->EndIndex;
    fr->BeginIndex = fr->EndIndex;
}

_Use_decl_annotations_
void
DefaultRxPacketQueueAdvance(
    NETPACKETQUEUE Queue
)
{
    // In the receive case the packets are owned by the client driver, no need to complete
    // them in the advance callback
    UNREFERENCED_PARAMETER(Queue);
}

_Use_decl_annotations_
void
DefaultPacketQueueSetNotificationEnabled(
    NETPACKETQUEUE Queue,
    BOOLEAN NotificationEnabled
)
{
    UNREFERENCED_PARAMETER((Queue, NotificationEnabled));
}

_Use_decl_annotations_
void
DefaultTxPacketQueueCancel(
    NETPACKETQUEUE Queue
)
{
    UNREFERENCED_PARAMETER(Queue);
}

_Use_decl_annotations_
void
DefaultRxPacketQueueCancel(
    NETPACKETQUEUE Queue
)
{
    auto rxQueue = GetNxQueueFromHandle(Queue);
    auto rings = rxQueue->GetRingCollection();

    auto pr = NetRingCollectionGetPacketRing(rings);
    auto fr = NetRingCollectionGetFragmentRing(rings);

    for (UINT32 i = pr->BeginIndex; i != pr->EndIndex; i = NetRingIncrementIndex(pr, i))
    {
        auto packet = NetRingGetPacketAtIndex(pr, i);
        packet->Ignore = 1;
    }

    pr->BeginIndex = pr->EndIndex;
    fr->BeginIndex = fr->EndIndex;
}
