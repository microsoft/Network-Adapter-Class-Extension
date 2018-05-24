// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KPtr.h>
#include <NetPacketExtensionPrivate.h>

class NxPacketExtensionPrivate
{

public:

    NTSTATUS
    Initialize(
        _In_ NET_PACKET_EXTENSION_PRIVATE const * Extension
        );

    NET_PACKET_EXTENSION_PRIVATE
        m_extension = {};

private:

    KPoolPtr<WCHAR>
        m_extensionName;

};

