// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#define INIT_GUID
#include <initguid.h>

#include <netfuncenum.h>

#include <NxXlat.hpp>

extern WDFWAITLOCK g_RegistrationLock;

#define NX_PRIVATE_GLOBALS_SIG          'IxNG'

class NxDriver;

struct NX_PRIVATE_GLOBALS {
    //
    // Equal to GLOBALS_SIG
    //
    ULONG Signature;

    //
    // Enable runtime verification checks for the client driver.
    //
    BOOLEAN CxVerifierOn;

    //
    // Public part of the globals
    //
    NET_DRIVER_GLOBALS Public;

    //
    // Pointer to the NxDriver
    //
    NxDriver * NxDriver;

    //
    // Pointer to the client driver's WDF globals
    //
    WDF_DRIVER_GLOBALS *ClientDriverGlobals;

    //
    // True if the client driver is a media specific
    // extension. For the list of such client drivers
    // see Verifier_VerifyIsMediaExtension
    //
    bool IsMediaExtension;
};

class CxDriverContext
{
public:
    //
    // Built-in datapath apps: NBL translator
    //
    NxTranslationAppFactory TranslationAppFactory;

    NTSTATUS Init();

    static void Destroy(_In_ WDFOBJECT Driver);
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CxDriverContext, GetCxDriverContextFromHandle);


FORCEINLINE
VOID
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

