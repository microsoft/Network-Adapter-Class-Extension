// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _NET_MEMORY_MDL_WRAPPER
{
    // For debugging purposes and driver verifier checks
    ULONG OsOwnedReferenceCount;
    MDL Mdl;
} NET_MEMORY_MDL_WRAPPER;

#ifdef __cplusplus
}
#endif
