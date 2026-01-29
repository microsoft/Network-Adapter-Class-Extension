// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "netadaptercx_triage.h"
#include "ndisminiport/MiniportReference.hpp"

#include <NxCollection.hpp>

class NxAdapter;

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

    NTSTATUS
    GetAllAdapters(
        _Out_ Rtl::KArray<unique_wdf_reference<NETADAPTER>> & AdaptersArray
    ) const;
};
