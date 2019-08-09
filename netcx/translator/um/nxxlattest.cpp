// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxXlatTest.tmh"
#include "NxXlatTest.hpp"
#include "NxNblDatapath.hpp"
#include "NxXlat.hpp"

extern "C"
NxTranslationAppFactory *
NxXlatTestAllocateFactory()
{
    auto appFactory = wil::make_unique_nothrow<NxTranslationAppFactory>();
    if (!appFactory)
    {
        return nullptr;
    }

    if (!NT_SUCCESS(appFactory->Initialize()))
    {
        return nullptr;
    }

    return appFactory.release();
}

extern "C"
NxNblDatapath *
NxXlatTestAllocateNblDispatcher()
{
    return new(std::nothrow) NxNblDatapath();
}

extern "C"
void
NxXlatTestFreeNblDispatcher(NxNblDatapath *dispatcher)
{
    delete dispatcher;
}

EXTERN_C BOOL WINAPI
DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD fdwReason,
    _In_ LPVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            WPP_INIT_TRACING(L"");
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            if (lpvReserved == nullptr)
            {
                WPP_CLEANUP();
            }
            break;
    }

    return TRUE;
}

VOID
__cdecl
WppHeapAutoLogStart(
    _In_ WPP_CB_TYPE * WppCb
)
{
    // Apparently someone forgot to implement this for non-UMDF drivers?
    UNREFERENCED_PARAMETER(WppCb);
}

