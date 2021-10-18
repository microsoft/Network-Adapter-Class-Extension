// Copyright (C) Microsoft Corporation. All rights reserved.

#include <ntddk.h>
#include <wdf.h>
#include <wdfcxbase.h>
#include <netadaptercx.h>
#include <netdevice_p.h>
#include <netadapterextension_p.h>
#include <NxTrace.hpp>

#pragma warning(push)
#pragma warning(disable:4201)
#include <Net20.h>
#include <Net21.h>
#pragma warning(pop)

#include "verifier.hpp"
#include "NxAdapterExtensionTypes.hpp"

static
size_t
sizeof_NET_ADAPTER_NDIS_PM_CAPABILITIES_version(
    _In_ WDF_CLASS_VERSION const & Version
)
{
    switch (MAKEVER(Version.Major, Version.Minor))
    {
        case MAKEVER(2,0):

            return sizeof(NET_ADAPTER_NDIS_PM_CAPABILITIES_V2_0);

        case MAKEVER(2,1):

            return sizeof(NET_ADAPTER_NDIS_PM_CAPABILITIES_V2_1);

        default:

            return 0;
    }
}

NxAdapterNdisPmCapabilities::NxAdapterNdisPmCapabilities(
        NX_PRIVATE_GLOBALS const & ClientGlobals,
        NET_ADAPTER_NDIS_PM_CAPABILITIES const & Capabilities
)
{
    Assign(ClientGlobals, Capabilities);
}

NxAdapterNdisPmCapabilities::operator bool(
    void
) const
{
    return Size != 0;
}

_Use_decl_annotations_
void
NxAdapterNdisPmCapabilities::Assign(
    NX_PRIVATE_GLOBALS const & ClientGlobals,
    NET_ADAPTER_NDIS_PM_CAPABILITIES const & Capabilities
)
{
    auto const expectedSize = sizeof_NET_ADAPTER_NDIS_PM_CAPABILITIES_version(ClientGlobals.ClientVersion);

    if (Capabilities.Size != expectedSize)
    {
        Verifier_ReportViolation(
            &ClientGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidStructTypeSize,
            Capabilities.Size,
            expectedSize);
    }

    switch (MAKEVER(ClientGlobals.ClientVersion.Major, ClientGlobals.ClientVersion.Minor))
    {
        case MAKEVER(2,1):
            SupportedProtocolOffloads = Capabilities.SupportedProtocolOffloads;
            __fallthrough;

        case MAKEVER(2,0):
            Size = Capabilities.Size;
            MediaSpecificWakeUpEvents = Capabilities.MediaSpecificWakeUpEvents;

            break;

    }
}

