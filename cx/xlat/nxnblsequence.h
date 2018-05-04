#pragma once

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
        ndisInitializeNblQueue(&m_queue);
    }

    void AddNbl(_In_ NET_BUFFER_LIST *nbl)
    {
        WIN_ASSERT(nbl->Next == nullptr);

        m_frameTypes.AddValue((USHORT)nbl->NetBufferListInfo[NetBufferListFrameType]);

        m_count += 1;
        ndisAppendNblChainToNblQueueFast(&m_queue, nbl, nbl);
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

private:

    ULONG m_count = 0;
    RtlHomogenousSequence<USHORT> m_frameTypes;

    NBL_QUEUE m_queue;
};

