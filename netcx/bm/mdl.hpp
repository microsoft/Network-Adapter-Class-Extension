// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <wil/resource.h>

struct RtlMdl : public MDL
{

    static
    RtlMdl *
    Make(
        _In_ void * VirtualAddress,
        _In_ size_t Length
    );

    PFN_NUMBER *
    GetPfnArray(
        void
    );

    PFN_NUMBER const *
    GetPfnArray(
        void
    ) const;

    size_t
    GetPfnArrayCount(
        void
    ) const;

private:

    void *
    operator new(
        _In_ size_t BaseSize,
        _In_ void * VirtualAddress,
        _In_ size_t Length
    );

};

static_assert(sizeof(RtlMdl) == sizeof(MDL));
