// Copyright (C) Microsoft Corporation. All rights reserved.


#ifdef _KERNEL_MODE

#include <ntddk.h>

#else

#include <windows.h>
#include <ntstatus.h>
#include <ntintsafe.h>

#include <nt/rtl/failfast.h>

typedef ULONG NODE_REQUIREMENT;
#define MM_ANY_NODE_OK 0x80000000

#include <ifdef.h>

#endif

#include <nt/rtl/integermacro.h>

#include <ntassert.h>

#include <NxTrace.hpp>
#include <NxTraceLogging.hpp>

#include "NxExtensionLayout.hpp"

#include "NxExtensionLayout.tmh"

static
size_t
CountTrailingZeros(
    _In_ size_t x
    )
{
    if (x == 0)
        return 32;

    auto n = 0u;

    if (0 == (x & 0x0000FFFF))
    {
        n = n + 16;
        x >>= 16;
    }
    if (0 == (x & 0x000000FF))
    {
        n = n + 8;
        x >>= 8;
    }
    if (0 == (x & 0x0000000F))
    {
        n = n + 4;
        x >>= 4;
    }
    if (0 == (x & 0x00000003))
    {
        n = n + 2;
        x >>= 2;
    }
    if (0 == (x & 0x00000001))
    {
        n = n + 1;
    }

    return n;
}

static
size_t
AssignLayoutWithArray(
    _In_ size_t Offset,
    _Inout_updates_all_(ElementCount) NET_EXTENSION_PRIVATE **Array,
    _In_ size_t ElementCount
)
{
    // Reset the offset assignments
    for (unsigned int i = 0; i < ElementCount; i++)
    {
        Array[i]->AssignedOffset = 0;
    }

    while (true)
    {
        auto targetAlignment = 1u << (CountTrailingZeros(Offset) + 1);
        NET_EXTENSION_PRIVATE *extension = nullptr;

        // Find the next unassigned block with the closest matching alignment

        for (unsigned int i = 0; i < ElementCount; i++)
        {
            if (Array[i]->AssignedOffset != 0)
                continue;

            if ((Array[i]->NonWdfStyleAlignment < targetAlignment) || (!extension))
            {
                extension = Array[i];
                break;
            }
        }

        // All are assigned
        if (extension == nullptr)
            break;

        extension->AssignedOffset = RTL_NUM_ALIGN_UP(Offset, extension->NonWdfStyleAlignment);
        Offset = extension->AssignedOffset + extension->Size;
    }

    return Offset;
}

static
size_t
ComputeSizeAndUpdateExtensions(
    _In_ size_t Offset,
    _In_ NET_EXTENSION_PRIVATE ** Array,
    _In_ size_t ElementCount
)
{
    return RTL_NUM_ALIGN_UP(AssignLayoutWithArray(Offset, Array, ElementCount), Offset);
}

NxExtensionLayout::NxExtensionLayout(
    size_t StartOffset,
    size_t MinimumAlignment
)
    : m_startOffset(StartOffset)
    , m_minimumAlignment(MinimumAlignment)
{
}

size_t
NxExtensionLayout::Generate(
    void
)
{
    if (m_extensions.count() == 0)
    {
        return RTL_NUM_ALIGN_UP(m_startOffset, m_minimumAlignment);
    }

    m_temporary.clear();
    for (auto & extension : m_extensions)
    {
        auto const appended = m_temporary.append(&extension);
        NT_FRE_ASSERT(appended);
    }

    return ComputeSizeAndUpdateExtensions(
        m_startOffset,
        &m_temporary[0],
        m_temporary.count());
}

NET_EXTENSION_PRIVATE const *
NxExtensionLayout::GetExtension(
    PCWSTR Name,
    UINT32 Version,
    NET_EXTENSION_TYPE Type
) const
{
    for (auto const & extension : m_extensions)
    {
        // caller isn't coherent
        NT_FRE_ASSERT(extension.Type == Type);

        if (0 == wcscmp(extension.Name, Name) && extension.Version >= Version)
        {
            return &extension;
        }
    }

    return nullptr;
}

NTSTATUS
NxExtensionLayout::PutExtension(
    PCWSTR Name,
    UINT32 Version,
    NET_EXTENSION_TYPE Type,
    size_t Size,
    size_t Alignment
)
{
    NT_FRE_ASSERT(Size != 0);
    NT_FRE_ASSERT(RTL_IS_POWER_OF_TWO(Alignment));

    // we should also can insertions of duplicates...

    auto const sort = [](NET_EXTENSION_PRIVATE const & l, NET_EXTENSION_PRIVATE const & r)
    {
        return l.NonWdfStyleAlignment < r.NonWdfStyleAlignment;
    };

    NET_EXTENSION_PRIVATE extension = {
        Name,
        (ULONG)Size,
        Version,
        (ULONG)Alignment,
        Type,
    };

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_temporary.reserve(m_extensions.count() + 1));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_extensions.insertSorted(extension, sort));

    return STATUS_SUCCESS;
}

