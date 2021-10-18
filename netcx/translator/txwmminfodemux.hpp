// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxTxDemux.hpp"

class TxWmmInfoDemux
    : public NxTxDemux
{

public:

    TxWmmInfoDemux(
        size_t Range
    ) noexcept;

    size_t
    Demux(
        _In_ NET_BUFFER_LIST const * NetBufferList
    ) const;

    NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY
    GenerateDemuxProperty(
        _In_ NET_BUFFER_LIST const * NetBufferList
    ) override;

private:

    size_t
    Demux(
        _In_ UINT8 Value
    ) const;

};

