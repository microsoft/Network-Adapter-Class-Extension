// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

// This class is used to copy memory from user-mode pointer into kernel-allocated buffer.
// It handles the allocation and freeing of the buffer automatically, as well as probing the user-mode buffer

template<typename T>
class UserModeMemoryCopy {

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CheckAndCopy(
        _In_ T* UserModeBuffer,
        _In_ SIZE_T Size
    )
    {
        __try
        {
            ProbeForRead(
                UserModeBuffer,
                Size,
                sizeof(UCHAR));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (Size == 0)
        {
            return STATUS_INVALID_PARAMETER;
        }

        auto bufferCopy = MakeSizedPoolPtrNP<T>('pcmu', Size);
        if (!bufferCopy)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        __try
        {
            RtlCopyMemory(
                bufferCopy.get(),
                UserModeBuffer,
                Size);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_INVALID_PARAMETER;
        }

        m_allocatedBuffer = wistd::move(bufferCopy);
        return STATUS_SUCCESS;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    T *Get(
        void
    ) const noexcept
    {
        return m_allocatedBuffer.get();
    }

private:

    KPoolPtrNP<T>
        m_allocatedBuffer;
};
