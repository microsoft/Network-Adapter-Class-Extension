#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "TxPeerAddressDemux.tmh"
#include "TxPeerAddressDemux.hpp"

#pragma warning(push)
#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union
#include <80211hdr.h>
#pragma warning(pop)

#include "NxTranslationApp.hpp"

static
NET_CLIENT_EUI48_ADDRESS const *
ParsePeerAddress(
    NET_BUFFER_LIST const * NetBufferList
)
{
    auto const nb = NetBufferList->FirstNetBuffer;
    auto const mdl = nb->CurrentMdl;
    auto const size = MmGetMdlByteCount(mdl) - nb->CurrentMdlOffset;
    auto const pointer = static_cast<UINT8 const *>(
        MmGetSystemAddressForMdlSafe(mdl, LowPagePriority | MdlMappingNoExecute));
    auto const header = reinterpret_cast<DOT11_MAC_HEADER const *>(&pointer[nb->CurrentMdlOffset]);

    if (size >= sizeof(*header))
    {
        return reinterpret_cast<NET_CLIENT_EUI48_ADDRESS const *>(header->Address1);
    }

    return nullptr;
}

TxPeerAddressDemux::TxPeerAddressDemux(
    size_t Range,
    NxTranslationApp const & App
) noexcept
    : NxTxDemux(NxTxDemux::Type::Peer, Range)
    , m_app(App)
{
}

_Use_decl_annotations_
size_t
TxPeerAddressDemux::Demux(
    NET_BUFFER_LIST const * NetBufferList
) const
{
    auto const peerAddress = ParsePeerAddress(NetBufferList);

    if (! peerAddress)
    {
        return SIZE_T_MAX;
    }

    return Demux(peerAddress);
}

_Use_decl_annotations_
NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY
TxPeerAddressDemux::GenerateDemuxProperty(
    NET_BUFFER_LIST const * NetBufferList
)
{
    auto const address = ParsePeerAddress(NetBufferList);
    NET_CLIENT_QUEUE_TX_DEMUX_PROPERTY property = {
        TxDemuxTypePeerAddress,
        reinterpret_cast<SIZE_T>(NetBufferList->Scratch),
    };

    NT_FRE_ASSERT(property.Value <= GetRange());

    // as a matter of presentation we generate broadcast address for
    // any queue that has the multicast bit set instead of using the
    // multicast address that triggered the generation.

    if (address->Value[0] & 0x01)
    {
        property.Property.PeerAddress = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    }
    else
    {
        property.Property.PeerAddress = *address;
    }

    return wistd::move(property);
}

size_t
TxPeerAddressDemux::Demux(
    NET_CLIENT_EUI48_ADDRESS const * Address
) const
{
    if (Address->Value[0] & 0x01)
    {
        return 0U;
    }

    // lookup failure returns a special value of ~0U so if
    // the lookup fails do not overflow the value back to 0U
    // which would return the demux for the broadcast queue.

    auto const demux = m_app.WifiTxPeerAddressDemux(Address);

    return demux == SIZE_T_MAX ? SIZE_T_MAX : demux + 1;
}
