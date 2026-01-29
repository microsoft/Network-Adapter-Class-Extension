// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#ifdef NETCX_KMDF
#include <KArray.h>
#include <KPtr.h>
#endif
#include <NxPrivateGlobals.hpp>

struct _NET_MEMORY_MDL_WRAPPER;
typedef _NET_MEMORY_MDL_WRAPPER NET_MEMORY_MDL_WRAPPER;

class NxMemory
{
public:
#ifdef NETCX_KMDF
    NxMemory(
        _In_ NX_PRIVATE_GLOBALS const* DriverGlobals
    );
#else
    NxMemory(
        _In_ NX_PRIVATE_GLOBALS const* DriverGlobals,
        _In_ wil::unique_hfile && RdMemoryHandle
    );

    static
    NTSTATUS
    OpenRdMemoryHandle(
        _In_ WDFDEVICE Device,
        _Out_ wil::unique_hfile & handle);
#endif

    ~NxMemory();

    NTSTATUS
    Initialize(
        _In_ const NET_MEMORY_COLLECTION_CONFIG* MemoryCollectionConfiguration
    );

#ifdef NETCX_UMDF
    HANDLE
    GetRdMemoryHandle(
        void
    ) const;
#endif

    NTSTATUS
    NetMemoryCreate(
        _In_ const NET_MEMORY_CONFIG * Configuration,
        _Out_ NET_MEMORY_ID * MemoryId
    );

    void
    NetMemoryDestroy(
        _In_ NET_MEMORY_ID MemoryId
    );

    NET_MEMORY_MDL_WRAPPER*
    GetMdlForDriverAllocatedMemory(
        _In_ NET_MEMORY_ID MemoryId
    );

private:
    NX_PRIVATE_GLOBALS const*
        m_driverGlobals = nullptr;

    volatile LONG64
        m_buffersOutstanding = 0;

#ifdef NETCX_UMDF
    wil::unique_hfile
        m_rdMemoryHandle;
#endif
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxMemory, GetNxMemoryFromHandle);
