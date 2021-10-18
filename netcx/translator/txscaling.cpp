// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "TxScaling.tmh"
#include "TxScaling.hpp"

#include "NxTranslationApp.hpp"

_Use_decl_annotations_
TxScaling::TxScaling(
    NxTranslationApp & App,
    INxNblDispatcher * NblDispatcher,
    Rtl::KArray<wistd::unique_ptr<NxTxXlat>, NonPagedPoolNx> const & Queues
) noexcept
    : m_app(App)
    , m_nblDispatcher(NblDispatcher)
    , m_queues(Queues)
{
}

_Use_decl_annotations_
NTSTATUS
TxScaling::Initialize(
    NET_CLIENT_ADAPTER_TX_DEMUX_CONFIGURATION const & Configuration
)
{
    Rtl::KArray<wistd::unique_ptr<NxTxDemux>, NonPagedPoolNx> demux;

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! demux.reserve(Configuration.Count));

    for (size_t i = 0U; i < Configuration.Count; i++)
    {
        auto const & requirement = Configuration.Demux[i];

        auto range = requirement.Range;
        switch (static_cast<NxTxDemux::Type>(requirement.Type))
        {

        case NxTxDemux::Type::UserPriority:
            NT_FRE_ASSERT(demux.append(wil::make_unique_nothrow<TxUserPriorityDemux>(range)));
            break;

        case NxTxDemux::Type::Peer:
            // range for peer address demultiplexing is expanded for broadcast/multicast
            range += 1;
            NT_FRE_ASSERT(demux.append(wil::make_unique_nothrow<TxPeerAddressDemux>(range, m_app)));
            break;

        case NxTxDemux::Type::WmmInfo:
            NT_FRE_ASSERT(demux.append(wil::make_unique_nothrow<TxWmmInfoDemux>(range)));
            break;

        }

        m_numberOfQueues *= range;

        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! demux[demux.count() - 1]);
    }

    m_demux = wistd::move(demux);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
size_t
TxScaling::GetNumberOfQueues(
    void
) const
{
    return m_numberOfQueues;
}

_Use_decl_annotations_
void
TxScaling::GenerateDemuxProperties(
    NET_BUFFER_LIST const * NetBufferList,
    Rtl::KArray<NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY> & Properties
) const
{
    Properties.clear();
    for (auto const & demux : m_demux)
    {
        NT_FRE_ASSERT(Properties.append(demux->GenerateDemuxProperty(NetBufferList)));
    }
}

_Use_decl_annotations_
void
TxScaling::SendNetBufferLists(
    NET_BUFFER_LIST * NetBufferList,
    ULONG PortNumber,
    ULONG NblCount,
    ULONG SendFlags
)
{
    UNREFERENCED_PARAMETER(PortNumber);
    UNREFERENCED_PARAMETER(NblCount);
    UNREFERENCED_PARAMETER(SendFlags);

    NT_FRE_ASSERT(NetBufferList != NULL);

    // optimize later, don't break chains per-demux
    while (NetBufferList)
    {
        auto nbl = NetBufferList;

        // When using peer address demuxing we store the peer ID in the NBL's scratch field, so initialize
        // it with SIZE_T_MAX, the value used to represent "invalid peer"
        static_assert(sizeof(SIZE_T) <= FIELD_SIZE(NET_BUFFER_LIST, Scratch));
        nbl->Scratch = reinterpret_cast<void *>(SIZE_T_MAX);

        size_t queueId = 0U;
        bool drop = false;
        for (size_t i = 0; i < m_demux.count(); i++)
        {
            auto const & demux = m_demux[i];
            size_t value = 0;

            queueId *= demux->GetRange();
            switch (demux->GetType())
            {

            case NxTxDemux::Type::UserPriority:
                queueId += static_cast<TxUserPriorityDemux const *>(demux.get())->Demux(nbl);
                break;

            case NxTxDemux::Type::WmmInfo:
                queueId += static_cast<TxWmmInfoDemux const *>(demux.get())->Demux(nbl);
                break;

            case NxTxDemux::Type::Peer:
                value = static_cast<TxPeerAddressDemux const *>(demux.get())->Demux(nbl);
                nbl->Scratch = reinterpret_cast<void *>(value);
                drop = value >= demux->GetRange();
                queueId += value;
                break;

            }

            if (drop)
            {
                queueId = 0U;
                break;
            }
        }

        NetBufferList = NetBufferList->Next;
        nbl->Next = NULL;

        if (drop)
        {
            m_nblDispatcher->SendNetBufferListsComplete(nbl, 1, 0);
        }
        else
        {
            m_queues[queueId]->QueueNetBufferList(nbl);
        }
    }
}

