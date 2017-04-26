/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxUtlity.hpp

Abstract:

    This contains definitions of the utility functions. 





Environment:

    kernel mode only

Revision History:

--*/

#pragma once

#include <wil\wistd_type_traits.h>
#include <wil\resource.h>

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxWdfCollectionAddMultiSz(
    _Inout_  WDFCOLLECTION                         Collection,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES                StringsAttributes,
    _In_reads_bytes_(BufferLength)
             PWCHAR                                MultiSzBuffer,
    _In_     ULONG                                 BufferLength,
    _In_opt_ RECORDER_LOG                          RecorderLog
    );

template<typename T>
void
NDIS_STATUS_INDICATION_INIT(
    _Out_ PNDIS_STATUS_INDICATION statusIndication,
    _In_ NDIS_HANDLE sourceHandle,
    NDIS_STATUS statusCode,
    _In_ T const * payload)
{
    static_assert(!wistd::is_pointer<T>::value, "Status indication payload cannot be a pointer.");

    *statusIndication = { 0 };

    statusIndication->Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    statusIndication->Header.Size = NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
    statusIndication->Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;

    statusIndication->SourceHandle = sourceHandle;
    statusIndication->StatusCode = statusCode;
    statusIndication->StatusBuffer = static_cast<PVOID>(const_cast<T*>(payload));
    statusIndication->StatusBufferSize = sizeof(*payload);
}

template<typename T>
NDIS_STATUS_INDICATION
MakeNdisStatusIndication(_In_ NDIS_HANDLE sourceHandle, NDIS_STATUS statusCode, T const & payload)
{
    NDIS_STATUS_INDICATION statusIndication;
    NDIS_STATUS_INDICATION_INIT(&statusIndication, sourceHandle, statusCode, &payload);

    return statusIndication;
}

template<typename TWdfHandle>
using unique_wdf_object = wil::unique_any<TWdfHandle, decltype(&::WdfObjectDelete), &::WdfObjectDelete>;

inline
void
_WdfObjectDereference(_In_ WDFOBJECT Object)
{
    WdfObjectDereference(Object);
}

template<typename TWdfHandle>
using unique_wdf_reference = wil::unique_any<TWdfHandle, decltype(&::_WdfObjectDereference), &::_WdfObjectDereference>;

template<typename TFxObject>
using unique_fx_ptr = wistd::unique_ptr<TFxObject, CFxObjectDeleter>;

// unique_wdf_object specializations
using unique_wdf_memory = unique_wdf_object<WDFMEMORY>;
using unique_wdf_common_buffer = unique_wdf_object<WDFCOMMONBUFFER>;
