/*++
Copyright (c) Microsoft Corporation

Module Name:

    KPtr.h

Abstract:

    Aliases wistd::unique_ptr as KPtr, a standards-conformant implementation of std::unique_ptr
    that is available in kernel mode.

Environment:

    Kernel mode or usermode unittest

--*/

#pragma once

#include <cstddef>
#include <wil\resource.h>
#include "KDeletePolicy.h"
#include <wil\wistd_type_traits.h>

template<typename T, typename TDELETE = KDeletePtr<T>>
using KPtr = wistd::unique_ptr<T, TDELETE>;

template<typename T>
using KPoolPtr = KPtr<T, KFreePool<T>>;

template<typename T>
using KPoolPtrNP = KPtr<T, KFreePoolNP<T>>;

// Allocates 1 object from the nonpaged pool.
template<typename T>
PAGED KPoolPtr<T> MakePoolPtr(ULONG poolTag)
{
    PAGED_CODE();

    return MakeSizedPoolPtr<T>(poolTag, sizeof(T));
}

// Allocates 1 object from the nonpaged pool.
template<typename T>
NONPAGED KPoolPtrNP<T> MakePoolPtrNP(ULONG poolTag)
{
    return MakeSizedPoolPtrNP<T>(poolTag, sizeof(T));
}

// Allocates `allocationSize` bytes to hold the object
template<typename THeader>
PAGED KPoolPtr<THeader> MakeSizedPoolPtr(ULONG poolTag, size_t allocationSize)
{
    PAGED_CODE();

    WIN_ASSERT(allocationSize >= sizeof(THeader));

#if (NTDDI_VERSION >= NTDDI_WIN10_VB) && !defined(KRTL_USE_LEGACY_POOL_API)

    auto allocation = ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        allocationSize,
        poolTag);
    if (!allocation)
    {
        return nullptr;
    }

#else // (NTDDI_VERSION >= NTDDI_WIN10_VB && !defined(KRTL_USE_LEGACY_POOL_API)

    auto allocation = ExAllocatePoolWithTag(NonPagedPoolNx, allocationSize, poolTag);
    if (!allocation)
    {
        return nullptr;
    }

    RtlZeroMemory(allocation, allocationSize);

#endif // (NTDDI_VERSION >= NTDDI_WIN10_VB) && !defined(KRTL_USE_LEGACY_POOL_API)

    // Still call the default constructor for the type in case of custom field initializers
    return KPoolPtr<THeader>(new (allocation) THeader());
}

// Allocates `allocationSize` bytes to hold the object
template<typename THeader>
NONPAGED KPoolPtrNP<THeader> MakeSizedPoolPtrNP(ULONG poolTag, size_t allocationSize)
{
    WIN_ASSERT(allocationSize >= sizeof(THeader));

#if (NTDDI_VERSION >= NTDDI_WIN10_VB) && !defined(KRTL_USE_LEGACY_POOL_API)

    auto allocation = ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        allocationSize,
        poolTag);
    if (!allocation)
    {
        return nullptr;
    }

#else // (NTDDI_VERSION >= NTDDI_WIN10_VB) && !defined(KRTL_USE_LEGACY_POOL_API)

    auto allocation = ExAllocatePoolWithTag(NonPagedPoolNx, allocationSize, poolTag);
    if (!allocation)
    {
        return nullptr;
    }

    RtlZeroMemory(allocation, allocationSize);

#endif // (NTDDI_VERSION >= NTDDI_WIN10_VB) && !defined(KRTL_USE_LEGACY_POOL_API)

    // Still call the default constructor for the type in case of custom field initializers
    return KPoolPtrNP<THeader>(new (allocation) THeader());
}

// Allocates `payloadSize` extra bytes after the object
template<typename THeader>
PAGED KPoolPtr<THeader> MakeExtendedPoolPtr(ULONG poolTag, ULONG payloadSize, _Out_opt_ PULONG allocationSize = nullptr)
{
    PAGED_CODE();

    if (allocationSize)
    {
        *allocationSize = 0;
    }

    ULONG totalSize;
    if (!NT_SUCCESS(RtlULongAdd(sizeof(THeader), payloadSize, &totalSize)))
    {
        return nullptr;
    }

    auto allocation = MakeSizedPoolPtr<THeader>(poolTag, totalSize);
    if (!allocation)
    {
        return nullptr;
    }

    if (allocationSize)
    {
        *allocationSize = totalSize;
    }

    return allocation;
}
