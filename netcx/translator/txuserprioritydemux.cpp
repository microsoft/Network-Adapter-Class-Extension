#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include <ndis/nbl8021q.h>

#include "TxUserPriorityDemux.tmh"
#include "TxUserPriorityDemux.hpp"

static
UINT8
ParseUserPriority(
    NET_BUFFER_LIST const * NetBufferList
)
{
    auto const & info = *reinterpret_cast<NDIS_NET_BUFFER_LIST_8021Q_INFO const *>(
        &NetBufferList->NetBufferListInfo[Ieee8021QNetBufferListInfo]);

    return info.TagHeader.UserPriority;
}

TxUserPriorityDemux::TxUserPriorityDemux(
    size_t Range
) noexcept
    : NxTxDemux(NxTxDemux::Type::UserPriority, Range)
{
}

_Use_decl_annotations_
size_t
TxUserPriorityDemux::Demux(
    NET_BUFFER_LIST const * NetBufferList
) const
{
    return ParseUserPriority(NetBufferList);
}

_Use_decl_annotations_
NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY
TxUserPriorityDemux::GenerateDemuxProperty(
    NET_BUFFER_LIST const * NetBufferList
)
{
    auto const priority = ParseUserPriority(NetBufferList);
    NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY property = {
        TxDemuxType8021p,
        Demux(priority),
    };
    property.Property.UserPriority = priority;

    return wistd::move(property);
}

size_t
TxUserPriorityDemux::Demux(
    UINT8 Value
) const
{
    return Value;
}

