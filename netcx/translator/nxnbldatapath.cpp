// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements the NDIS NET_BUFFER_LIST datapath via pluggable handlers

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxNblDatapath.tmh"
#include "NxNblDatapath.hpp"
#include <nblutil.h>

NxNblDatapath::NxNblDatapath()
{
    // Initial state is closed.
    m_txRundown.CloseAndWait();
    m_rxRundown.CloseAndWait();
}

void
NxNblDatapath::SetTxHandler(_In_opt_ INxNblTx *tx)
{
    if (tx)
    {
        WIN_ASSERT(m_tx == nullptr);
        m_tx = tx;
        m_txRundown.Reinitialize();
    }
    else
    {
        m_txRundown.CloseAndWait();
        m_tx = nullptr;
    }
}

void
NxNblDatapath::SetRxHandler(_In_opt_ INxNblRx *rx)
{
    if (rx)
    {
        WIN_ASSERT(m_rx == nullptr);
        m_rx = rx;
        m_rxRundown.Reinitialize();
    }
    else
    {
        m_rxRundown.CloseAndWait();
        m_rx = nullptr;
    }
}

void
NxNblDatapath::SendNetBufferLists(
    _In_ NET_BUFFER_LIST *nblChain,
    _In_ ULONG portNumber,
    _In_ ULONG sendFlags)
{
    auto numberOfNbls = ndisNumNblsInNblChain(nblChain);

    if (m_txRundown.TryAcquire(numberOfNbls))
    {
        m_tx->SendNetBufferLists(nblChain, portNumber, numberOfNbls, sendFlags);
    }
    else
    {
        ndisSetStatusInNblChain(nblChain, NDIS_STATUS_PAUSED);

        auto sendCompleteFlags = NDIS_TEST_SEND_AT_DISPATCH_LEVEL(sendFlags)
            ? NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL
            : 0;

        NdisMSendNetBufferListsComplete(m_ndisHandle, nblChain, sendCompleteFlags);
    }
}

void
NxNblDatapath::SendNetBufferListsComplete(
    _In_ NET_BUFFER_LIST *nblChain,
    _In_ ULONG numberOfNbls,
    _In_ ULONG sendCompleteFlags)
{
    WIN_ASSERT(numberOfNbls == ndisNumNblsInNblChain(nblChain));

    NdisMSendNetBufferListsComplete(m_ndisHandle, nblChain, sendCompleteFlags);
    m_txRundown.Release(numberOfNbls);
}

_Must_inspect_result_
bool
NxNblDatapath::IndicateReceiveNetBufferLists(
    _In_ NET_BUFFER_LIST *nblChain,
    _In_ ULONG portNumber,
    _In_ ULONG numberOfNbls,
    _In_ ULONG receiveFlags)
{
    WIN_ASSERT(numberOfNbls == ndisNumNblsInNblChain(nblChain));

    if (m_rxRundown.TryAcquire(numberOfNbls))
    {
        NdisMIndicateReceiveNetBufferLists(m_ndisHandle, nblChain, portNumber, numberOfNbls, receiveFlags);

        if (NDIS_TEST_RECEIVE_CANNOT_PEND(receiveFlags))
        {
            m_rxRundown.Release(numberOfNbls);
        }
    }
    else if (NDIS_TEST_RECEIVE_CAN_PEND(receiveFlags))
    {
        return false;
    }

    return true;
}

void
NxNblDatapath::ReturnNetBufferLists(
    _In_ NET_BUFFER_LIST *nblChain,
    _In_ ULONG receiveReturnFlags)
{
#if DBG
    auto actualNumberOfNbls = ndisNumNblsInNblChain(nblChain);
#endif

    auto numberOfNbls =
        m_rx->ReturnNetBufferLists(nblChain, receiveReturnFlags);

    NT_ASSERT(numberOfNbls == actualNumberOfNbls);

    m_rxRundown.Release(numberOfNbls);
}

