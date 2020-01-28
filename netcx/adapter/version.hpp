// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#define INIT_GUID
#include <initguid.h>

#include <netfuncenum.h>

#include <NxXlat.hpp>

extern WDFWAITLOCK g_RegistrationLock;

#define NX_PRIVATE_GLOBALS_SIG          'IxNG'

class NxDriver;
enum class MediaExtensionType;

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

class CxDriverContext
{
public:
    //
    // Built-in datapath apps: NBL translator
    //
    NxTranslationAppFactory
        TranslationAppFactory;

    NTSTATUS
    Init(
        void
    );

    static void
    Destroy(
        _In_ WDFOBJECT Driver
    );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CxDriverContext, GetCxDriverContextFromHandle);


FORCEINLINE
void
NxDbgBreak(
    NX_PRIVATE_GLOBALS * PrivateGlobals
)
{
    if (PrivateGlobals->CxVerifierOn && ! KdRefreshDebuggerNotPresent())
    {
        DbgBreakPoint();
    }
}

FORCEINLINE
NX_PRIVATE_GLOBALS *
GetPrivateGlobals(
    NET_DRIVER_GLOBALS * PublicGlobals
)
{
    return CONTAINING_RECORD(PublicGlobals,
                             NX_PRIVATE_GLOBALS,
                             Public);
}

extern NDIS_WDF_CX_DRIVER NetAdapterCxDriverHandle;

