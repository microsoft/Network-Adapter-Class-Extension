// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This contains definitions of the utility functions.

--*/

#pragma once

#include <FxObjectBase.hpp>
#include <KWaitEvent.h>
#include <KArray.h>

#include "ExecutionContext.hpp"

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
    Releasing,
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
    _In_ ULONG BufferLength
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

using unique_file_object_reference = wistd::unique_ptr<FILE_OBJECT, wil::function_deleter<decltype(&::ObfDereferenceObject), &::ObfDereferenceObject>>;

struct unique_ptr_array_deleter
{
    template <typename T>
    void operator()(_Pre_opt_valid_ _Frees_ptr_opt_ T * RawPointer) const
    {
        wistd::unique_ptr<T []> uniquePtr{ RawPointer };
        uniquePtr.reset();
    }
};

template <typename T>
wil::unique_any_array_ptr<T, unique_ptr_array_deleter>
make_unique_array(
    _In_ size_t NumberOfElements
)
{
    auto allocation = wil::make_unique_nothrow<T []>(NumberOfElements);

    if (!allocation)
    {
        return {};
    }

    return { allocation.release(), NumberOfElements };
}

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

inline
LONG
NxInterlockedDecrementFloor(
    __inout LONG  volatile *Target,
    __in LONG Floor
    )
{
    LONG startVal;
    LONG currentVal;

    currentVal = *Target;

    do
    {
        if (currentVal <= Floor)
        {
            //
            // This value cannot be returned in the success path
            //
            return Floor - 1;
        }

        startVal = currentVal;

        //
        // currentVal will be the value that used to be Target if the exchange was made
        // or its current value if the exchange was not made.
        //
        currentVal = InterlockedCompareExchange(Target, startVal - 1, startVal);

        //
        // If startVal == currentVal, then no one updated Target in between the deref at the top
        // and the InterlockedCompareExchange afterward.
        //
    } while (startVal != currentVal);

    //
    // startVal is the old value of Target. Since InterlockedDecrement returns the new
    // decremented value of Target, we should do the same here.
    //
    return startVal - 1;
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
