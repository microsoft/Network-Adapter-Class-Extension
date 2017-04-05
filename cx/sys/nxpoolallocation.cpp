/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxPoolAllocation.cpp

Abstract:

    Used for pool packet buffers allocated for Rx datapath

--*/

#include "Nx.hpp"
#include "NxPoolAllocation.tmh"

size_t
NxPoolAllocation::GetSize() const
{
    size_t bufferSize = 0;
    WdfMemoryGetBuffer(m_memory.get(), &bufferSize);
    return ALIGN_DOWN_BY(bufferSize, m_alignmentRequirement + 1);
}

PVOID
NxPoolAllocation::GetBuffer() const
{
    auto const va = WdfMemoryGetBuffer(m_memory.get(), nullptr);
    return ALIGN_DOWN_POINTER_BY(va, m_alignmentRequirement + 1);
}

PHYSICAL_ADDRESS
NxPoolAllocation::GetLogicalAddress() const
{
    return INVALID_PHYSICAL_ADDRESS;
}

NxAllocationType
NxPoolAllocation::GetAllocationType() const
{
    return NxAllocationType::Pool;
}
