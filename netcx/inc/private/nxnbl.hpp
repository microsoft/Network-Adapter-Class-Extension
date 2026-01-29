// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Defines an abstract interface for an NDIS6 NET_BUFFER_LIST datapath and
    generic handlers that register to implement the datapath.

Notes:

    These abstract interfaces exist to improve the modularity & testability of
    NetAdapterCx.sys.  Thus the "xlat" library doesn't need to know anything
    about NDIS APIs, while the "sys" directory doesn't need to implement any
    per-NBL logic.

    Because the production code only has a single concrete implementation of
    each of these interfaces, we expect the optimized code to eliminate the
    virtual dispatch table.

--*/

#pragma once

#include <KMacros.h>
#include <KRundown.h>

/// Implemented by a subsystem that can implement the NET_BUFFER_LIST send path
struct INxNblTx
{
    /// Invoked when NDIS calls into MiniportSendNetBufferLists
    virtual void SendNetBufferLists(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG portNumber,
        _In_ ULONG numberOfNbls,
        _In_ ULONG sendFlags) = 0;
};

/// Implemented by a subsystem that can implement the NET_BUFFER_LIST receive path
struct INxNblRx
{
    /// Invoked when NDIS calls into MiniportReturnNetBufferList
    virtual ULONG ReturnNetBufferLists(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG receiveCompleteFlags) = 0;
};

struct PacketCounter
{

    UINT64
        Packets;

    UINT64
        Bytes;

};

struct PacketCounters
{

    PacketCounter
        Unicast;

    PacketCounter
        Multicast;

    PacketCounter
        Broadcast;

    UINT64
        Errors;

};

using TxPacketCounters = PacketCounters;

// Match the stats counters defined in NDIS_RSC_STATISTICS_INFO
struct NblRscStatistics
{
    // NDIS_RSC_STATISTICS_INFO.CoalescedPkts
    UINT64 NblsCoalesced = 0;

    // NDIS_RSC_STATISTICS_INFO.Aborts
    UINT64 NblsAborted = 0;

    // NDIS_RSC_STATISTICS_INFO.CoalesceEvents
    UINT64 ScusGenerated = 0;

    // NDIS_RSC_STATISTICS_INFO.CoalescedOctets
    UINT64 BytesCoalesced = 0;
};

struct RxPacketCounters : PacketCounters
{
    NblRscStatistics
        Rsc = {};
};

struct INxNblDispatcher
{
    virtual void CloseTxHandler(void) = 0;

    /// Registers or deregisters a Tx handler with the dispatcher
    virtual void SetTxHandler(_In_opt_ INxNblTx *tx) = 0;

    /// Registers or deregisters an Rx handler with the dispatcher
    virtual void SetRxHandler(_In_opt_ INxNblRx *rx) = 0;

    /// Invoked by the miniport driver to implement the NDIS
    /// MiniportSendNetBufferLists callback
    virtual void SendNetBufferLists(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG portNumber,
        _In_ ULONG sendFlags) = 0;

    /// Invoked by the INxNblTx handler when it has finished sending
    /// a NET_BUFFER_LIST chain
    virtual void SendNetBufferListsComplete(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG numberOfNbls,
        _In_ ULONG sendCompleteFlags) = 0;

    virtual void CountTxStatistics(
        _In_ TxPacketCounters const & Counters) = 0;

    /// Invoked by the INxNblRx handler when it would like to indicate
    /// some NBLs that were received.
    /// <return>true if the NBLs were accepted by the the datapath</return>
    _Must_inspect_result_
    virtual bool IndicateReceiveNetBufferLists(
        _In_ RxPacketCounters const & Counters,
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG portNumber,
        _In_ ULONG numberOfNbls,
        _In_ ULONG receiveFlags) = 0;

    /// Invoked by the miniport driver to implement the NDIS
    /// MiniportReturnNetBufferLists callback
    virtual void ReturnNetBufferLists(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG receiveCompleteFlags) = 0;

    virtual NTSTATUS QueryStatisticsInfo(
        _In_ NDIS_OID_REQUEST & Request) = 0;

    virtual NTSTATUS QueryRscStatisticsInfo(
        _In_ NDIS_OID_REQUEST & Request) const = 0;
};

