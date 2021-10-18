// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxTxDemux.hpp"

class NxTranslationApp;

class TxPeerAddressDemux
    : public NxTxDemux
{

public:

    TxPeerAddressDemux(
        size_t Range,
        NxTranslationApp const & App
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

    NxTranslationApp const &
        m_app;

    size_t
    Demux(
        _In_ NET_CLIENT_EUI48_ADDRESS const * Address
    ) const;

};

