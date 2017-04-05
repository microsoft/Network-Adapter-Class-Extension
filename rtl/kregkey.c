// Copyright (C) Microsoft Corporation. All rights reserved.
#include "umwdm.h"

#include "KRegKey.h"

PAGED NTSTATUS 
KRegKey::Open(
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCWSTR Name,
    _In_opt_ HANDLE ParentKey)
{
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return Open(DesiredAccess, &temp, ParentKey);
}

PAGED NTSTATUS
KRegKey::Open(
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCUNICODE_STRING Name,
    _In_opt_ HANDLE ParentKey)
{
    HANDLE handle = nullptr;
    OBJECT_ATTRIBUTES ObjectAttributes;

#ifndef _KERNEL_MODE

    UNICODE_STRING RelativeName;
    if (ParentKey == nullptr)
    {
        ParentKey = TestHook_RootHandle;

        // If we're changing an abosolute path into a relative
        // path, we need to chop off the leading backslash.
        if (Name->Buffer[0] == L'\\')
        {
            RelativeName = *Name;
            RelativeName.Length -= sizeof(WCHAR);
            RelativeName.MaximumLength -= sizeof(WCHAR);
            RelativeName.Buffer++;
            Name = &RelativeName;
        }
    }

#endif

    InitializeObjectAttributes(
            &ObjectAttributes,
            const_cast<PUNICODE_STRING>(Name),
            OBJ_CASE_INSENSITIVE,
            ParentKey,
            nullptr);

#ifdef _KERNEL_MODE
    ObjectAttributes.Attributes |= OBJ_KERNEL_HANDLE;
#endif

    NTSTATUS ntstatus = ZwOpenKey(&handle, DesiredAccess, &ObjectAttributes);
    reset(reinterpret_cast<HANDLE*>(handle));

    return ntstatus;
}

PAGED NTSTATUS 
KRegKey::GetSubkeyName(
    _In_ ULONG index,
    _Inout_ KPtr<Rtl::KString> &name)
{
    PAGED_CODE();

    // 255 is the max length of a key name, per msdn.com/ms724872
    struct
    {
        KEY_BASIC_INFORMATION header;
        WCHAR buffer[256];
    } data;

    ULONG cbResult = 0;
    NTSTATUS ntstatus = ZwEnumerateKey(
            static_cast<HANDLE>(*this),
            index,
            KeyBasicInformation,
            &data,
            sizeof(data),
            &cbResult);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    if (data.header.NameLength > sizeof(data.buffer))
    {
        return STATUS_NAME_TOO_LONG;
    }

    // Force null-termination
    data.header.Name[data.header.NameLength / sizeof(WCHAR)] = L'\0';

    UNICODE_STRING us;
    us.Length = static_cast<USHORT>(data.header.NameLength);
    us.MaximumLength = us.Length + sizeof(WCHAR);
    us.Buffer = data.header.Name;
    
    name.reset(Rtl::KString::Initialize(&us));
    if (!name)
        return STATUS_INSUFFICIENT_RESOURCES;

    return STATUS_SUCCESS;
}

PAGED NTSTATUS 
KRegKey::QueryValueBoolean(
    _In_ PCWSTR Name,
    _Out_ BOOLEAN *Value,
    _In_ BooleanDisposition Disposition)
{
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return QueryValueBoolean(&temp, Value, Disposition);
}

PAGED NTSTATUS 
KRegKey::QueryValueBoolean(
    _In_ PCUNICODE_STRING Name,
    _Out_ BOOLEAN *Value,
    _In_ BooleanDisposition Disposition)
{
    ULONG UlongValue;
    NTSTATUS NtStatus = QueryValueUlong(Name, &UlongValue);
    if (!NT_SUCCESS(NtStatus))
    {
        if (NtStatus == STATUS_OBJECT_NAME_NOT_FOUND)
        {
            switch (Disposition)
            {
                case DefaultToFalse:
                    *Value = FALSE;
                    return STATUS_SUCCESS;
                case DefaultToTrue:
                    *Value = TRUE;
                    return STATUS_SUCCESS;
                default:
                    WIN_ASSERT(FALSE);
                    __fallthrough;
                case FailIfNotFound:
                    return NtStatus;
            }
        }
    }

    switch (UlongValue)
    {
        case 0:
            *Value = FALSE;
            break;
        case 1:
            *Value = TRUE;
            break;
        default:
            return STATUS_BAD_DATA;
    }

    return STATUS_SUCCESS;
}

