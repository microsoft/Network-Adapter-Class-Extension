// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

typedef
NTSTATUS
DeviceResetHandler(
    _In_ void * InterfaceContext,
    _In_ DEVICE_RESET_TYPE ResetType,
    _In_ ULONG Flags,
    _In_opt_ void * ResetParameters
);

typedef void InterfaceDereference(
    void *Context
);

#if NETCX_UMDF
typedef
NTSTATUS
(*PDBGK_LIVEDUMP_ADDSECONDARYDATA_ROUTINE)(
    // Report handle passed to the live dump callback routine
    _In_ HANDLE ReportHandle,
    // GUID to identify the secondary data
    _In_reads_bytes_(sizeof(GUID)) LPCGUID Identifier,
    // Virtual address of data to add
    _In_reads_bytes_(Size) PVOID Data,
    // Size of data to add
    _In_ ULONG Size
);

typedef
NTSTATUS
DBGK_LIVEDUMP_CALLBACK_ROUTINE(
    // An opaque handle to the WER report
    _In_ HANDLE ReportHandle,
    // Function pointer for a routine to add secondary data to the live dump
    _In_ PDBGK_LIVEDUMP_ADDSECONDARYDATA_ROUTINE AddSecondaryDataRoutine,
    // Bugcheck Code and Parameters
    _In_ ULONG BugCheckCode,
    _In_opt_ ULONG_PTR P1,
    _In_opt_ ULONG_PTR P2,
    _In_opt_ ULONG_PTR P3,
    _In_opt_ ULONG_PTR P4,
    // Caller supplied context
    _In_opt_ PVOID CallbackContext
);
#endif

/*
This class abstracts DEVICE_RESET_INTERFACE_STANDARD interaction from the caller.
In KMDF it directly queries the device interface.
In UMDF it uses netcxrd to query and interact with the device interface.
*/
class ResetInterface
{
public:

    _IRQL_requires_(PASSIVE_LEVEL)
    ~ResetInterface(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    void
    QueryDeviceResetInterface(
        _In_ WDFDEVICE Device
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    GetSupportedDeviceResetType(
        _Inout_ ULONG * SupportedResetTypes
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool
    DeviceResetTypeSupported(
        _In_ DEVICE_RESET_TYPE ResetType
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    DeviceReset(
        _In_ DEVICE_RESET_TYPE ResetType
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    CaptureLiveKernelDump(
        _In_ WDFDRIVER Driver,
        _In_ WDFDEVICE Device,
        _In_ void * Context,
        _In_ DBGK_LIVEDUMP_CALLBACK_ROUTINE *Callback
    ) const;

private:

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    TriggerPldr(
        void
    ) const;

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS
    TriggerFldr(
        void
    ) const;

private:

    bool
        m_deviceResetSupported{ false };

    ULONG
        m_supportedResetTypes{ 0 };

    void *
        m_resetInterfaceContext{ nullptr };

    DeviceResetHandler *
        m_resetHandler{ nullptr };

    InterfaceDereference *
        m_interfaceDereferenceHandler{ nullptr };
};
