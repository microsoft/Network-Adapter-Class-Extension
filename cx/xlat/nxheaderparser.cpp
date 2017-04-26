// Copyright (C) Microsoft Corporation. All rights reserved.
#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxHeaderParser.tmh"

#include "NxHeaderParser.hpp"

PVOID
NxPacketHeaderParser::TryGetContiguousBuffer(ULONG bufferSize)
{
    while (m_currentFragment && m_fragmentOffset == m_currentFragment->ValidLength)
    {
        if (m_currentFragment->LastFragmentOfFrame)
        {
            return nullptr;
        }

        m_fragmentOffset = 0;
        m_currentFragment = NET_PACKET_FRAGMENT_GET_NEXT(m_currentFragment);
    }

    if (!m_currentFragment)
    {
        return nullptr;
    }

    if (bufferSize > m_currentFragment->ValidLength - m_fragmentOffset)
    {
        return nullptr;
    }

    auto const va =
        (UINT8*)m_currentFragment->VirtualAddress +
        m_currentFragment->Offset +
        m_fragmentOffset;

    m_fragmentOffset += bufferSize;

    return va;
}

void
NxPacketHeaderParser::Retreat(ULONG byteCount)
{
    NT_ASSERT(byteCount != 0);

    while (true)
    {
        auto const retreatBy = min(byteCount, m_fragmentOffset);

        m_fragmentOffset -= retreatBy;
        byteCount -= retreatBy;

        if (byteCount == 0)
        {
            break;
        }

        for (auto prev = &m_packet.Data; prev; prev = NET_PACKET_FRAGMENT_GET_NEXT(prev))
        {
            NT_ASSERT(prev != m_currentFragment);

            if (NET_PACKET_FRAGMENT_GET_NEXT(prev) == m_currentFragment)
            {
                m_currentFragment = prev;
                m_fragmentOffset = prev->ValidLength;
            }
        }
    }
}
