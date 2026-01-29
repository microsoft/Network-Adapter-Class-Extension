// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxNblDatapath class serves two purposes:

        - Allow handlers for the NDIS6 NET_BUFFER_LIST datapath to come and go.
          While no handler is registered, the NxNblDatapath bounces NBLs.

        - Synchronize deregistration of handlers, so a handler cannot be
          deregistered while there are outstanding NBLs.

Notes:

    A datapath handler for the Tx (transmit) path implements INxNblTx, then
    calls SetTxHandler to register himself.  He will immediately begin getting
    calls to his SendNetBufferLists handler.

    To deregister, the handler calls SetTxHandler(nullptr).  This will block
    until all in-flight NBLs have been returned to NDIS.  Any future attempts
    to send an NBL will be immediately returned.

    Rx is similar.  However, there is an additional restriction: the Rx handler
    should not attempt to indicate a new received NBL after calling
    SetRxHandler(nullptr).

Example:

                NDIS                NxNblDatapath           Tx handler
                  |                      |
                  |                      |
                  |--------Send NBL----->|
                  |                      |
                  |                      |A Tx handler is not
                  |                      |registered yet, so
                  |                      |we just bounce the NBL.
                  |                      |                      .
                  |<--SendComplete NBL---|                      |
                  |                      |<--SetTxHandler(this)-|
                  |                      |                      |
                  |--------Send NBL----->|                      |
                  |                      |--INxNblTx::Send NBL->|
                  |                      |                      |
                  |                      |                      |
                  |                      |<--SendComplete NBL---|
                  |<--SendComplete NBL---|                      |
                  |                      |                      |

                                  ( time passes)

                  |                      |                      |
                  |                      |<--SetTxHandler(NULL)-|
                  |                      |                      |
                  |                      |Block until last NBL  |
                  |                      |to be returned...     |
                  |                      |                      |
                  |                      |<--SendComplete NBL---|
                  |                      |   (the last NBL)     |
                  |<--SendComplete NBL---|                      |
                  |                      |--SetTxHandler done-->|
                  |                      |                      |

--*/

#pragma once

#include "NxNbl.hpp"
#include <KRundown.h>

class NxNblDatapath : public INxNblDispatcher
{
public:

    NxNblDatapath();

    /// Must be called during initialization to set the NDIS handle to use
    /// in the NDIS datapath APIs.
    void SetNdisHandle(NDIS_HANDLE handle) { m_ndisHandle = handle; }

    //
    // INxNblDispatcher
    //
    virtual void SetTxHandler(_In_opt_ INxNblTx *tx);
    virtual void SetRxHandler(_In_opt_ INxNblRx *rx);
    void CloseTxHandler(void);

    virtual void SendNetBufferLists(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG portNumber,
        _In_ ULONG sendFlags);

    virtual void SendNetBufferListsComplete(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG numberOfNbls,
        _In_ ULONG sendCompleteFlags);

    virtual void CountTxStatistics(
        _In_ TxPacketCounters const & Counters);

    _Must_inspect_result_
    virtual bool IndicateReceiveNetBufferLists(
        _In_ RxPacketCounters const & Counters,
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG portNumber,
        _In_ ULONG numberOfNbls,
        _In_ ULONG receiveFlags);

    virtual void ReturnNetBufferLists(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG receiveCompleteFlags);

    NTSTATUS QueryStatisticsInfo(_In_ NDIS_OID_REQUEST & Request) override;

    NTSTATUS QueryRscStatisticsInfo(_In_ NDIS_OID_REQUEST & Request) const override;

private:

    NDIS_HANDLE m_ndisHandle = 0;

    KRundown m_txRundown;
    KRundown m_rxRundown;

    _Interlocked_
    LONG m_txClosed = true;
    INxNblTx *m_tx = nullptr;
    INxNblRx *m_rx = nullptr;

    struct DatapathCounters
    {

        TxPacketCounters
            Tx;

        RxPacketCounters
            Rx;

    } m_counters = {};

public:

    DatapathCounters const & Counters = m_counters;

};
