/*++
Copyright (c) Microsoft Corporation

Module Name:

    KRegKey.h

Abstract:

    Implements KRegKey, a class that automatically manages the lifetime
    of registry handles, and simplifies common registry operations

Environment:

    Kernel mode or usermode unittest

--*/

#pragma once

#include "KMacros.h"
#include "KPtr.h"
#include "KString.h"
#include <ntstrsafe.h>
#include <wil\resource.h>

#ifdef _KERNEL_MODE
#define CloseRegistryHandle ZwClose
#else
inline void CloseRegistryHandle(HANDLE h)
{
    RegCloseKey(reinterpret_cast<HKEY>(h));
}
#endif

using unique_reg_key = wil::unique_any<HANDLE, decltype(&::CloseRegistryHandle), &::CloseRegistryHandle>;

class KRegKey : public unique_reg_key
{
public:

    PAGED KRegKey() { }

#ifndef _KERNEL_MODE

    PAGED static void TestHook_SetNewRoot(
        _In_opt_ HANDLE rootKey);

#endif

    PAGED NTSTATUS Open(
        _In_ ACCESS_MASK DesiredAccess,
        _In_ PCWSTR Name,
        _In_opt_ HANDLE ParentKey = nullptr);

    PAGED NTSTATUS Open(
        _In_ ACCESS_MASK DesiredAccess,
        _In_ PCUNICODE_STRING Name,
        _In_opt_ HANDLE ParentKey = nullptr);

    enum BooleanDisposition { FailIfNotFound, DefaultToFalse, DefaultToTrue };

    PAGED NTSTATUS GetSubkeyName(
        _In_ ULONG index,
        _Inout_ KPtr<Rtl::KString> &name);

    PAGED NTSTATUS QueryValueBoolean(
        _In_ PCWSTR Name,
        _Out_ BOOLEAN *Value,
        _In_ BooleanDisposition Disposition = FailIfNotFound);

    PAGED NTSTATUS QueryValueBoolean(
        _In_ PCUNICODE_STRING Name,
        _Out_ BOOLEAN *Value,
        _In_ BooleanDisposition Disposition = FailIfNotFound);

    PAGED NTSTATUS QueryValueUlong(
        _In_ PCWSTR Name,
        _Out_ ULONG *Value);

    PAGED NTSTATUS QueryValueUlong(
        _In_ PCUNICODE_STRING Name,
        _Out_ ULONG *Value);

    PAGED NTSTATUS QueryValueUshort(
        _In_ PCWSTR Name,
        _Out_ USHORT *Value);

    PAGED NTSTATUS QueryValueUshort(
        _In_ PCUNICODE_STRING Name,
        _Out_ USHORT *Value);

    PAGED NTSTATUS QueryValueUlong64(
        _In_ PCWSTR Name,
        _Out_ ULONG64 *Value);

    PAGED NTSTATUS QueryValueUlong64(
        _In_ PCUNICODE_STRING Name,
        _Out_ ULONG64 *Value);

    PAGED NTSTATUS QueryValueString(
        _In_ PCWSTR Name,
        _Inout_ KPtr<Rtl::KString> &Value);

    PAGED NTSTATUS QueryValueString(
        _In_ PCUNICODE_STRING Name,
        _Inout_ KPtr<Rtl::KString> &Value);

    PAGED NTSTATUS QueryValueGuid(
        _In_ PCWSTR Name,
        _Out_ GUID *Value);

    PAGED NTSTATUS QueryValueGuid(
        _In_ PCUNICODE_STRING Name,
        _Out_ GUID *Value);
    
    template <typename Lambda>    
    PAGED NTSTATUS QueryValueBlob(
        _In_ PCWSTR Name,
        _In_ Lambda Callback)
    {
        UNICODE_STRING temp;
        NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
        if (!NT_SUCCESS(ntstatus))
            return ntstatus;

        return QueryValueBlob(&temp, Callback);
    }

    template <typename Lambda>    
    PAGED NTSTATUS QueryValueBlob(
        _In_ PCUNICODE_STRING Name,
        _In_ Lambda Callback)
    {
        NTSTATUS NtStatus;

        union
        {
            KEY_VALUE_PARTIAL_INFORMATION StackInformation;
            UCHAR Buffer[256];
        };

        KEY_VALUE_PARTIAL_INFORMATION *Information = &StackInformation;
        wistd::unique_ptr<UCHAR[]> HeapBuffer;

        ULONG BytesNeeded;    
        NtStatus = ZwQueryValueKey(
                *this,
                const_cast<PUNICODE_STRING>(Name),
                KeyValuePartialInformation,
                Buffer,
                sizeof(Buffer),
                &BytesNeeded);
        if (NtStatus == STATUS_BUFFER_OVERFLOW)
        {
            HeapBuffer.reset(new(std::nothrow, 'niBR') UCHAR[BytesNeeded]);
            if (!HeapBuffer)
                return STATUS_INSUFFICIENT_RESOURCES;

            NtStatus = ZwQueryValueKey(
                    *this,
                    const_cast<PUNICODE_STRING>(Name),
                    KeyValuePartialInformation,
                    HeapBuffer.get(),
                    BytesNeeded,
                    &BytesNeeded);
            if (!NT_SUCCESS(NtStatus))
                return NtStatus;

            Information = reinterpret_cast<KEY_VALUE_PARTIAL_INFORMATION*>(HeapBuffer.get());
        }
        else if (!NT_SUCCESS(NtStatus))
        {
            return NtStatus;
        }

        if (Information->Type != REG_BINARY)
            return STATUS_OBJECT_TYPE_MISMATCH;

        return Callback(Information->Data, Information->DataLength);
    }

