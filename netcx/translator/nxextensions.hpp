// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/extension.h>

#pragma warning(push)
#pragma warning(default:4820) // warn if the compiler inserted padding

union TxExtensions {

    // order of structure members shall match TxQueueExtensions
    struct {

        NET_EXTENSION
            Checksum;

        NET_EXTENSION
            Gso;

        NET_EXTENSION
            WifiExemptionAction;

        NET_EXTENSION
            LogicalAddress;

        NET_EXTENSION
            VirtualAddress;

        NET_EXTENSION
            Mdl;

        NET_EXTENSION
            Ieee8021q;

    } Extension;

    NET_EXTENSION
        Extensions[sizeof(Extension) / sizeof(NET_EXTENSION)];

};

union RxExtensions {

    // order of structure members shall match RxQueueExtensions
    struct {

        NET_EXTENSION
            Checksum;

        NET_EXTENSION
            Rsc;

        NET_EXTENSION
            RscTimestamp;

        NET_EXTENSION
            DataBuffer;

        NET_EXTENSION
            LogicalAddress;

        NET_EXTENSION
            VirtualAddress;

        NET_EXTENSION
            ReturnContext;

        NET_EXTENSION
            Ieee8021q;

        NET_EXTENSION
            Hash;
    } Extension;

    NET_EXTENSION
        Extensions[sizeof(Extension) / sizeof(NET_EXTENSION)];

};

#pragma warning(pop)

