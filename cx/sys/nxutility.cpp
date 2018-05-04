// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains utility functions.

--*/

#include "Nx.hpp"

#include "NxUtility.tmh"

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
    )
/*++
Routine Description:


Arguments:

    Collection - Input collection to which the WDFSTRING objects are added.

--*/
{
    FuncEntry(FLAG_UTILITY);

    NTSTATUS status;
    UNICODE_STRING str;
    PWCHAR pCur;
    size_t totalCchLength, curCchLen;
    WDFSTRING wdfstring;
    ULONG  numberOfStringsAdded = 0;
    WDF_OBJECT_ATTRIBUTES attribs;

    pCur = (PWCHAR)MultiSzBuffer;
    status = STATUS_SUCCESS;

    if ((BufferLength % sizeof(WCHAR)) != 0) {
        LogError(RecorderLog, FLAG_UTILITY,
                 "Length %d is not a multiple of 2, STATUS_OBJECT_TYPE_MISMATCH", BufferLength);
        FuncExit(FLAG_UTILITY);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    if (BufferLength >= USHORT_MAX) {
        LogError(RecorderLog, FLAG_UTILITY,
                 "BufferLength %d is greater than USHORT_MAX", BufferLength);
        FuncExit(FLAG_UTILITY);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    totalCchLength = BufferLength/sizeof(WCHAR);

    if (totalCchLength < 2 ||
        pCur[totalCchLength-1] != UNICODE_NULL ||
        pCur[totalCchLength-2] != UNICODE_NULL) {

        LogError(RecorderLog, FLAG_UTILITY,
            "Buffer does not have double NULL terminal chars, STATUS_OBJECT_TYPE_MISMATCH");
        FuncExit(FLAG_UTILITY);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    //
    // If the client didn't specify any specific string attributes
    // use a local copy
    //
    if (StringsAttributes == WDF_NO_OBJECT_ATTRIBUTES) {
        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        StringsAttributes = &attribs;
    }

    //
    // If the client didn't specify a parent object explicitly
    // parent this to the Collection.
    //
    if (StringsAttributes->ParentObject == NULL) {
        StringsAttributes->ParentObject = Collection;
    }

    #pragma prefast(suppress:26000, "Bug in SAL Checker")
    while (*pCur != UNICODE_NULL) {

        curCchLen = wcslen(pCur);

        str.Buffer = pCur;
        str.Length = (USHORT) (curCchLen * sizeof(WCHAR));
        str.MaximumLength = (USHORT) (curCchLen * sizeof(WCHAR));

        LogVerbose(RecorderLog, FLAG_UTILITY,
                   "Multi Sz includes string %wZ", &str);

        status = WdfStringCreate(&str, StringsAttributes, &wdfstring);

        if (!NT_SUCCESS(status)) {
            LogError(RecorderLog, FLAG_UTILITY,
                     "The WdfStringCreate failed, %!STATUS!", status);
            goto Exit;
        }

        status = WdfCollectionAdd(Collection, wdfstring);

        if (!NT_SUCCESS(status)) {






            WdfObjectDelete(wdfstring);
            LogError(RecorderLog, FLAG_UTILITY,
                     "The WdfCollectionAdd failed, %!STATUS!", status);
            goto Exit;
        }

        numberOfStringsAdded++;

        //
        // Increment to the next string in the multi sz (length of string +
        // 1 for the NULL)
        //
        pCur += curCchLen + 1;
    }

Exit:

    if (!NT_SUCCESS(status)) {

        //
        // In the case of a failure, remove the entries that we added
        // to the Collection
        //
        while (numberOfStringsAdded != 0) {






            WDFOBJECT str;
            str = WdfCollectionGetLastItem(Collection);
            WdfCollectionRemove(Collection, str);
            WdfObjectDelete(str);
            numberOfStringsAdded--;
        }
    }

    FuncExit(FLAG_UTILITY);
    return status;
}


