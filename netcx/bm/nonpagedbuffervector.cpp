// Copyright (C) Microsoft Corporation. All rights reserved.
#include "BmPrecomp.hpp"

#include "NonPagedBufferVector.hpp"
#include "NonPagedBufferVector.tmh"

NonPagedBufferVector::NonPagedBufferVector(
    _In_ size_t BufferCount,
    _In_ size_t BufferSize,
    _In_ size_t BufferOffset
)
    : IBufferVector(BufferCount, BufferSize, BufferOffset)
{
}

NonPagedBufferVector::~NonPagedBufferVector(
    void
)
{
    for (size_t i = 0; i < m_BufferCount; i++)
    {
        if (m_NonPagedBufferVector[i].VirtualAddress)
        {
            ExFreePool(m_NonPagedBufferVector[i].VirtualAddress);
            m_NonPagedBufferVector[i].VirtualAddress = nullptr;
        }

        if (m_NonPagedBufferVector[i].Mdl)
        {
            ExFreePool(m_NonPagedBufferVector[i].Mdl);
            m_NonPagedBufferVector[i].Mdl = nullptr;
        }
    }
}

NONPAGED
_Use_decl_annotations_
void
NonPagedBufferVector::GetEntryByIndex(
    size_t Index,
    NET_DATA_HEADER * NetDataHeader
) const
{
    NetDataHeader->VirtualAddress = m_NonPagedBufferVector[Index].VirtualAddress;
    NetDataHeader->LogicalAddress = 0ULL;
    NetDataHeader->Mdl = m_NonPagedBufferVector[Index].Mdl;
}
