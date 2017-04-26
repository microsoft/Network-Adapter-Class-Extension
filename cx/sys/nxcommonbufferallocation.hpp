/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxCommonBufferAllocation.hpp

Abstract:

    Used for DMA packet buffers allocated for Rx datapath

--*/

#pragma once

class NxCommonBufferAllocation : public INxQueueAllocation
{
    unique_wdf_common_buffer m_commonBuffer;

public:
    NxCommonBufferAllocation(
        unique_wdf_common_buffer&& commonBuffer) :
        m_commonBuffer(wistd::move(commonBuffer))
    {

    }

    size_t GetSize() const override;

    PVOID GetBuffer() const override;

    PHYSICAL_ADDRESS GetLogicalAddress() const override;

    NxAllocationType GetAllocationType() const override;
};
