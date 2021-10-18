// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <FxObjectBase.hpp>

//
// The NxDriver is an object that represents a NetAdapterCx Client Driver
//

struct NX_PRIVATE_GLOBALS;

class NxDriver;

FORCEINLINE
NxDriver *
GetNxDriverFromWdfDriver(
    _In_ WDFDRIVER Driver
);

class NxDriver
    : public CFxObject<WDFDRIVER, NxDriver, GetNxDriverFromWdfDriver, false>
{

private:

    NDIS_HANDLE
        m_ndisMiniportDriverHandle = nullptr;

    NX_PRIVATE_GLOBALS *
        m_privateGlobals = nullptr;

    NxDriver(
        _In_ WDFDRIVER Driver,
        _In_ NX_PRIVATE_GLOBALS * NxPrivateGlobals
    );

public:

    static
    NTSTATUS
    _CreateAndRegisterIfNeeded(
        _In_ NX_PRIVATE_GLOBALS * NxPrivateGlobals
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

    NDIS_HANDLE
    GetNdisMiniportDriverHandle(
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

