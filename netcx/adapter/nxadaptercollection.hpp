// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "netadaptercx_triage.h"

#include <NxCollection.hpp>

class NxAdapter;

class NxAdapterCollection : public NxCollection<NxAdapter>
{
public:

    static
    void
    GetTriageInfo(
        void
    );

    NxAdapter *
    FindAndReferenceAdapterByInstanceName(
        _In_ UNICODE_STRING const * InstanceName
    ) const;

    NxAdapter *
    FindAndReferenceAdapterByBaseName(
        _In_ UNICODE_STRING const * BaseName
    ) const;
};