    template <typename CountLambda, typename PerStringLambda>    
    PAGED NTSTATUS QueryValueMultisz(
        _In_ PCWSTR Name,
        _In_ CountLambda CountCallback,
        _In_ PerStringLambda PerStringCallback)
    {
        UNICODE_STRING temp;
        NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
        if (!NT_SUCCESS(ntstatus))
            return ntstatus;

        return QueryValueMultisz(&temp, CountCallback, PerStringCallback);
    }

    template <typename CountLambda, typename PerStringLambda>    
    PAGED NTSTATUS QueryValueMultisz(
        _In_ PCUNICODE_STRING Name,
        _In_ CountLambda CountCallback,
        _In_ PerStringLambda PerStringCallback)
    {
        NTSTATUS NtStatus;

        union
        {
            KEY_VALUE_PARTIAL_INFORMATION StackInformation;
            UCHAR Buffer[256];
        };

        KEY_VALUE_PARTIAL_INFORMATION *Information = &StackInformation;
        wistd::unique_ptr<UCHAR[]> HeapBuffer;

        ULONG BytesNeeded;    
        NtStatus = ZwQueryValueKey(
                *this,
                const_cast<PUNICODE_STRING>(Name),
                KeyValuePartialInformation,
                Buffer,
                sizeof(Buffer),
                &BytesNeeded);
        if (NtStatus == STATUS_BUFFER_OVERFLOW)
        {
            HeapBuffer.reset(new(std::nothrow, 'zSlM') UCHAR[BytesNeeded]);
            if (!HeapBuffer)
                return STATUS_INSUFFICIENT_RESOURCES;

            NtStatus = ZwQueryValueKey(
                    *this,
                    const_cast<PUNICODE_STRING>(Name),
                    KeyValuePartialInformation,
                    HeapBuffer.get(),
                    BytesNeeded,
                    &BytesNeeded);
            if (!NT_SUCCESS(NtStatus))
                return NtStatus;

            Information = reinterpret_cast<KEY_VALUE_PARTIAL_INFORMATION*>(HeapBuffer.get());
        }
        else if (!NT_SUCCESS(NtStatus))
        {
            return NtStatus;
        }

        if (Information->Type != REG_MULTI_SZ)
            return STATUS_OBJECT_TYPE_MISMATCH;

        if (Information->DataLength % sizeof(WCHAR))
            return STATUS_INVALID_PARAMETER;

        PCWSTR CurrentStart = (PCWSTR)Information->Data;
        PCWSTR End = (PCWSTR)(Information->Data + Information->DataLength);

        ULONG NumStrings = 0;
        while (true)
        {
            if (CurrentStart >= End)
                return STATUS_BUFFER_TOO_SMALL;

            if (*CurrentStart == L'\0')
                break;

            PCWSTR CurrentEnd = CurrentStart;
            do
            {
                ++CurrentEnd;

                if (CurrentEnd >= End)
                    return STATUS_BUFFER_TOO_SMALL;

            } while (*CurrentEnd != L'\0');

            ++NumStrings;

            CurrentStart = CurrentEnd + 1;
        }

        NtStatus = CountCallback(NumStrings);
        if (!NT_SUCCESS(NtStatus))
            return NtStatus;

        CurrentStart = (PCWSTR)Information->Data;
        ULONG Index = 0;
        while (true)
        {
            if (*CurrentStart == L'\0')
                return STATUS_SUCCESS;
        
            PCWSTR CurrentEnd = CurrentStart;
            do
            {
                ++CurrentEnd;
            } while (*CurrentEnd != L'\0');
        
            NtStatus = PerStringCallback(CurrentStart, Index);
            if (!NT_SUCCESS(NtStatus))
                return NtStatus;

            CurrentStart = CurrentEnd + 1;
            Index++;
        }

    }

    PAGED NTSTATUS SetValueUlong(
        _In_ PCWSTR Name,
        _In_ ULONG Value);

    PAGED NTSTATUS SetValueUlong(
        _In_ PCUNICODE_STRING Name,
        _In_ ULONG Value);

    PAGED NTSTATUS SetValueBlob(
        _In_ PCWSTR Name,
        _In_ ULONG ValueLength,
        _In_reads_bytes_(ValueLength) UCHAR const *ValueBuffer);

    PAGED NTSTATUS SetValueBlob(
        _In_ PCUNICODE_STRING Name,
        _In_ ULONG ValueLength,
        _In_reads_bytes_(ValueLength) UCHAR const *ValueBuffer);

    PAGED NTSTATUS DeleteValue(
        _In_ PCWSTR Name);

    PAGED NTSTATUS DeleteValue(
        _In_ PCUNICODE_STRING Name);

    PAGED operator bool() const { return is_valid(); }

    PAGED operator HANDLE() const { return reinterpret_cast<HANDLE>(this->get()); }

#ifndef _KERNEL_MODE
    PAGED operator HKEY() const { return reinterpret_cast<HKEY>(this->get()); }

    static HANDLE TestHook_RootHandle;
#endif
};
