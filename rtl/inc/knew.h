// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "umwdm.h"
#include "KDebug.h"
#include "KMacros.h"
#include <new.h>

//
//                                           KALLOCATOR           KALLOCATOR_NONPAGED
// ---------------------------------------+--------------------+--------------------+
//  The object must be allocated at IRQL: | = PASSIVE_LEVEL    |  = PASSIVE_LEVEL   |
// ---------------------------------------+--------------------+--------------------+
//  The object must be freed at IRQL:     | = PASSIVE_LEVEL    |  = PASSIVE_LEVEL   |
// ---------------------------------------+--------------------+--------------------+
//  Constructor & destructor run at:      | = PASSIVE_LEVEL    |  = PASSIVE_LEVEL   |
// ---------------------------------------+--------------------+--------------------+
//  Member functions default to:          | PAGED code segment | .text code segment |
// ---------------------------------------+--------------------+--------------------+
//  Compiler-generated code goes to:      | PAGED code segment | .text code segment |
// ---------------------------------------+--------------------+--------------------+
//  The memory is allocated from pool:    | paged or nonpaged  | paged or nonpaged  |
// ---------------------------------------+--------------------+--------------------+
//

PAGED void *operator new(size_t s, std::nothrow_t const &, ULONG tag);
PAGED void operator delete(void *p, ULONG tag);
PAGED void *operator new[](size_t s, std::nothrow_t const &, ULONG tag);
PAGED void operator delete[](void *p, ULONG tag);
PAGEDX void __cdecl operator delete[](void *p);
void __cdecl operator delete(void *p);

template <ULONG TAG, ULONG ARENA = PagedPool>
struct PAGED KALLOCATION_TAG
{
    static const ULONG AllocationTag = TAG;
    static const ULONG AllocationArena = ARENA;
};

template <ULONG TAG, ULONG ARENA = NonPagedPoolNx>
struct KALLOCATION_TAG_NONPAGED
{
    static const ULONG AllocationTag = TAG;
    static const ULONG AllocationArena = ARENA;
};

template <ULONG TAG, ULONG ARENA = PagedPool>
struct PAGED KALLOCATOR : public KALLOCATION_TAG<TAG, ARENA>
{
    // Scalar new & delete

    PAGED void *operator new(size_t cb, std::nothrow_t const &)
    {
        PAGED_CODE();
        return ExAllocatePoolWithTag(static_cast<POOL_TYPE>(ARENA), cb, TAG);
    }

    PAGED void operator delete(void *p)
    {
        PAGED_CODE();

        if (p != nullptr)
        {
            ExFreePoolWithTag(p, TAG);
        }
    }

    // Scalar new with bonus bytes

    PAGED void *operator new(size_t cb, std::nothrow_t const &, size_t extraBytes)
    {
        PAGED_CODE();

        auto size = cb + extraBytes;

        // Overflow check
        if (size < cb)
            return nullptr;

        return ExAllocatePoolWithTag(static_cast<POOL_TYPE>(ARENA), size, TAG);
    }

    // Array new & delete

    PAGED void *operator new[](size_t cb, std::nothrow_t const &)
    {
        PAGED_CODE();
        return ExAllocatePoolWithTag(static_cast<POOL_TYPE>(ARENA), cb, TAG);
    }

    PAGED void operator delete[](void *p)
    {
        PAGED_CODE();

        if (p != nullptr)
        {
            ExFreePoolWithTag(p, TAG);
        }
    }

    // Placement new & delete
    
    PAGED void *operator new(size_t n, void * p)
    {
        PAGED_CODE();
        UNREFERENCED_PARAMETER((n));
        return p;
    }
    
    PAGED void operator delete(void *p1, void *p2)
    {
        PAGED_CODE();
        UNREFERENCED_PARAMETER((p1, p2));
    }
};

template <ULONG TAG, ULONG ARENA = NonPagedPoolNx>
struct KALLOCATOR_NONPAGED : public KALLOCATION_TAG_NONPAGED<TAG, ARENA>
{
    // Scalar new & delete

    PAGED void *operator new(size_t cb, std::nothrow_t const &)
    {
        PAGED_CODE();
        return ExAllocatePoolWithTag(static_cast<POOL_TYPE>(ARENA), cb, TAG);
    }

    PAGED void operator delete(void *p)
    {
        PAGED_CODE();

        if (p != nullptr)
        {
            ExFreePoolWithTag(p, TAG);
        }
    }

    // Scalar new with bonus bytes

    PAGED void *operator new(size_t cb, std::nothrow_t const &, size_t extraBytes)
    {
        PAGED_CODE();

        auto size = cb + extraBytes;

        // Overflow check
        if (size < cb)
            return nullptr;

        return ExAllocatePoolWithTag(static_cast<POOL_TYPE>(ARENA), size, TAG);
    }

    // Array new & delete

    PAGED void *operator new[](size_t cb, std::nothrow_t const &)
    {
        PAGED_CODE();
        return ExAllocatePoolWithTag(static_cast<POOL_TYPE>(ARENA), cb, TAG);
    }

    PAGED void operator delete[](void *p)
    {
        PAGED_CODE();

        if (p != nullptr)
        {
            ExFreePoolWithTag(p, TAG);
        }
    }

    // Placement new & delete
    
    PAGED void *operator new(size_t n, void * p)
    {
        PAGED_CODE();
        UNREFERENCED_PARAMETER((n));
        return p;
    }
    
    PAGED void operator delete(void *p1, void *p2)
    {
        PAGED_CODE();
        UNREFERENCED_PARAMETER((p1, p2));
    }
};

template <ULONG TAG>
struct __declspec(empty_bases) PAGED PAGED_OBJECT :
    public KALLOCATOR<TAG, PagedPool>, 
    public _NDIS_DEBUG_BLOCK<TAG>
{

};

template <ULONG TAG>
struct __declspec(empty_bases) PAGED NONPAGED_OBJECT :
    public KALLOCATOR<TAG, NonPagedPoolNx>, 
    public _NDIS_DEBUG_BLOCK<TAG>
{

};

