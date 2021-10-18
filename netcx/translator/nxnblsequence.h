#pragma once

#include <ndis/nblqueue.h>
#include <ndis/nblreceive.h>
#include "RtlHomogenousSequence.h"

// Utility class that builds on top of a simple NBL_QUEUE.
//
// In addition to tracking how many NBLs are in the queue, this class also
// tracks additional data needed to populate
// NdisMIndicateReceiveNetBufferLists, including the length of the NBL chain
// and whether all the NBLs have the same EtherType.

class NxNblSequence
{
public:

    NxNblSequence()
    {
        NdisInitializeNblQueue(&m_queue);
    }

    void AddNbl(_In_ NET_BUFFER_LIST *nbl)
    {
        WIN_ASSERT(nbl->Next == nullptr);

        m_frameTypes.AddValue((USHORT)nbl->NetBufferListInfo[NetBufferListFrameType]);

        m_count += 1;
        NdisAppendNblChainToNblQueueFast(&m_queue, nbl, nbl);
    }

    operator bool() const
    {
        return m_count > 0;
    }

    NBL_QUEUE &GetNblQueue()
    {
        return m_queue;
    }

    ULONG GetCount() const
    {
        return m_count;
    }

    ULONG GetReceiveFlags() const
    {
        ULONG receiveFlags = 0;

        if (m_frameTypes.IsHomogenous())
            receiveFlags |= NDIS_RECEIVE_FLAGS_SINGLE_ETHER_TYPE;

        return receiveFlags;
    }

    // Calling PopAllNblsFromNblQueue() + PushNblsToNblQueue() will temporarily
    // get the NBL chain out of NBL_QUEUE, perform coalescing,
    // and then put NBL chain back to NBL_QUEUE.
    // Frame type of coalesced NBLs is not changed, so m_frameTypes remain unchanged.
    NET_BUFFER_LIST *
    PopAllNblsFromNblQueue(
        void
    )
    {
        NET_BUFFER_LIST * nblChain = NdisPopAllFromNblQueue(&m_queue);
        m_count = 0U;

        return nblChain;
    }

    void
    PushNblsToNblQueue(
        _In_ NET_BUFFER_LIST * NblChainHead,
        _In_ NET_BUFFER_LIST * NblChainTail,
        _In_ ULONG NblCount
    )
    {
        NdisAppendNblChainToNblQueueFast(&GetNblQueue(), NblChainHead, NblChainTail);
        m_count += NblCount;
    }

private:

    ULONG m_count = 0;
    RtlHomogenousSequence<USHORT> m_frameTypes;

    NBL_QUEUE m_queue;
};

