// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <NetClientTypes.h>

class NxTxDemux
    : public NxNonpagedAllocation<'TxtN'>
{

public:

    enum class Type
    {
        UserPriority = 1,
        Peer,
        WmmInfo,
    };

    NxTxDemux(
        NxTxDemux::Type,
        size_t Range
    ) noexcept;

    NxTxDemux::Type
    GetType(
        void
    ) const;

    size_t
    GetRange(
        void
    ) const;

    virtual
    NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY
    GenerateDemuxProperty(
        _In_ NET_BUFFER_LIST const * NetBufferList
    ) = 0;

private:

    NxTxDemux::Type const
        m_type;

    size_t const
        m_range = SIZE_T_MAX;
};

