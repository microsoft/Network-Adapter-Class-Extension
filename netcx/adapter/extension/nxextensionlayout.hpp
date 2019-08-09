// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KArray.h>
#include <net/extension.h>

#include "NxExtension.hpp"

#ifdef TEST_HARNESS_CLASS_NAME
#define TEST_HARNESS_FRIEND_DECLARATION friend class TEST_HARNESS_CLASS_NAME
#else
#define TEST_HARNESS_FRIEND_DECLARATION
#endif

class NxExtensionLayout
{
    TEST_HARNESS_FRIEND_DECLARATION;

public:

    NxExtensionLayout(
        size_t StartOffset,
        size_t MinimumAlignment
    );

    size_t
    Generate(
        void
    );

    NET_EXTENSION_PRIVATE const *
    GetExtension(
        PCWSTR Name,
        UINT32 Version,
        NET_EXTENSION_TYPE Type
    ) const;

    NTSTATUS
    PutExtension(
        PCWSTR Name,
        UINT32 Version,
        NET_EXTENSION_TYPE Type,
        size_t Size,
        size_t Alignment
    );

private:

    size_t
        m_startOffset = 0;

    size_t
        m_minimumAlignment = 0;

    Rtl::KArray<NET_EXTENSION_PRIVATE>
        m_extensions;

    Rtl::KArray<NET_EXTENSION_PRIVATE *>
        m_temporary;

};


