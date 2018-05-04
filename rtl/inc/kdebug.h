
#pragma once

#include "KMacros.h"

template<ULONG SIGNATURE>
struct KRTL_CLASS NdisDebugBlock
{
#if DBG
    PAGED ~NdisDebugBlock() 
    {
        ASSERT_VALID();
        Signature |= 0x80;
    }
#endif

    NONPAGED void ASSERT_VALID() const
    {
#if DBG
        WIN_ASSERT(Signature == SIGNATURE);
#endif
    }

private:
#if DBG
    ULONG Signature = SIGNATURE;
#endif
};
