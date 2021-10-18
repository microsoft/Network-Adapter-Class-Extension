// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements the NDIS NET_BUFFER_LIST datapath via pluggable handlers

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxNblDatapath.tmh"
#include "NxNblDatapath.hpp"

#include <ndis/nblsend.h>
#include <ndis/nblreceive.h>
#include <ndis/nblchain.h>
#include <ndis/oidrequest.h>

#ifndef _KERNEL_MODE

extern "C"
__declspec(dllimport)
VOID
NdisMSendNetBufferListsComplete(
    __in NDIS_HANDLE              MiniportAdapterHandle,
    __in PNET_BUFFER_LIST         NetBufferList,
    __in ULONG                    SendCompleteFlags
);

extern "C"
__declspec(dllimport)
VOID
NdisMIndicateReceiveNetBufferLists(
    __in  NDIS_HANDLE             MiniportAdapterHandle,
    __in  PNET_BUFFER_LIST        NetBufferLists,
    __in  NDIS_PORT_NUMBER        PortNumber,
    __in  ULONG                   NumberOfNetBufferLists,
    __in  ULONG                   ReceiveFlags
);

#define NDIS_STATUS_PAUSED ((NDIS_STATUS)STATUS_NDIS_PAUSED)

#endif // _KERNEL_MODE

NxNblDatapath::NxNblDatapath()
{
    // Initial state is closed.
    m_txRundown.CloseAndWait();
    m_rxRundown.CloseAndWait();
}

void
NxNblDatapath::CloseTxHandler()
{
    InterlockedExchange(&m_txClosed, true);
}

void
NxNblDatapath::SetTxHandler(_In_opt_ INxNblTx *tx)
{
    if (tx)
    {
        WIN_ASSERT(m_tx == nullptr);
        m_tx = tx;
        m_txRundown.Reinitialize();
        InterlockedExchange(&m_txClosed, false);
    }
    else
    {
        CloseTxHandler();
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
    auto numberOfNbls = NdisNumNblsInNblChain(nblChain);

    if (! ReadAcquire(&m_txClosed) && m_txRundown.TryAcquire(numberOfNbls))
    {
        m_tx->SendNetBufferLists(nblChain, portNumber, numberOfNbls, sendFlags);
    }
    else
    {
        NdisSetStatusInNblChain(nblChain, NDIS_STATUS_PAUSED);

        auto sendCompleteFlags = WI_IsFlagSet(sendFlags, NDIS_SEND_FLAGS_DISPATCH_LEVEL)
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
    WIN_ASSERT(numberOfNbls == NdisNumNblsInNblChain(nblChain));

    NdisMSendNetBufferListsComplete(m_ndisHandle, nblChain, sendCompleteFlags);
    m_txRundown.Release(numberOfNbls);
}

void
NxNblDatapath::CountTxStatistics(
    _In_ TxPacketCounters const & Counters
)
{
    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&m_counters.Tx.Unicast.Packets), Counters.Unicast.Packets);
    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&m_counters.Tx.Unicast.Bytes), Counters.Unicast.Bytes);
    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&m_counters.Tx.Multicast.Packets), Counters.Multicast.Packets);
    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&m_counters.Tx.Multicast.Bytes), Counters.Multicast.Bytes);
    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&m_counters.Tx.Broadcast.Packets), Counters.Broadcast.Packets);
    InterlockedAddNoFence64(
        reinterpret_cast<LONG64 *>(&m_counters.Tx.Broadcast.Bytes), Counters.Broadcast.Bytes);
}

_Must_inspect_result_
bool
NxNblDatapath::IndicateReceiveNetBufferLists(
    _In_ RxPacketCounters const & Counters,
    _In_ NET_BUFFER_LIST *nblChain,
    _In_ ULONG portNumber,
    _In_ ULONG numberOfNbls,
    _In_ ULONG receiveFlags)
{
    WIN_ASSERT(numberOfNbls == NdisNumNblsInNblChain(nblChain));
    NT_FRE_ASSERT(WI_IsFlagClear(receiveFlags, NDIS_RECEIVE_FLAGS_RESOURCES));

    if (m_rxRundown.TryAcquire(numberOfNbls))
    {

        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Unicast.Packets), Counters.Unicast.Packets);
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Unicast.Bytes), Counters.Unicast.Bytes);
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Multicast.Packets), Counters.Multicast.Packets);
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Multicast.Bytes), Counters.Multicast.Bytes);
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Broadcast.Packets), Counters.Broadcast.Packets);
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Broadcast.Bytes), Counters.Broadcast.Bytes);

        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Rsc.NblsCoalesced), Counters.Rsc.NblsCoalesced);
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Rsc.ScusGenerated), Counters.Rsc.ScusGenerated);
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Rsc.BytesCoalesced), Counters.Rsc.BytesCoalesced);
        InterlockedAddNoFence64(
            reinterpret_cast<LONG64 *>(&m_counters.Rx.Rsc.NblsAborted), Counters.Rsc.NblsAborted);

        NdisMIndicateReceiveNetBufferLists(m_ndisHandle, nblChain, portNumber, numberOfNbls, receiveFlags);

        return true;
    }
    else
    {
        return false;
    }
}

