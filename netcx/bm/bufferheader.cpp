// Copyright (C) Microsoft Corporation. All rights reserved.
#include "bmprecomp.hpp"

#include "BufferPool.hpp"
#include "BufferHeader.hpp"
#include "BufferHeader.tmh"

static const BYTE
    HeaderPadding = 0xca;

static const BYTE
    BufferPadding = 0xbd;

_Use_decl_annotations_
BufferHeader *
BufferHeader::FromListEntry(
    LIST_ENTRY * ListEntry
)
{
    auto bufferHeader = CONTAINING_RECORD(
        ListEntry,
        BufferHeader,
        m_linkage);

    bufferHeader->ASSERT_VALID();
    return bufferHeader;
}

_Use_decl_annotations_
BufferHeader *
BufferHeader::FromMdl(
    MDL * Mdl
)
{
    // Since the allocation is page aligned we know the buffer header is at the beginning of the page
    auto bufferHeader = static_cast<BufferHeader *>(MmGetMdlBaseVa(Mdl));

    bufferHeader->ASSERT_VALID();
    return bufferHeader;
}

_Use_decl_annotations_
BufferHeader::BufferHeader(
    LIST_ENTRY * FreeListHead,
    LIST_ENTRY * UsedListHead,
    UINT64 BaseLogicalAddress,
    SIZE_T BufferSize,
    SIZE_T BufferOffset,
    SIZE_T BufferPaddingSize
)
    : m_freeListHead(FreeListHead)
    , m_usedListHead(UsedListHead)
    , m_baseLogicalAddress(BaseLogicalAddress)
    , m_bufferSize(BufferSize)
    , m_bufferOffset(BufferOffset)
    , m_bufferPaddingSize(BufferPaddingSize)
{
    // The allocation for the buffer has the following layout in memory
    //
    // -----------------------------------------------------------------
    // | BufferHeader | Header padding | Usable buffer | Buffer padding |
    //  ----------------------------------------------------------------
    //
    // Header padding: Used to ensure the start of the buffer used by drivers is correctly aligned, can be zero
    // Buffer padding: Used to ensure the size of the entire allocation is a multiple of PAGE_SIZE, can be zero
    //
    // If any of the paddings are non-zero the memory is filled with special values
    auto bufferStart = static_cast<BYTE *>(GetVirtualAddress());
    auto headerPaddingStart = (BYTE *)(this + 1);
    auto bufferPaddingStart = bufferStart + m_bufferSize;
    size_t const headerPaddingSize = bufferStart - headerPaddingStart;

    if (headerPaddingSize > 0)
    {
        RtlFillMemory(
            headerPaddingStart,
            headerPaddingSize,
            HeaderPadding);
    }

    if (m_bufferPaddingSize > 0)
    {
        RtlFillMemory(
            bufferPaddingStart,
            m_bufferPaddingSize,
            BufferPadding);
    }
}

_Use_decl_annotations_
NTSTATUS
BufferHeader::Initialize(
    void
)
{
    // Build a MDL for the usable part of the allocation
    auto const bufferVirtualAddress = GetVirtualAddress();
    size_t const mdlSize = MmSizeOfMdl(bufferVirtualAddress, m_bufferSize);

    m_mdl = MakeSizedPoolPtrNP<MDL>('ldMB', mdlSize);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !m_mdl);

    MmInitializeMdl(
        m_mdl.get(),
        bufferVirtualAddress,
        m_bufferSize);

    MmBuildMdlForNonPagedPool(m_mdl.get());

    // Initial state is freed and available for use
    m_flags.Freed = 1;
    InsertTailList(m_freeListHead, &m_linkage);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
MDL *
BufferHeader::GetMdl(
    void
)
{
    return m_mdl.get();
}

_Use_decl_annotations_
void
BufferHeader::ASSERT_VALID(
    void
) const
{
    NT_FRE_ASSERTMSG(
        "Buffer header signature is invalid. Memory corruption likely happened",
        m_signature1 == BufferSignature);

    NT_FRE_ASSERTMSG(
        "Buffer header signature is invalid. Memory corruption likely happened",
        m_signature2 == BufferSignature);
}

_Use_decl_annotations_
void *
BufferHeader::GetVirtualAddress(
    void
) const
{
    return (BYTE  *)(this) + m_bufferOffset;
}

_Use_decl_annotations_
UINT64
BufferHeader::GetLogicalAddress(
    void
) const
{
    return m_baseLogicalAddress != 0
        ? m_baseLogicalAddress + m_bufferOffset
        : 0;
}

_Use_decl_annotations_
void
BufferHeader::MarkAsUsed(
    void
)
{
    NT_FRE_ASSERTMSG(
        "An internal buffer handling error happened, buffer marked as not free is being initialized for allocation",
        m_flags.Freed == 1);

    m_flags.Freed = 0;

    NT_FRE_ASSERTMSG(
        "An internal buffer handling error happened, free buffer did not have 0 references",
        m_referenceCount == 0);

    Reference();
    InsertTailList(m_usedListHead, &m_linkage);
}

_Use_decl_annotations_
void
BufferHeader::Reference(
    void
)
{
    ASSERT_VALID();
    m_referenceCount++;
}

_Use_decl_annotations_
void
BufferHeader::Dereference(
    void
)
{
    ASSERT_VALID();
    m_referenceCount--;

    if (m_referenceCount == 0)
    {
        NT_FRE_ASSERTMSG(
            "Buffer is being destroyed without being freed. Likely a reference count imbalance happened",
            m_flags.Freed == 1);

        // Users are free to chain MDLs, when the last reference is taken make sure to break any links
        GetMdl()->Next = nullptr;

        // Move buffer from used to free list
        RemoveEntryList(&m_linkage);
        InsertTailList(m_freeListHead, &m_linkage);
    }
}

_Use_decl_annotations_
void
BufferHeader::Free(
    void
)
{
    ASSERT_VALID();
    NT_FRE_ASSERT(m_flags.Freed == 0);

    m_flags.Freed = 1;
    Dereference();
}
