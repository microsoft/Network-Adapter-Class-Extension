// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/extension.h>

#define MAXIMUM_ALLOWED_EXTENSION_SIZE 1024

struct NET_EXTENSION_PRIVATE
{
    /// Name of the packet extension.  String buffer is allocated with the
    /// NET_EXTENSION_PRIVATE block itself.
    PCWSTR Name = nullptr;

    /// Number of bytes requested to add to each packet.
    SIZE_T Size = 0;
    ULONG Version = 0;
    /// Alignment of the block to add to each packet.
    ULONG NonWdfStyleAlignment = 0;

    NET_EXTENSION_TYPE Type;

    /// The actual offset that was assigned to the packet extension, as
    /// measured from the start of the NET_PACKET.  Or, zero if no offset
    /// has been assigned yet.
    SIZE_T AssignedOffset = 0;
};

