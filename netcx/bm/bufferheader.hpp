// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <kptr.h>

class BufferPool2;

static const ULONG
    BufferSignature = 0xa60c;

class BufferHeader
{
#ifdef TEST_HARNESS_CLASS_NAME
    friend class
        TEST_HARNESS_CLASS_NAME;
#endif

public:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    static
    BufferHeader *
    FromListEntry(
        LIST_ENTRY * ListEntry
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    static
    BufferHeader *
    FromMdl(
        MDL * Mdl
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    BufferHeader(
        _In_ LIST_ENTRY * FreeListHead,
        _In_ LIST_ENTRY * UsedListHead,
        _In_ UINT64 BaseLogicalAddress,
        _In_ SIZE_T BufferSize,
        _In_ SIZE_T BufferOffset,
        _In_ SIZE_T BufferPaddingSize
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    ASSERT_VALID(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void *
    GetVirtualAddress(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    UINT64
    GetLogicalAddress(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    MarkAsUsed(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Reference(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Dereference(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Free(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    MDL *
    GetMdl(
        void
    );

private:

    ULONG const
        m_signature1 = BufferSignature;

    struct
    {
        ULONG
            Freed : 1;

        ULONG
            NotUsed : 31;
    } m_flags = {};

    LIST_ENTRY
        m_linkage = {};

    LIST_ENTRY * const
        m_freeListHead;

    LIST_ENTRY * const
        m_usedListHead;

    UINT64
        m_baseLogicalAddress;

    SIZE_T const
        m_bufferSize;

    SIZE_T const
        m_bufferOffset;

    SIZE_T const
        m_bufferPaddingSize;

    KPoolPtrNP<MDL>
        m_mdl;

    ULONG
        m_referenceCount = 0;

    ULONG const
        m_signature2 = BufferSignature;
};
