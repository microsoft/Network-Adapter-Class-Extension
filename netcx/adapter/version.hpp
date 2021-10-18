// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#define INIT_GUID
#include <initguid.h>

#include <netfuncenum.h>

#include <NxXlat.hpp>

#include "NxPrivateGlobals.hpp"
#include "NxCollection.hpp"
#include "NxDevice.hpp"

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
        _In_ UNICODE_STRING const * RegistryPath
    );

    static void
    Destroy(
        _In_ WDFOBJECT Driver
    );

    NxCollection<NxDevice> &
    GetDeviceCollection(
        void
    );

    UNICODE_STRING const *
    GetRegistryPath(
        void
    ) const;

private:
    NxCollection<NxDevice>
        m_deviceCollection;

    KPoolPtr<UNICODE_STRING>
        m_registryPath;

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

