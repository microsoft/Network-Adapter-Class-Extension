// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This module contains the "C" interface for the NxConfiguration object.

--*/

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxConfigurationApi.tmh"

#include "NxConfiguration.hpp"
#include "verifier.hpp"
#include "version.hpp"

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
VOID
NETEXPORT(NetConfigurationClose)(
    _In_ PNET_DRIVER_GLOBALS               Globals,
    _In_ NETCONFIGURATION                  Configuration
    )
/*++
Routine Description:

    This method is called by the clients to close the configuration it had
    previously opened.

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
    in a prior call to NdisCxAdapterOpenConfiguration or
    NetConfigurationOpenSubConfiguration

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    GetNxConfigurationFromHandle(Configuration)->Close();

    return;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationOpenSubConfiguration)(
    _In_     PNET_DRIVER_GLOBALS        Globals,
    _In_     NETCONFIGURATION           Configuration,
    _In_     PCUNICODE_STRING           SubConfigurationName,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES     SubConfigurationAttributes,
    _Out_    NETCONFIGURATION*          SubConfiguration
    )
/*++
Routine Description:

    This method is called by the clients to open a sub configuration of
    a given configuration.

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    SubConfigurationName - Name of the Sub Configuration

    SubConfigurationAttributes - Optional WDF_OBJECT_ATTRIBUTES for the
        sub config. The client can only specify attributes fields allowed by
        WdfObjectAllocateContext API.

    SubConfiguration - Pointer to an address in which the handle of the
        sub configuration is returned.

--*/
{
    NTSTATUS                 status;

    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);


    *SubConfiguration = NULL;

    NxConfiguration * subNxConfiguration;
    auto nxConfiguration = GetNxConfigurationFromHandle(Configuration);
    status = NxConfiguration::_Create(pNxPrivateGlobals,
                                      nxConfiguration->m_NxAdapter,
                                      nxConfiguration,
                                      &subNxConfiguration);
    if (!NT_SUCCESS(status)){
        return status;
    }

    status = subNxConfiguration->OpenAsSubConfiguration(SubConfigurationName);

    if (!NT_SUCCESS(status)){
        subNxConfiguration->DeleteFromFailedOpen();
        return status;
    }

    //
    // Add The Client's Attributes as the last step, no failure after that point
    //
    if (SubConfigurationAttributes != WDF_NO_OBJECT_ATTRIBUTES){

        status = subNxConfiguration->AddAttributes(SubConfigurationAttributes);

        if (!NT_SUCCESS(status)){
            subNxConfiguration->Close();
            return status;
        }
    }

    //
    // *** NOTE: NO FAILURE AFTER THIS. **
    // We dont want clients Cleanup/Destroy callbacks
    // to be called unless this call succeeds.
    //
    *SubConfiguration = subNxConfiguration->GetFxObject();
    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryUlong)(
    _In_  PNET_DRIVER_GLOBALS                   Globals,
    _In_  NETCONFIGURATION                      Configuration,
    _In_  NET_CONFIGURATION_QUERY_ULONG_FLAGS   Flags,
    _In_  PCUNICODE_STRING                      ValueName,
    _Out_ PULONG                                Value
    )
/*++
Routine Description:

    This method is queries a Ulong value from the Adapter Configuration

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    Flags - A value defined in the NET_CONFIGURATION_ULONG_FLAGS enum

    ValueName - Name of the Ulong to be querried.

    Value - Address of the memory where the querried ULONG would be written

--*/
{
    NTSTATUS status;

    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyQueryAsUlongFlags(pNxPrivateGlobals, Flags);

    auto nxConfiguration = GetNxConfigurationFromHandle(Configuration);

    switch (Flags) {
    case NET_CONFIGURATION_QUERY_ULONG_NO_FLAGS:
    case NET_CONFIGURATION_QUERY_ULONG_MAY_BE_STORED_AS_HEX_STRING:
        break;
    default:
        status = STATUS_INVALID_PARAMETER;
        LogError(nxConfiguration->GetRecorderLog(), FLAG_CONFIGURATION,
                 "Invalid Flags Value %!NET_CONFIGURATION_QUERY_ULONG_FLAGS!", Flags);
        return status;
    }

    status = nxConfiguration->QueryUlong(Flags, ValueName, Value);

    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryString)(
    _In_     PNET_DRIVER_GLOBALS                   Globals,
    _In_     NETCONFIGURATION                      Configuration,
    _In_     PCUNICODE_STRING                      ValueName,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES                StringAttributes,
    _Out_    WDFSTRING*                            WdfString
    )