PAGED NTSTATUS 
KRegKey::QueryValueUshort(
    _In_ PCWSTR Name,
    _Out_ USHORT *Value)
{
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return QueryValueUshort(&temp, Value);
}

PAGED NTSTATUS 
KRegKey::QueryValueUshort(
    _In_ PCUNICODE_STRING Name,
    _Out_ USHORT *Value)
{
    ULONG UlongValue;
    NTSTATUS NtStatus = QueryValueUlong(Name, &UlongValue);
    if (!NT_SUCCESS(NtStatus)) {
        return NtStatus;
    }

    if (UlongValue > MAXUSHORT) {
        return STATUS_INTEGER_OVERFLOW;
    }

    *Value = static_cast<USHORT>(UlongValue);

    return STATUS_SUCCESS;
}

PAGED NTSTATUS 
KRegKey::QueryValueUlong64(
    _In_ PCWSTR Name,
    _Out_ ULONG64 *Value)
{
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return QueryValueUlong64(&temp, Value);
}

PAGED NTSTATUS 
KRegKey::QueryValueUlong64(
    _In_ PCUNICODE_STRING Name,
    _Out_ ULONG64 *Value)
{
    NTSTATUS NtStatus;

    struct
    {
        KEY_VALUE_PARTIAL_INFORMATION Information;
        ULONG64 Buffer;
    } Data;

    ULONG BytesNeeded;    
    NtStatus = ZwQueryValueKey(
            *this,
            const_cast<PUNICODE_STRING>(Name),
            KeyValuePartialInformation,
            &Data,
            sizeof(Data),
            &BytesNeeded);
    if (!NT_SUCCESS(NtStatus))
        return NtStatus;

    if (Data.Information.Type != REG_QWORD)
        return STATUS_OBJECT_TYPE_MISMATCH;

    if (Data.Information.DataLength != sizeof(ULONG64))
        return STATUS_BUFFER_TOO_SMALL;

    *Value = *(ULONG64 UNALIGNED*)Data.Information.Data;

    return STATUS_SUCCESS;
}


PAGED NTSTATUS 
KRegKey::QueryValueUlong(
    _In_ PCWSTR Name,
    _Out_ ULONG *Value)
{
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return QueryValueUlong(&temp, Value);
}

PAGED NTSTATUS 
KRegKey::QueryValueUlong(
    _In_ PCUNICODE_STRING Name,
    _Out_ ULONG *Value)
{
    NTSTATUS NtStatus;

    struct
    {
        KEY_VALUE_PARTIAL_INFORMATION Information;
        ULONG Buffer;
    } Data;

    ULONG BytesNeeded;    
    NtStatus = ZwQueryValueKey(
            *this,
            const_cast<PUNICODE_STRING>(Name),
            KeyValuePartialInformation,
            &Data,
            sizeof(Data),
            &BytesNeeded);
    if (!NT_SUCCESS(NtStatus))
        return NtStatus;

    if (Data.Information.Type != REG_DWORD)
        return STATUS_OBJECT_TYPE_MISMATCH;

    if (Data.Information.DataLength != sizeof(ULONG))
        return STATUS_BUFFER_TOO_SMALL;

    *Value = *(ULONG UNALIGNED*)Data.Information.Data;

    return STATUS_SUCCESS;
}

PAGED NTSTATUS 
KRegKey::QueryValueString(
    _In_ PCWSTR Name,
    _Inout_ KPtr<Rtl::KString> &Value)
{
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return QueryValueString(&temp, Value);
}

