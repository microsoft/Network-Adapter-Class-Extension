// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This contains definitions of the utility functions.

--*/

#pragma once

#include <FxObjectBase.hpp>
#include <KWaitEvent.h>

class AsyncResult
{
public:

    void
    Set(
        _In_ NTSTATUS Status
    );

    NTSTATUS
    Wait(
        void
    );

private:

    KAutoEvent
        m_signal;

    NTSTATUS
        m_status = STATUS_SUCCESS;
};

enum class DeviceState
{
    Initialized = 0,
    SelfManagedIoInitialized,
    Started,

    // In a framework initiated removal (orderly or surprise PnP remove)
    // the releasing of a device needs to be split in two phases. In phase
    // 1 NDIS unbinds and halts all the network adapters, in phase 2 we wait
    // until any outstanding reference on said adapters are released.
    //
    // ReleasingPhase1Pending and ReleasingPhase2Pending are used to keep track
    // of which phase is still pending execution so that we can do only what is
    // needed in case a client driver calls NetAdapterStop in the middle of a
    // release
    ReleasingPhase1Pending,
    ReleasingPhase2Pending,

    Released,
    Removed
};

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxWdfCollectionAddMultiSz(
    _Inout_ WDFCOLLECTION Collection,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * StringsAttributes,
    _In_reads_bytes_(BufferLength) PWCHAR MultiSzBuffer,
    _In_ ULONG BufferLength,
    _In_opt_ RECORDER_LOG RecorderLog
);

template<typename T>
void
NDIS_STATUS_INDICATION_INIT(
    _Out_ PNDIS_STATUS_INDICATION statusIndication,
    _In_ NDIS_HANDLE sourceHandle,
    NDIS_STATUS statusCode,
    _In_ T const * payload
)
{
    static_assert(!wistd::is_pointer<T>::value, "Status indication payload cannot be a pointer.");

    *statusIndication = { 0 };

    statusIndication->Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    statusIndication->Header.Size = NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
    statusIndication->Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;

    statusIndication->SourceHandle = sourceHandle;
    statusIndication->StatusCode = statusCode;
    statusIndication->StatusBuffer = static_cast<void *>(const_cast<T*>(payload));
    statusIndication->StatusBufferSize = sizeof(*payload);
}

template<typename T>
NDIS_STATUS_INDICATION
MakeNdisStatusIndication(
    _In_ NDIS_HANDLE sourceHandle,
    NDIS_STATUS statusCode,
    T const & payload
)
{
    NDIS_STATUS_INDICATION statusIndication;
    NDIS_STATUS_INDICATION_INIT(&statusIndication, sourceHandle, statusCode, &payload);

    return statusIndication;
}

inline
void
_WdfObjectDereference(
    _In_ WDFOBJECT Object
)
{
    WdfObjectDereference(Object);
}

template<typename TWdfHandle>
using unique_wdf_reference = wil::unique_any<TWdfHandle, decltype(&::_WdfObjectDereference), &::_WdfObjectDereference>;

template<typename TFxObject>
using unique_fx_ptr = wistd::unique_ptr<TFxObject, CFxObjectDeleter>;

NET_ADAPTER_DATAPATH_CALLBACKS
GetDefaultDatapathCallbacks(
    void
);

ULONG
inline
InterlockedIncrementU(
    _In_ ULONG volatile * Addend
)
{
    return static_cast<ULONG>(InterlockedIncrement(reinterpret_cast<LONG volatile*>(Addend)));
}

ULONG
inline
InterlockedDecrementU(
    _In_ ULONG volatile * Addend
)
{
    return static_cast<ULONG>(InterlockedDecrement(reinterpret_cast<LONG volatile*>(Addend)));
}

FORCEINLINE
void
InitializeListEntry(
    _Out_ PLIST_ENTRY ListEntry
    )
/*++

Routine Description:

    Initialize a list entry to NULL.

    - Using this improves catching list manipulation errors
    - This should not be called on a list head
    - Callers may depend on use of NULL value

--*/
{
    ListEntry->Flink = ListEntry->Blink = NULL;
}
