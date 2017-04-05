/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    Version.hpp

Abstract:

    
Revision History:

--*/

#pragma once

#define INIT_GUID
#include <initguid.h>

#include <netadaptercxfuncenum.h>

extern WDFWAITLOCK g_RegistrationLock;

#define NX_PRIVATE_GLOBALS_SIG          'IxNG'

typedef struct _NX_PRIVATE_GLOBALS {
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
    PNxDriver NxDriver;

} NX_PRIVATE_GLOBALS, *PNX_PRIVATE_GLOBALS;


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


VOID 
FORCEINLINE
NxDbgBreak(
    PNX_PRIVATE_GLOBALS PrivateGlobals
    )
{
    if (PrivateGlobals->CxVerifierOn) {
        //
        // Not checking if KD is connected. If verifier is enabled and KD is not
        // connected a bugcheck is reasonable to draw attention to the failure.
        //
        DbgBreakPoint();
    }
}

PNX_PRIVATE_GLOBALS
FORCEINLINE
GetPrivateGlobals(
    PNET_DRIVER_GLOBALS PublicGlobals
    )
{
    return CONTAINING_RECORD(PublicGlobals,
                             NX_PRIVATE_GLOBALS,
                             Public);
}

extern "C"
DRIVER_INITIALIZE DriverEntry;
extern "C"
EVT_WDF_DRIVER_UNLOAD EvtDriverUnload;

extern "C"
NTSTATUS
NetAdapterCxInitialize(
    VOID
    );

extern "C"
VOID
NetAdapterCxDeinitialize(
    VOID
    );

extern "C"
NTSTATUS
NetAdapterCxBindClient(
    PWDF_CLASS_BIND_INFO ClassInfo,
    PWDF_COMPONENT_GLOBALS ClientGlobals
    );

extern "C"
VOID
NetAdapterCxUnbindClient(
    PWDF_CLASS_BIND_INFO ClassInfo,
    PWDF_COMPONENT_GLOBALS ClientGlobals
    );

NTSTATUS
CreateControlDevice(
    _Inout_ PUNICODE_STRING Name
    );

VOID
DeleteControlDevice(
    VOID
    );

extern NDIS_WDF_CX_DRIVER NetAdapterCxDriverHandle;
