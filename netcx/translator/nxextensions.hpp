// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#pragma warning(push)
#pragma warning(default:4820) // warn if the compiler inserted padding

union TxExtensions {

    // order of structure members shall match TxQueueExtensions
    struct {

        NET_EXTENSION
            Checksum;

        NET_EXTENSION
            Lso;

        NET_EXTENSION
            LogicalAddress;

        NET_EXTENSION
            VirtualAddress;

        NET_EXTENSION
            Mdl;

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
            LogicalAddress;

        NET_EXTENSION
            VirtualAddress;

        NET_EXTENSION
            Mdl;

        NET_EXTENSION
            ReturnContext;

    } Extension;

    NET_EXTENSION
        Extensions[sizeof(Extension) / sizeof(NET_EXTENSION)];

};

#pragma warning(pop)

