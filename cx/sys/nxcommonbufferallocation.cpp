/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxCommonBufferAllocation.cpp

Abstract:

    Used for DMA packet buffers allocated for Rx datapath

--*/

#include "Nx.hpp"
#include "NxCommonBufferAllocation.tmh"

size_t
NxCommonBufferAllocation::GetSize() const
{
    return WdfCommonBufferGetLength(m_commonBuffer.get());
}

PVOID
NxCommonBufferAllocation::GetBuffer() const
{
    return WdfCommonBufferGetAlignedVirtualAddress(m_commonBuffer.get());
}

PHYSICAL_ADDRESS
NxCommonBufferAllocation::GetLogicalAddress() const
{
    return WdfCommonBufferGetAlignedLogicalAddress(m_commonBuffer.get());
}

NxAllocationType
NxCommonBufferAllocation::GetAllocationType() const
{
    return NxAllocationType::DmaCommonBuffer;
}
