#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxScatterGatherList.tmh"

#include "NxScatterGatherList.hpp"

NxScatterGatherList::NxScatterGatherList(
    _In_ NxDmaAdapter const &DmaAdapter
) :
    m_dmaAdapter(DmaAdapter)
{

}

NxScatterGatherList::~NxScatterGatherList(
    void
)
{
    if (m_scatterGatherList != nullptr)
    {
        m_dmaAdapter.PutScatterGatherList(m_scatterGatherList);
        m_scatterGatherList = nullptr;
    }
}

SCATTER_GATHER_LIST *
NxScatterGatherList::release(
    void
)
{
    auto sgl = m_scatterGatherList;
    m_scatterGatherList = nullptr;
    return sgl;
}

SCATTER_GATHER_LIST *
NxScatterGatherList::get(
    void
) const
{
    return m_scatterGatherList;
}

SCATTER_GATHER_LIST ** const
NxScatterGatherList::releaseAndGetAddressOf(
    void
)
{
    auto sgl = release();
    if (sgl != nullptr)
    {
        m_dmaAdapter.PutScatterGatherList(sgl);
    }

    return &m_scatterGatherList;
}

SCATTER_GATHER_LIST const *
NxScatterGatherList::operator->(
    void
) const
{
    return m_scatterGatherList;
}

SCATTER_GATHER_LIST const &
NxScatterGatherList::operator*(
    void
) const
{
    return *m_scatterGatherList;
}
