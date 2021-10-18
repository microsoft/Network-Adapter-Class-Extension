// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the definition of the NxAdapter Configuration object.

--*/

#pragma once

#include <FxObjectBase.hpp>

//
// The NxConfiguration is an object that represents a Net Configuration
//

struct NX_PRIVATE_GLOBALS;

class NxDevice;
class NxAdapter;
class NxConfiguration;

FORCEINLINE
NxConfiguration *
GetNxConfigurationFromHandle(
    _In_ NETCONFIGURATION Configuration
);

class NxConfiguration
    : public CFxObject<NETCONFIGURATION, NxConfiguration, GetNxConfigurationFromHandle, false>
{

private:
    NxConfiguration *
        m_parentNxConfiguration;

public:
    bool const
        m_isDeviceConfig;

    NxDevice *
        m_device;

    NxAdapter *
        m_adapter;

    WDFKEY
        m_key;

    //
    // Opaque handle returned by ndis.sys for this adapter.
    //
    NDIS_HANDLE
        m_ndisConfigurationHandle;

private:
    NxConfiguration(
        _In_opt_ NX_PRIVATE_GLOBALS * NxPrivateGlobals,
        _In_ NETCONFIGURATION Configuration,
        _In_ NxConfiguration * ParentNxConfiguration,
        _In_ WDFDEVICE Device
    );

    NxConfiguration(
        _In_opt_ NX_PRIVATE_GLOBALS * NxPrivateGlobals,
        _In_ NETCONFIGURATION Configuration,
        _In_ NxConfiguration * ParentNxConfiguration,
        _In_ NETADAPTER Adapter
    );

    PAGED
    NTSTATUS
    ReadConfiguration(
        _Out_ PNDIS_CONFIGURATION_PARAMETER * ParameterValue,
        _In_ PCUNICODE_STRING Keyword,
        _In_ NDIS_PARAMETER_TYPE ParameterType
    );

public:

    ~NxConfiguration();

    template <typename T>
    static
    NTSTATUS
    _Create(
        _In_opt_ NX_PRIVATE_GLOBALS * PrivateGlobals,
        _In_ T Object,
        _In_opt_ NxConfiguration * ParentNxConfiguration,
        _Out_ NxConfiguration ** NxConfiguration
    );

    static
    void
    _EvtCleanup(
        _In_ WDFOBJECT Configuration
    );

    NTSTATUS
    Open(
        void
    );

    NTSTATUS
    OpenAsSubConfiguration(
        PCUNICODE_STRING SubConfigurationName
    );

    void
    DeleteFromFailedOpen(
        void
    );

    void
    Close(
        void
    );

    NTSTATUS
    AddAttributes(
        _In_ WDF_OBJECT_ATTRIBUTES * Attributes
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    QueryUlong(
        _In_ NET_CONFIGURATION_QUERY_ULONG_FLAGS Flags,
        _In_ PCUNICODE_STRING ValueName,
        _Out_ PULONG Value
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    QueryString(
        _In_ PCUNICODE_STRING ValueName,
        _In_opt_ WDF_OBJECT_ATTRIBUTES * StringAttributes,
        _Out_ WDFSTRING * WdfString
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    QueryMultiString(
        _In_ PCUNICODE_STRING ValueName,
        _In_opt_ WDF_OBJECT_ATTRIBUTES * StringsAttributes,
        _In_ WDFCOLLECTION Collection
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    QueryBinary(
        _In_ PCUNICODE_STRING ValueName,
        _Strict_type_match_ _In_ POOL_TYPE PoolType,
        _In_opt_ WDF_OBJECT_ATTRIBUTES * MemoryAttributes,
        _Out_ WDFMEMORY * WdfMemory
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    QueryLinkLayerAddress(
        _Out_ NET_ADAPTER_LINK_LAYER_ADDRESS * LinkLayerAddress
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    AssignUlong(
        _In_ PCUNICODE_STRING ValueName,
        _In_ ULONG Value
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    AssignUnicodeString(
        _In_ PCUNICODE_STRING ValueName,
        _In_ PCUNICODE_STRING Value
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    AssignBinary(
        _In_ PCUNICODE_STRING ValueName,
        _In_reads_bytes_(BufferLength) void * Buffer,
        _In_ ULONG BufferLength
    );

    _Must_inspect_result_
    _IRQL_requires_max_(PASSIVE_LEVEL)
    NTSTATUS
    AssignMultiString(
        _In_ PCUNICODE_STRING ValueName,
        _In_ WDFCOLLECTION StringsCollection
    );

};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxConfiguration, _GetNxConfigurationFromHandle);

FORCEINLINE
NxConfiguration *
GetNxConfigurationFromHandle(
    _In_ NETCONFIGURATION Configuration
    )
/*++
Routine Description:

    This routine is just a wrapper around the _GetNxConfigurationFromHandle function.
    To be able to define a the NxConfiguration class above, we need a forward declaration of the
    accessor function. Since _GetNxConfigurationFromHandle is defined by Wdf, we dont want to
    assume a prototype of that function for the foward declaration.

--*/

{
    return _GetNxConfigurationFromHandle(Configuration);
}

template <typename T>
NTSTATUS
NxConfiguration::_Create(
    _In_opt_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ T Object,
    _In_opt_ NxConfiguration * ParentNxConfiguration,
    _Out_ NxConfiguration ** NxConfigurationArg
)
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxConfiguration);
    if (ParentNxConfiguration != NULL)
    {
        attributes.ParentObject = ParentNxConfiguration->GetFxObject();
    }
    else
    {
        attributes.ParentObject = Object;
    }

    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxConfiguration::_EvtCleanup;

    //
    // Ensure that the destructor would be called when this object is destroyed.
    //
    NxConfiguration::_SetObjectAttributes(&attributes);

    NETCONFIGURATION netConfiguration;
    NTSTATUS status = WdfObjectCreate(&attributes, (WDFOBJECT*)&netConfiguration);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Since we just created the NetAdapter, the NxConfiguration object has
    // yet not been constructed. Get the nxConfiguration's memory.
    //
    void * nxConfigurationMemory = GetNxConfigurationFromHandle(netConfiguration);

    //
    // Use the inplacement new and invoke the constructor on the
    // NxConfiguration's memory
    //
    auto nxConfiguration = new (nxConfigurationMemory) NxConfiguration(PrivateGlobals,
        netConfiguration,
        ParentNxConfiguration,
        Object);

    __analysis_assume(nxConfiguration != NULL);

    NT_ASSERT(nxConfiguration);

    *NxConfigurationArg = nxConfiguration;

    return STATUS_SUCCESS;
}

