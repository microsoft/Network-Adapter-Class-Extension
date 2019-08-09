// Copyright (C) Microsoft Corporation. All rights reserved.

#include "bmprecomp.hpp"
#include "Mdl.hpp"
#include "Mdl.tmh"

_Use_decl_annotations_
RtlMdl *
RtlMdl::Make(
    void * VirtualAddress,
    size_t Length
)
{
    auto mdl = new (VirtualAddress, Length) RtlMdl();

    if (mdl == nullptr)
    {
        return nullptr;
    }

    MmInitializeMdl(
        mdl,
        VirtualAddress,
        Length);

    return mdl;
}

_Use_decl_annotations_
void *
RtlMdl::operator new(
    size_t BaseSize,
    void * VirtualAddress,
    size_t Length
)
{
    auto allocationSize = MmSizeOfMdl(
        VirtualAddress,
        Length);

    NT_FRE_ASSERT(allocationSize > BaseSize);

    auto storage = ExAllocatePoolWithTag(
        NonPagedPoolNx,
        allocationSize,
        BUFFER_MANAGER_POOL_TAG);

    if (storage == nullptr)
    {
        return nullptr;
    }

    RtlZeroMemory(storage, allocationSize);
    return storage;
}

PFN_NUMBER *
RtlMdl::GetPfnArray(
    void
)
{
    return MmGetMdlPfnArray(this);
}

PFN_NUMBER const *
RtlMdl::GetPfnArray(
    void
) const
{
    return MmGetMdlPfnArray(this);
}

size_t
RtlMdl::GetPfnArrayCount(
    void
) const
{
    return ADDRESS_AND_SIZE_TO_SPAN_PAGES(
        MmGetMdlVirtualAddress(this),
        MmGetMdlByteCount(this));
}
