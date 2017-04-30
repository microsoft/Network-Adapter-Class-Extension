// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "KMacros.h"

template<ULONG SIGNATURE>
struct PAGED _NDIS_DEBUG_BLOCK
{
#if DBG
    PAGED _NDIS_DEBUG_BLOCK() WI_NOEXCEPT : Signature(SIGNATURE) { }
    PAGED ~_NDIS_DEBUG_BLOCK() 
    {
        ASSERT_VALID();
        Signature |= 0x80;
    }

private:
    ULONG Signature;
public:    
#endif

    PAGED void ASSERT_VALID() const
    {
#if DBG
        WIN_ASSERT(Signature == SIGNATURE);
#endif
    }
};

#define DEBUG_STRUCTURE(tag) _NDIS_DEBUG_BLOCK<tag>

