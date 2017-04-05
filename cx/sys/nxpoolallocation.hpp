/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxPoolAllocation.hpp

Abstract:

    Used for pool packet buffers allocated for Rx datapath

--*/

#pragma once

class NxPoolAllocation : public INxQueueAllocation
{
    unique_wdf_memory m_memory;
    ULONG m_alignmentRequirement;

public:
    NxPoolAllocation(
        unique_wdf_memory&& memory,
        ULONG alignmentRequirement) :
        m_memory(wistd::move(memory)),
        m_alignmentRequirement(alignmentRequirement)
    {

    }

    size_t GetSize() const override;

    PVOID GetBuffer() const override;

    PHYSICAL_ADDRESS GetLogicalAddress() const override;

    NxAllocationType GetAllocationType() const override;
};
