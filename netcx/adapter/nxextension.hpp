// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KNew.h>
#include <KPtr.h>

#include "extension/NxExtension.hpp"

class NxExtension
{

public:

    NTSTATUS
    Initialize(
        _In_ NET_EXTENSION_PRIVATE const * Extension
    );

    NET_EXTENSION_PRIVATE
        m_extension = {};

private:

    KPoolPtr<WCHAR>
        m_extensionName;

};

