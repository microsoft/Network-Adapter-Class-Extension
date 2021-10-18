#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include <ndis/nbl8021q.h>

#include "TxWmmInfoDemux.tmh"
#include "TxWmmInfoDemux.hpp"

static
UINT8
ParseWmmInfo(
    NET_BUFFER_LIST const * NetBufferList
)
{
    auto const & info = *reinterpret_cast<NDIS_NET_BUFFER_LIST_8021Q_INFO const *>(
        &NetBufferList->NetBufferListInfo[Ieee8021QNetBufferListInfo]);

    return info.WLanTagHeader.WMMInfo;
}

TxWmmInfoDemux::TxWmmInfoDemux(
    size_t Range
) noexcept
    : NxTxDemux(NxTxDemux::Type::WmmInfo, Range)
{
}

_Use_decl_annotations_
size_t
TxWmmInfoDemux::Demux(
    NET_BUFFER_LIST const * NetBufferList
) const
{
    return ParseWmmInfo(NetBufferList);
}

_Use_decl_annotations_
NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY
TxWmmInfoDemux::GenerateDemuxProperty(
    NET_BUFFER_LIST const * NetBufferList
)
{
    auto const priority = ParseWmmInfo(NetBufferList);
    NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY property = {
        TxDemuxTypeWmmInfo,
        Demux(priority),
    };
    property.Property.WmmInfo = priority;

    return wistd::move(property);
}

size_t
TxWmmInfoDemux::Demux(
    _In_ UINT8 Value
) const
{
    return Value;
}
