// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#define INIT_GUID
#include <initguid.h>

#include <netfuncenum.h>

#include <NxXlat.hpp>

#include "NxPrivateGlobals.hpp"
#include "NxCollection.hpp"
#include "NxDevice.hpp"

#include <KLoaderTypes.h>

#ifdef NETCX_UMDF
#include <AlpcIncludes.h>
#include <AlpcServer.hpp>

class CanaryIoctlFactory;

#endif

#ifdef _KERNEL_MODE
#include <KLoaderApi.h>

using kloader_module_reference_ptr = wil::unique_any<KLOADER_MODULE_REFERENCE, decltype(KLoaderDereferenceModule), KLoaderDereferenceModule>;
#endif

class CxDriverContext
{
public:
#ifdef NETCX_KMDF
    //
    // Built-in datapath apps: NBL translator
    //
    NxTranslationAppFactory
        TranslationAppFactory;
#endif

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

    KLOADER_DISPATCH_TABLE const *
    GetExecutionContextDispatchTable(
        void
    ) const;

#ifdef NETCX_UMDF
    NTSTATUS
    SendCanaryIoctl(
        _In_ HANDLE Handle,
        _In_ DWORD IoControlCode,
        _Out_ wistd::unique_ptr<CanaryIoctl> & Ioctl
    );
#endif

private:

    NTSTATUS
    InitializeExecutionContextModule(
        void
    );

private:
    NxCollection<NxDevice>
        m_deviceCollection;

    KPoolPtr<UNICODE_STRING>
        m_registryPath;

#ifdef NETCX_UMDF
    AlpcServer
        m_alpcServer;

    wistd::unique_ptr<CanaryIoctlFactory>
        m_canaryIoctlFactory;
#endif

#ifdef NETCX_KMDF
    kloader_module_reference_ptr
        m_executionContextModule{};
#endif

    KLOADER_DISPATCH_TABLE const *
        m_executionContextDispatch{ nullptr };
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

