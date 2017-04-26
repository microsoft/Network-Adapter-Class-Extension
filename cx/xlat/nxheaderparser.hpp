// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
class NxPacketHeaderParser
{
    NET_PACKET const &m_packet;
    NET_PACKET_FRAGMENT const *m_currentFragment;

    ULONG m_fragmentOffset = 0;

public:

    NxPacketHeaderParser(NET_PACKET const &packet) : m_packet(packet), m_currentFragment(&m_packet.Data)
    {

    }

    PVOID TryGetContiguousBuffer(ULONG bufferSize);
    void Retreat(ULONG retreatCount);

    template<typename THeader>
    THeader UNALIGNED *TryTakeHeader(ULONG headerSize = sizeof(THeader))
    {
        return (THeader UNALIGNED *)TryGetContiguousBuffer(headerSize);
    }
};
