// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <karray.h>
#include <kptr.h>

class PAGED IBufferVector :
    public NONPAGED_OBJECT<'vmxn'>
{
    friend class BufferPool;

public:

    IBufferVector(
        _In_ size_t BufferCount,
        _In_ size_t BufferOffset,
        _In_ size_t BufferCapacity
    );

    virtual
    ~IBufferVector(
        void
    ) = default;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    Initialize(
        void
    );

    void
    GetEntryByIndex(
        _In_ size_t Index,
        _Out_ NET_DATA_HEADER * NetDataHeader
    ) const;

    virtual
    void *
    GetKernelModeVirtualAddress(
        _In_ size_t Index
    ) const = 0;

    virtual
    ULONG64
    GetLogicalAddress(
        _In_ size_t Index
    ) const = 0;

private:
    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CreateMdls(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    MapToUserModeAddressSpace(
        void
    );

protected:

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    UnmapFromUserModeAddressSpace(
        void
    );

    size_t
        m_BufferCount = 0;

    size_t
        m_BufferOffset = 0;

    size_t
        m_BufferCapacity = 0;

    Rtl::KArray<KPoolPtrNP<MDL>, NonPagedPoolNx>
        m_mdl;

#if NETCX_REDIRECTOR
    Rtl::KArray<void *, NonPagedPoolNx>
        m_userModeAddress;
#endif
};
