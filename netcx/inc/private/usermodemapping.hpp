// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "WilKernel.hpp"

// This class is used to map an existing kernel-allocated memory buffer into a user-mode process.
// It's not advisable to use it directly, since unmapping can only be done in the user-mode process context,
// and thus is very error-prone. Instead, use the UserModeMemory class, which handles the allocation and unmapping in
// the process context automatically

template <typename T>
struct UserModeMapping
{
    _IRQL_requires_max_(DISPATCH_LEVEL)
    UserModeMapping(
        void
    ) noexcept = default;

    UserModeMapping(
        const UserModeMapping &
    ) noexcept = delete;

    UserModeMapping &
    operator =(
        const UserModeMapping &
    ) noexcept = delete;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UserModeMapping(
        UserModeMapping && other
    ) noexcept
    {
        *this = wistd::move(other);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UserModeMapping &
    operator =(
        UserModeMapping && other
    ) noexcept
    {
        this->m_mdl = wistd::move(other.m_mdl);
        this->m_userModeAddress = other.m_userModeAddress;
        other.m_userModeAddress = nullptr;

        return *this;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    ~UserModeMapping(
        void
    )
    {
        Unmap();
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Map(
        _In_ T * KernelAddress,
        _In_ size_t Size = sizeof(T)
    )
    {
        ULONG ulSize;

        NTSTATUS ntStatus = RtlSizeTToULong(
            Size,
            &ulSize);

        if (ntStatus != STATUS_SUCCESS)
        {
            return ntStatus;
        }

        void *kernelAddress{ nullptr };

        if constexpr (wistd::is_const_v<T>)
        {
            kernelAddress = const_cast<wistd::remove_const_t<T> *>(KernelAddress);
        }
        else
        {
            kernelAddress = KernelAddress;
        }

        unique_io_mdl mdl
        {
            IoAllocateMdl(
                kernelAddress,
                ulSize,
                FALSE,
                TRUE,
                nullptr)
        };

        if (!mdl)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        MmBuildMdlForNonPagedPool(mdl.get());

        T * userModeAddress{ nullptr };

        __try
        {
            userModeAddress = static_cast<T *>(
                MmMapLockedPagesSpecifyCache(
                    mdl.get(),
                    UserMode,
                    MmCached,
                    nullptr,
                    FALSE,
                    NormalPagePriority | MdlMappingNoExecute));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }

        if (userModeAddress == nullptr)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Unmap();

        m_mdl = wistd::move(mdl);
        m_userModeAddress = userModeAddress;

        return STATUS_SUCCESS;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    Unmap(
        void
    )
    {
        if (m_mdl)
        {
            if constexpr (wistd::is_const_v<T>)
            {
                MmUnmapLockedPages(
                    const_cast<wistd::remove_const_t<T> *>(m_userModeAddress),
                    m_mdl.get());
            }
            else
            {
                MmUnmapLockedPages(
                    m_userModeAddress,
                    m_mdl.get());
            }

            m_userModeAddress = nullptr;
            m_mdl.reset();
        }
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    T *
    GetUserModeAddress(
        void
    ) const
    {
        return m_userModeAddress;
    }

private:

    unique_io_mdl
        m_mdl;

    T *
        m_userModeAddress{ nullptr };
};

template <typename LambdaType>
typename wistd::result_of<LambdaType()>::type
RunInProcessContext(
    PEPROCESS Process,
    LambdaType Lambda
)
{
#ifdef _KERNEL_MODE
    if (PsGetCurrentProcess() == Process)
    {
        return Lambda();
    }

    KAPC_STATE apcState;
    KeStackAttachProcess((PKPROCESS)Process, &apcState);

    if constexpr (wistd::is_same_v<wistd::result_of_t<LambdaType()>, void>)
    {
        Lambda();
        KeUnstackDetachProcess(&apcState);

        return;
    }
    else
    {
        auto const result = Lambda();

        KeUnstackDetachProcess(&apcState);

        return result;
    }
#else
    UNREFERENCED_PARAMETER(Process);
    return Lambda();
#endif
}
