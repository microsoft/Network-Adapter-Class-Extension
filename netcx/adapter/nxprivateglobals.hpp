// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

class NxDriver;
enum class MediaExtensionType;

#define NX_PRIVATE_GLOBALS_SIG 'IxNG'

constexpr
ULONG64
MAKEVER(
    ULONG major,
    ULONG minor
)
{
    return (static_cast<ULONG64>(major) << 16) | minor;
}

struct NX_PRIVATE_GLOBALS
{
    //
    // Equal to GLOBALS_SIG
    //
    ULONG
        Signature;

    //
    // Enable runtime verification checks for the client driver.
    //
    BOOLEAN
        CxVerifierOn;

    //
    // Public part of the globals
    //
    NET_DRIVER_GLOBALS
        Public;

    //
    // Pointer to the NxDriver
    //
    NxDriver *
        NxDriver;

    //
    // Pointer to the client driver's WDF globals
    //
    WDF_DRIVER_GLOBALS *
        ClientDriverGlobals;

    MediaExtensionType
        ExtensionType;

    //
    // Target NetAdapterCx version the client driver is bound to
    //
    WDF_CLASS_VERSION
        ClientVersion;

    bool
    IsClientVersionGreaterThanOrEqual(
        _In_ WDF_MAJOR_VERSION Major,
        _In_ WDF_MINOR_VERSION Minor
    );
};

inline
NX_PRIVATE_GLOBALS *
GetPrivateGlobals(
    NET_DRIVER_GLOBALS * PublicGlobals
)
{
    return CONTAINING_RECORD(
        PublicGlobals,
        NX_PRIVATE_GLOBALS,
        Public);
}
