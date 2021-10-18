// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "netadaptercx_triage.h"

#include <NxCollection.hpp>

class NxAdapter;

using unique_miniport_reference = wil::unique_any<NDIS_HANDLE, decltype(NdisWdfMiniportDereference), NdisWdfMiniportDereference>;

class NxAdapterCollection
    : public NxCollection<NxAdapter>
{
public:

    static
    void
    GetTriageInfo(
        void
    );

    unique_miniport_reference
    FindAndReferenceMiniportByInstanceName(
        _In_ UNICODE_STRING const * InstanceName
    ) const;

    NxAdapter *
    FindAndReferenceAdapterByBaseName(
        _In_ UNICODE_STRING const * BaseName
    ) const;
};
