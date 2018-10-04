// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxDriver object.

--*/

#pragma once

#ifndef _KERNEL_MODE

#include <windows.h>
#include <wdf.h>
#include <wdfcx.h>
#include <wdfcxbase.h>

#include <WppRecorder.h>

#include "NdisUm.h"

#endif // _KERNEL_MODE

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

class NxDriver : public CFxObject<WDFDRIVER,
                                  NxDriver,
                                  GetNxDriverFromWdfDriver,
                                  false>
{
//friend class NxAdapter;

private:
    WDFDRIVER    m_Driver;
    RECORDER_LOG m_RecorderLog;
    NDIS_HANDLE  m_NdisMiniportDriverHandle;
    NX_PRIVATE_GLOBALS *m_PrivateGlobals;

    NxDriver(
        _In_ WDFDRIVER                Driver,
        _In_ NX_PRIVATE_GLOBALS *     NxPrivateGlobals
        );

public:
    static
    NTSTATUS
    _CreateAndRegisterIfNeeded(
        _In_ NX_PRIVATE_GLOBALS *           NxPrivateGlobals
        );

    static
    NTSTATUS
    _CreateIfNeeded(
        _In_ WDFDRIVER           Driver,
        _In_ NX_PRIVATE_GLOBALS * NxPrivateGlobals
        );

    NTSTATUS
    Register();

    static
    NDIS_STATUS
    _EvtNdisSetOptions(
        _In_  NDIS_HANDLE  NdisDriverHandle,
        _In_  NDIS_HANDLE  NxDriverAsContext
        );

    static
    VOID
    _EvtWdfCleanup(
        _In_  WDFOBJECT Driver
    );

    RECORDER_LOG
    GetRecorderLog() {
        return m_RecorderLog;
    }

    NDIS_HANDLE
    GetNdisMiniportDriverHandle() {
        return m_NdisMiniportDriverHandle;
    }

    NX_PRIVATE_GLOBALS *
    GetPrivateGlobals(
        void
        ) const;

    ~NxDriver();

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