PAGED NTSTATUS 
KRegKey::QueryValueString(
    _In_ PCUNICODE_STRING Name,
    _Inout_ KPtr<Rtl::KString> &Value)
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
        if (BytesNeeded > 1024 * 1024)
            return STATUS_IMPLEMENTATION_LIMIT;

        HeapBuffer.reset(new(std::nothrow, 'rtSR') UCHAR[BytesNeeded]);
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
    
    if (Information->Type != REG_SZ)
        return STATUS_OBJECT_TYPE_MISMATCH;

    if (Information->DataLength % sizeof(WCHAR))
        return STATUS_INVALID_PARAMETER;

    UNICODE_STRING String;
    String.Length = static_cast<USHORT>(Information->DataLength);
    String.MaximumLength = String.Length;
    String.Buffer = reinterpret_cast<PWCH>(Information->Data);

    while (String.Length >= sizeof(WCHAR) && 
        String.Buffer[String.Length / sizeof(WCHAR) - 1] == L'\0')
    {
        String.Length -= sizeof(WCHAR);
    }

    #pragma warning(suppress : 26015) // prefast doesn't understand DataLength
    Value.reset(Rtl::KString::Initialize(&String));
    if (!Value)
        return STATUS_INSUFFICIENT_RESOURCES;
    
    return STATUS_SUCCESS;
}

PAGED 
NTSTATUS 
KRegKey::QueryValueGuid(
    _In_ PCWSTR Name,
    _Out_ GUID *Value)
{
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return QueryValueGuid(&temp, Value);
}

PAGED 
NTSTATUS 
KRegKey::QueryValueGuid(
    _In_ PCUNICODE_STRING Name,
    _Out_ GUID *Value)
{
    return QueryValueBlob(Name, [&](UCHAR const *Buffer, ULONG Length)
    {
        if (Length != sizeof(GUID))
            return STATUS_INVALID_PARAMETER;

        RtlCopyMemory(Value, Buffer, sizeof(GUID));
        return STATUS_SUCCESS;
    });
}

PAGED
NTSTATUS
KRegKey::SetValueUlong(
    _In_ PCWSTR Name,
    _In_ ULONG Value)
{
    PAGED_CODE();
    
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return SetValueUlong(&temp, Value);
}

PAGED
NTSTATUS 
KRegKey::SetValueUlong(
    _In_ PCUNICODE_STRING Name,
    _In_ ULONG Value)
{
    PAGED_CODE();
    
    return ZwSetValueKey(
            *this,
            const_cast<PUNICODE_STRING>(Name),
            0,
            REG_DWORD,
            &Value,
            sizeof(Value));
}

PAGED
NTSTATUS 
KRegKey::SetValueBlob(
    _In_ PCWSTR Name,
    _In_ ULONG ValueLength,
    _In_reads_bytes_(ValueLength) UCHAR const *ValueBuffer)
{
    PAGED_CODE();
    
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return SetValueBlob(&temp, ValueLength, ValueBuffer);
}

PAGED
NTSTATUS 
KRegKey::SetValueBlob(
    _In_ PCUNICODE_STRING Name,
    _In_ ULONG ValueLength,
    _In_reads_bytes_(ValueLength) UCHAR const *ValueBuffer)
{
    PAGED_CODE();
    
    return ZwSetValueKey(
            *this,
            const_cast<PUNICODE_STRING>(Name),
            0,
            REG_BINARY,
            const_cast<UCHAR*>(ValueBuffer),
            ValueLength);
}

PAGED 
NTSTATUS 
KRegKey::DeleteValue(
    _In_ PCWSTR Name)
{
    PAGED_CODE();
    
    UNICODE_STRING temp;
    NTSTATUS ntstatus = RtlUnicodeStringInit(&temp, Name);
    if (!NT_SUCCESS(ntstatus))
        return ntstatus;

    return DeleteValue(&temp);
}

PAGED 
NTSTATUS 
KRegKey::DeleteValue(
    _In_ PCUNICODE_STRING Name)
{
    PAGED_CODE();

    return ZwDeleteValueKey(*this, const_cast<PUNICODE_STRING>(Name));
}


#ifndef _KERNEL_MODE

HANDLE KRegKey::TestHook_RootHandle = nullptr;

// static 
PAGED void 
KRegKey::TestHook_SetNewRoot(
    _In_opt_ HANDLE rootKey)
{
    TestHook_RootHandle = rootKey;
}

#endif


