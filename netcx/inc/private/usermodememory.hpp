// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "UserModeMapping.hpp"

// This class is a specialized resource wrapper to handle allocating and freeing memory that is mapped into
// kernel and a single user process address space. It will handle switching to the right process context
// if needed.
// It's primarily used when we need to allocate memory on kernel side and then share it with the user process.
// This class can only be used at APC or PASSIVE_LEVEL.

template <typename T>
class UserModeMemory
{
public:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UserModeMemory(
        void
    ) noexcept = default;

    UserModeMemory(
        const UserModeMemory &
    ) noexcept = delete;

    UserModeMemory &
    operator =(
        const UserModeMemory &
    ) noexcept = delete;

    _IRQL_requires_max_(DISPATCH_LEVEL)
        UserModeMemory(
        UserModeMemory && other
    ) noexcept
    {
        *this = wistd::move(other);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UserModeMemory &
    operator =(
        UserModeMemory && other
    ) noexcept
    {
        this->m_kernelModeAddress = wistd::move(other.m_kernelModeAddress);
        this->m_userModeProcess = wistd::move(other.m_userModeProcess);
        this->m_userModeMapping = wistd::move(other.m_userModeMapping);

        return *this;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    ~UserModeMemory(
        void
    )
    {
        Reset();
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Allocate(
        _In_ PEPROCESS UserModeProcess,
        _In_ ULONG PoolTag,
        _In_ SIZE_T Size = sizeof(T)
    )
    {
        // Keep a reference to the process whose address space we will use
        NTSTATUS ntStatus = ObReferenceObjectByPointer(
            UserModeProcess,
            PROCESS_ALL_ACCESS,
            *PsProcessType,
            KernelMode);

        if (ntStatus != STATUS_SUCCESS)
        {
            return ntStatus;
        }

        wil::unique_any<PEPROCESS, decltype(::ObfDereferenceObject), ::ObfDereferenceObject> userModeProcess{ UserModeProcess };

        // Align the requested allocation up to PAGE_SIZE, this is because address mapping is done at PAGE_SIZE granularity, if we don't
        // align up we may expose unrelated data to the host process.
        const SIZE_T bufferSize = ALIGN_UP_BY(
            Size,
            PAGE_SIZE);

        auto baseAddress = MakeSizedPoolPtrNP<T>(PoolTag, bufferSize);
        if (!baseAddress)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        UserModeMapping<T> userModeMapping;

        ntStatus = RunInProcessContext(
            userModeProcess.get(),
            [&userModeMapping, &baseAddress, bufferSize]()
            {
                return userModeMapping.Map(
                    baseAddress.get(),
                    bufferSize);
            });

        if (ntStatus != STATUS_SUCCESS)
        {
            return ntStatus;
        }

        Reset();

        m_kernelModeAddress = wistd::move(baseAddress);
        m_userModeProcess = wistd::move(userModeProcess);
        m_userModeMapping = wistd::move(userModeMapping);

        return STATUS_SUCCESS;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Reset(
        void
    )
    {
        if (m_kernelModeAddress)
        {
            RunInProcessContext(
                m_userModeProcess.get(),
                [this]()
                {
                    m_userModeMapping.Unmap();
                });

            m_kernelModeAddress.reset();
            m_userModeProcess.reset();
        }
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    T *
    GetUserModeAddress(
        void
    ) const
    {
        return m_userModeMapping.GetUserModeAddress();
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    T *
    GetKernelModeAddress(
        void
    ) const
    {
        return m_kernelModeAddress.get();
    }

private:

    KPoolPtrNP<T>
        m_kernelModeAddress{ nullptr };

    wil::unique_any<PEPROCESS, decltype(::ObfDereferenceObject), ::ObfDereferenceObject>
        m_userModeProcess;

    UserModeMapping<T>
        m_userModeMapping;
};
