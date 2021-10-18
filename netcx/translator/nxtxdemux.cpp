#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxTxDemux.tmh"
#include "NxTxDemux.hpp"

NxTxDemux::NxTxDemux(
    NxTxDemux::Type Type,
    size_t Range
) noexcept
    : m_type(Type)
    , m_range(Range)
{
}

NxTxDemux::Type
NxTxDemux::GetType(
    void
) const
{
    return m_type;
}

size_t
NxTxDemux::GetRange(
    void
) const
{
    return m_range;
}

