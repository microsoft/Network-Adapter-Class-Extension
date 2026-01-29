// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxNblQueue is a FIFO queues of NET_BUFFER_LISTs, with
    built-in synchronization.

--*/

#pragma once

#include <KSpinLock.h>
#include <ndis/nblqueue.h>
#include <ndis/nblreceive.h>
#include "RtlHomogenousSequence.h"

class NxNblQueue
{
public:

    PAGED NxNblQueue();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void Enqueue(_In_ PNET_BUFFER_LIST pNbl);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void Enqueue(_Inout_ NBL_COUNTED_QUEUE *queue);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG64 DequeueAll(_Out_ NBL_QUEUE *destination);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_BUFFER_LIST *DequeueAll();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ULONG64 GetNblQueueDepth() const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NET_BUFFER_LIST const * PeekNbl() const;

private:

    NBL_COUNTED_QUEUE m_nblQueue;
    KSpinLock m_SpinLock;
};

//
// Utility class that builds on top of a simple NBL_QUEUE.
//
// In addition to tracking how many NBLs are in the queue, this class also
// tracks additional data needed to populate
// NdisMIndicateReceiveNetBufferLists, including the length of the NBL chain
// and whether all the NBLs have the same EtherType.
//
class NxRxNblQueue
{

public:

    PAGED NxRxNblQueue() { NdisInitializeNblCountedQueue(&m_nblQueue); }

    ULONG GetReceiveFlags() const
    {
        ULONG receiveFlags = 0;

        //
        // Set NDIS_RECEIVE_FLAGS_SINGLE_ETHER_TYPE if and only if:
        // 1) NetBufferListFrameType field is one of the ETHERNET_TYPE_XXX values.
        //    For example, NetBufferListFrameType will be 0 for "pure" 802.2 LLC frame (when SNAP is NOT used)
        //    because there is no layer 3 payload. So we should NOT set NDIS_RECEIVE_FLAGS_SINGLE_ETHER_TYPE.
        // AND
        // 2) Obviously, NetBufferListFrameType fields for all appended frames should be the same
        //
        // No matter we have single or multiple frames, as long as only 0 is added to RtlHomogenousSequence,
        // it is always UNCERTAIN if frames have single EtherType.
        //
        if (m_frameTypes.GetSetBits() != 0U && m_frameTypes.IsHomogenous())
        {
            receiveFlags |= NDIS_RECEIVE_FLAGS_SINGLE_ETHER_TYPE;
        }

        return receiveFlags;
    }

    void AppendNblChain(
        _In_ NET_BUFFER_LIST *first,
        _In_ NET_BUFFER_LIST **tailPointer,
        _In_ size_t numNbls,
        _In_ RtlHomogenousSequence<USHORT>& frameType)
    {
        m_frameTypes.Combine(frameType);

        NdisAppendNblChainWithTailPointerToNblCountedQueueFast(
            &m_nblQueue,
            first,
            tailPointer,
            numNbls);
    }

    ULONG GetCount() const { return (ULONG) m_nblQueue.NblCount; }

    NET_BUFFER_LIST* GetNbls() const { return NdisGetNblChainFromNblCountedQueue(&m_nblQueue); }

    NBL_QUEUE* GetNblQueue() { return &m_nblQueue.Queue; }

    void Reset()
    {
        NdisInitializeNblCountedQueue(&m_nblQueue);
        m_frameTypes.Reset();
    }

private:

    NBL_COUNTED_QUEUE m_nblQueue;
    RtlHomogenousSequence<USHORT> m_frameTypes;
};