void
NxNblDatapath::ReturnNetBufferLists(
    _In_ NET_BUFFER_LIST *nblChain,
    _In_ ULONG receiveReturnFlags)
{
#if DBG
    auto actualNumberOfNbls = NdisNumNblsInNblChain(nblChain);
#endif

    auto numberOfNbls =
        m_rx->ReturnNetBufferLists(nblChain, receiveReturnFlags);

    NT_ASSERT(numberOfNbls == actualNumberOfNbls);

    m_rxRundown.Release(numberOfNbls);
}

NTSTATUS
NxNblDatapath::QueryStatisticsInfo(
    _In_ NDIS_OID_REQUEST & Request
)
{
    auto const length = Request.DATA.QUERY_INFORMATION.InformationBufferLength;
    auto const buffer = static_cast<UINT8 const *>(Request.DATA.QUERY_INFORMATION.InformationBuffer);

    Request.DATA.QUERY_INFORMATION.BytesNeeded = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;

    CX_RETURN_NTSTATUS_IF(
        STATUS_BUFFER_TOO_SMALL,
        length < NDIS_SIZEOF_STATISTICS_INFO_REVISION_1);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        ! (buffer + length > buffer));

    RtlZeroMemory(const_cast<UINT8 *>(buffer), length);
    Request.DATA.QUERY_INFORMATION.BytesWritten = length;

    auto statistics = static_cast<NDIS_STATISTICS_INFO *>(Request.DATA.QUERY_INFORMATION.InformationBuffer);
    statistics->Header = {
        NDIS_OBJECT_TYPE_DEFAULT,
        NDIS_STATISTICS_INFO_REVISION_1,
        NDIS_SIZEOF_STATISTICS_INFO_REVISION_1,
    };
    statistics->SupportedStatistics =
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS |
        NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR |
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR |
        NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS |
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV |
        NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT |
        NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT;

    statistics->ifHCInUcastPkts = Counters.Rx.Unicast.Packets;
    statistics->ifHCInUcastOctets = Counters.Rx.Unicast.Bytes;
    statistics->ifHCInMulticastPkts = Counters.Rx.Multicast.Packets;
    statistics->ifHCInMulticastOctets = Counters.Rx.Multicast.Bytes;
    statistics->ifHCInBroadcastPkts = Counters.Rx.Broadcast.Packets;
    statistics->ifHCInBroadcastOctets = Counters.Rx.Broadcast.Bytes;
    statistics->ifHCInOctets = statistics->ifHCInUcastOctets
        + statistics->ifHCInMulticastOctets + statistics->ifHCInBroadcastOctets;

    statistics->ifHCOutUcastPkts = Counters.Tx.Unicast.Packets;
    statistics->ifHCOutUcastOctets = Counters.Tx.Unicast.Bytes;
    statistics->ifHCOutMulticastPkts = Counters.Tx.Multicast.Packets;
    statistics->ifHCOutMulticastOctets = Counters.Tx.Multicast.Bytes;
    statistics->ifHCOutBroadcastPkts = Counters.Tx.Broadcast.Packets;
    statistics->ifHCOutBroadcastOctets = Counters.Tx.Broadcast.Bytes;
    statistics->ifHCOutOctets = statistics->ifHCOutUcastOctets
        + statistics->ifHCOutMulticastOctets + statistics->ifHCOutBroadcastOctets;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxNblDatapath::QueryRscStatisticsInfo(
    NDIS_OID_REQUEST & Request
) const
{
    auto const length = Request.DATA.QUERY_INFORMATION.InformationBufferLength;
    auto const buffer = static_cast<UINT8 *>(Request.DATA.QUERY_INFORMATION.InformationBuffer);

    Request.DATA.QUERY_INFORMATION.BytesNeeded = NDIS_SIZEOF_RSC_STATISTICS_REVISION_1;

    CX_RETURN_NTSTATUS_IF(
        STATUS_BUFFER_TOO_SMALL,
        length < NDIS_SIZEOF_RSC_STATISTICS_REVISION_1);

    CX_RETURN_NTSTATUS_IF(
        STATUS_BUFFER_OVERFLOW,
        ! (buffer + length > buffer));

    RtlZeroMemory(buffer, length);
    Request.DATA.QUERY_INFORMATION.BytesWritten = length;

    auto statistics = reinterpret_cast<NDIS_RSC_STATISTICS_INFO *>(buffer);
    statistics->Header = {
        NDIS_OBJECT_TYPE_DEFAULT,
        NDIS_RSC_STATISTICS_REVISION_1,
        NDIS_SIZEOF_RSC_STATISTICS_REVISION_1
    };
    statistics->CoalescedPkts = Counters.Rx.Rsc.NblsCoalesced;
    statistics->CoalesceEvents = Counters.Rx.Rsc.ScusGenerated;
    statistics->CoalescedOctets = Counters.Rx.Rsc.BytesCoalesced;
    statistics->Aborts = Counters.Rx.Rsc.NblsAborted;

    return STATUS_SUCCESS;
}