/*++
Routine Description:

    This method is queries a String value from the Adapter Configuration

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    ValueName - Name of the String to be querried.

    StringAttributes - optional WDF_OBJECT_ATTRIBUTES for the string

    WdfString - Output WDFSTRING object.

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    return GetNxConfigurationFromHandle(Configuration)->QueryString(ValueName, StringAttributes, WdfString);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryMultiString)(
    _In_     PNET_DRIVER_GLOBALS                   Globals,
    _In_     NETCONFIGURATION                      Configuration,
    _In_     PCUNICODE_STRING                      ValueName,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES                StringsAttributes,
    _Inout_  WDFCOLLECTION                         Collection
    )
/*++
Routine Description:

    This method is queries a Multi String value from the Adapter Configuration

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    ValueName - Name of the String to be querried.

    StringsAttributes - For each string in the multi-string, a WDFSTRING object
        is created and added to the input Collection. The StringsAttributes is
        used while allocating the string objects.
        These string objects are parented to the collection object unless
        the client explicitly specifies the Parent.

    Collection - Input collection to which the WDFSTRING objects are added.

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    return GetNxConfigurationFromHandle(Configuration)->QueryMultiString(ValueName, StringsAttributes, Collection);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryBinary)(
    _In_     PNET_DRIVER_GLOBALS                   Globals,
    _In_     NETCONFIGURATION                      Configuration,
    _In_     PCUNICODE_STRING                      ValueName,
    _Strict_type_match_ _In_
             POOL_TYPE                             PoolType,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES                MemoryAttributes,
    _Out_    WDFMEMORY*                            WdfMemory
    )
/*++
Routine Description:

    This method is queries a binary data from the Adapter Configuration

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    ValueName - Name of the Data to be querried.

    PoolType - PoolType as described by the documentation of WdfMemoryCreate

    MemoryAttributes - optional WDF_OBJECT_ATTRIBUTES for the memory object
        that is allocated and returned as part of this function.
        The memory objects is parented to the Configuration object unless
        the client explicitly specifies the Parent.

    WdfMemory - Output WDFMEMORY object representing the Data read
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    return GetNxConfigurationFromHandle(Configuration)->QueryBinary(ValueName,
                                          PoolType,
                                          MemoryAttributes,
                                          WdfMemory);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationQueryLinkLayerAddress)(
    _In_  PNET_DRIVER_GLOBALS DriverGlobals,
    _In_  NETCONFIGURATION Configuration,
    _Out_ PNET_ADAPTER_LINK_LAYER_ADDRESS LinkLayerAddress
    )
/*++
Routine Description:

    This method queries from registry the link layer address of the adapter

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NetAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    LinkLayerAddress [out] - A caller-supplied location that, on return,
        contains the queried link layer address.

Returns:

    STATUS_BUFFER_OVERFLOW - Returned if the value stored in the registry
    was larger than NDIS_MAX_PHYS_ADDRESS_LENGTH

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(DriverGlobals);
    auto nxConfiguration = GetNxConfigurationFromHandle(Configuration);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);
    Verifier_VerifyNotNull(pNxPrivateGlobals, LinkLayerAddress);
    Verifier_VerifyNotNull(pNxPrivateGlobals, nxConfiguration->m_NxAdapter);

    return nxConfiguration->QueryLinkLayerAddress(LinkLayerAddress);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationAssignUlong)(
    _In_  PNET_DRIVER_GLOBALS                   Globals,
    _In_  NETCONFIGURATION                      Configuration,
    _In_  PCUNICODE_STRING                      ValueName,
    _In_  ULONG                                 Value
    )
/*++
Routine Description:

    This method is assigns a Ulong value to the Adapter Configuration

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    Flags - A value defined in the NET_CONFIGURATION_ULONG_FLAGS enum

    ValueName - Name of the Ulong to be assigned.

    Value - The value of the ULONG

--*/
{
    auto  pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    return GetNxConfigurationFromHandle(Configuration)->AssignUlong(ValueName, Value);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationAssignUnicodeString)(
    _In_  PNET_DRIVER_GLOBALS                   Globals,
    _In_  NETCONFIGURATION                      Configuration,
    _In_  PCUNICODE_STRING                      ValueName,
    _In_  PCUNICODE_STRING                      Value
    )
/*++
Routine Description:

    This method is assigns a string to the Adapter Configuration

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    ValueName - Name of the Unicode String to be assigned.

    Value - The Unicode String to be assigned

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    return GetNxConfigurationFromHandle(Configuration)->AssignUnicodeString(ValueName, Value);
}


_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationAssignBinary)(
    _In_                               PNET_DRIVER_GLOBALS       Globals,
    _In_                               NETCONFIGURATION          Configuration,
    _In_                               PCUNICODE_STRING          ValueName,
    _In_reads_bytes_(BufferLength)     PVOID                     Buffer,
    _In_                               ULONG                     BufferLength
    )
/*++
Routine Description:

    This method is assigns binary data to the Adapter Configuration

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    ValueName - Name of the Binary Data to be assigned.

    Buffer - The buffer holding the binary data

    BufferLength - The length of the buffer in bytes.
--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    return GetNxConfigurationFromHandle(Configuration)->AssignBinary(ValueName, Buffer, BufferLength);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetConfigurationAssignMultiString)(
    _In_  PNET_DRIVER_GLOBALS                   Globals,
    _In_  NETCONFIGURATION                      Configuration,
    _In_  PCUNICODE_STRING                      ValueName,
    _In_  WDFCOLLECTION                         StringsCollection
    )
/*++
Routine Description:

    This method is assigns a Multi String to the Adapter Configuration

Arguments:

    Configuration - Pointer to the Adapter Configuration opened
        in a prior call to NdisCxAdapterOpenConfiguration or
        NetConfigurationOpenSubConfiguration

    ValueName - Name of the Binary Data to be assigned.

    StringsCollection - Collection of WDFSTRINGs that would form the
        multisz string

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    return GetNxConfigurationFromHandle(Configuration)->AssignMultiString(ValueName, StringsCollection);
}

