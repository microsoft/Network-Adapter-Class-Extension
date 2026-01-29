// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <wil/resource.h>
#ifdef _KERNEL_MODE
#include <ndis/lwminiportapi.h>
#endif

#include <FxObjectBase.hpp>

//
// The NxDriver is an object that represents a NetAdapterCx Client Driver
//

using unique_lwm_driver = wil::unique_any<NDIS_HANDLE, decltype(&::NdisLWMDeregisterMiniportDriver), NdisLWMDeregisterMiniportDriver>;

struct NX_PRIVATE_GLOBALS;

class NxDriver;

FORCEINLINE
NxDriver *
GetNxDriverFromWdfDriver(
    _In_ WDFDRIVER Driver
);

enum class NetCxDriverType : ULONG
{
    PnpDeviceDriver = 0,
    DevicelessDriver,
    MaxDriverTypes
};

class NxDriver
    : public CFxObject<WDFDRIVER, NxDriver, GetNxDriverFromWdfDriver, false>
{

private:

    NDIS_HANDLE
        m_ndisMiniportDriverHandle = nullptr;

    //
    // Lightweight miniport driver handle we get from NDIS
    // when client driver creates deviceless NETADAPTER
    //
    unique_lwm_driver
        m_ndisLwmDriverHandle;

    NX_PRIVATE_GLOBALS *
        m_privateGlobals = nullptr;

    NxDriver(
        _In_ WDFDRIVER Driver,
        _In_ NX_PRIVATE_GLOBALS * NxPrivateGlobals
    );

public:

    static EXECUTION_CONTEXT_RUNTIME_KNOBS DevicelessEcRuntimeKnobs;

    static
    NTSTATUS
    _CreateAndRegisterIfNeeded(
        _In_ NX_PRIVATE_GLOBALS * NxPrivateGlobals,
        _In_ NetCxDriverType DriverType
    );

    static
    NTSTATUS
    _CreateIfNeeded(
        _In_ WDFDRIVER Driver,
        _In_ NX_PRIVATE_GLOBALS * NxPrivateGlobals
    );

    NTSTATUS
    Register(
        void
    );

    void
    Deregister(
        void
    );

    NTSTATUS
    RegisterLwmDriver(
        void
    );

    NDIS_HANDLE
    GetNdisMiniportDriverHandle(
        void
    ) const;

    NDIS_HANDLE
    GetNdisLwmDriverHandle(
        void
    ) const;

    NX_PRIVATE_GLOBALS *
    GetPrivateGlobals(
        void
    ) const;
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxDriver, _GetNxDriverFromWdfDriver);

FORCEINLINE
NxDriver *
GetNxDriverFromWdfDriver(
    _In_ WDFDRIVER Driver
    )
/*++
Routine Description:

    This routine is just a wrapper around the _GetNxDriverFromWdfDriver function.
    To be able to define a the NxDriver class above, we need a forward declaration of the
    accessor function. Since _GetNxDriverFromWdfDriver is defined by Wdf, we dont want to
    assume a prototype of that function for the foward declaration.

--*/

{
    return _GetNxDriverFromWdfDriver(Driver);
}

