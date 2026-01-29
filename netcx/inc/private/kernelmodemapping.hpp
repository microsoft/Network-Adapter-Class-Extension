// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <wil/resource.h>
#include "WilKernel.hpp"

struct _MDL;
typedef struct _MDL MDL;

template <typename T>
struct KernelModeMapping
{
    _IRQL_requires_max_(DISPATCH_LEVEL)
    KernelModeMapping(
        void
    ) noexcept = default;

    KernelModeMapping(
        const KernelModeMapping &
    ) noexcept = delete;

    KernelModeMapping &
    operator =(
        const KernelModeMapping &
    ) noexcept = delete;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    KernelModeMapping(
        KernelModeMapping && other
    ) noexcept
    {
        *this = wistd::move(other);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    KernelModeMapping &
    operator =(
        KernelModeMapping && other
    ) noexcept
    {
        this->m_mdl = wistd::move(other.m_mdl);
        this->m_kernelAddress = other.m_kernelAddress;
        other.m_kernelAddress = nullptr;

        return *this;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    ~KernelModeMapping(
        void
    )
    {
        Unmap();
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Map(
        _In_ T * UserAddress,
        _In_ size_t Size = sizeof(T),
        _In_ LOCK_OPERATION AccessMode = IoModifyAccess
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

        unique_io_mdl mdl
        {
            IoAllocateMdl(
                static_cast<void *>(UserAddress),
                ulSize,
                FALSE,
                TRUE,
                nullptr)
        };

        if (!mdl)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        __try
        {
            MmProbeAndLockPages(
                mdl.get(),
                UserMode,
                AccessMode);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }

        auto cleanup = wil::scope_exit([&mdl]()
        {
            MmUnlockPages(mdl.get());
        });

        auto kernelAddress = static_cast<T *>(
            MmMapLockedPagesSpecifyCache(
                mdl.get(),
                KernelMode,
                MmCached,
                nullptr,
                FALSE,
                NormalPagePriority | MdlMappingNoExecute));

        if (kernelAddress == nullptr)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        cleanup.release();

        // Now since nothing can fail we can unmap the old mapping
        Unmap();

        m_mdl = wistd::move(mdl);
        m_kernelAddress = kernelAddress;

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
            MmUnlockPages(m_mdl.get());

            m_kernelAddress = nullptr;
            m_mdl.reset();
        }
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    T *
    GetKernelAddress(
        void
    ) const
    {
        return m_kernelAddress;
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    T *
    operator ->(
        void
    ) const
    {
        return GetKernelAddress();
    }

private:

    unique_io_mdl
        m_mdl;

    T *
        m_kernelAddress{ nullptr };
};
