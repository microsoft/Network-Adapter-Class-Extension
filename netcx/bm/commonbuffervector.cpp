// Copyright (C) Microsoft Corporation. All rights reserved.
#include "BmPrecomp.hpp"

#include "CommonBufferVector.hpp"
#include "CommonBufferVector.tmh"

_Use_decl_annotations_
CommonBufferVector::CommonBufferVector(
    DMA_ADAPTER * DmaAdapter,
    size_t BufferCount,
    size_t BufferSize,
    size_t BufferOffset
)
    : IBufferVector(BufferCount, BufferSize, BufferOffset)
    , m_DmaAdapter(DmaAdapter)
{
}

CommonBufferVector::~CommonBufferVector(
    void
)
{
    // Free the common buffer vector if one was allocated
    if (m_CommonBufferVector != nullptr)
    {
        m_DmaAdapter->DmaOperations->FreeCommonBufferVector(m_DmaAdapter, m_CommonBufferVector);
        m_CommonBufferVector = nullptr;
    }
}

NONPAGED
_Use_decl_annotations_
void
CommonBufferVector::GetEntryByIndex(
    size_t Index,
    NET_DATA_HEADER * NetDataHeader
) const
{
    m_DmaAdapter->DmaOperations->GetCommonBufferFromVectorByIndex(
        m_DmaAdapter,
        m_CommonBufferVector,
        static_cast<ULONG>(Index),
        &NetDataHeader->VirtualAddress,
        reinterpret_cast<PHYSICAL_ADDRESS *>(&NetDataHeader->LogicalAddress));

    NetDataHeader->Mdl = nullptr;
}
