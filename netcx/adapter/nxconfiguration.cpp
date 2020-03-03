// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This implements the Nx Adapter Configuration object.

--*/

#include "Nx.hpp"

#include "NxConfiguration.tmh"
#include "NxConfiguration.hpp"

#include "NxDevice.hpp"
#include "NxAdapter.hpp"
#include "NxMacros.hpp"
#include "NxUtility.hpp"
#include "verifier.hpp"

struct NX_PRIVATE_GLOBALS;

NxConfiguration::NxConfiguration(
    _In_ NX_PRIVATE_GLOBALS *     NxPrivateGlobals,
    _In_ NETCONFIGURATION         Configuration,
    _In_ NxConfiguration *        ParentNxConfiguration,
    _In_ NxDevice *               NxDevice) :
    CFxObject(Configuration),
    m_ParentNxConfiguration(ParentNxConfiguration),
    m_NxDevice(NxDevice),
    m_NdisConfigurationHandle(NULL),
    m_IsDeviceConfig(true)
{
    UNREFERENCED_PARAMETER(NxPrivateGlobals);
}

NxConfiguration::NxConfiguration(
    _In_ NX_PRIVATE_GLOBALS *     NxPrivateGlobals,
    _In_ NETCONFIGURATION         Configuration,
    _In_ NxConfiguration *        ParentNxConfiguration,
    _In_ NxAdapter *              NxAdapter) :
    CFxObject(Configuration),
    m_ParentNxConfiguration(ParentNxConfiguration),
    m_NxAdapter(NxAdapter),
    m_NdisConfigurationHandle(NULL),
    m_IsDeviceConfig(false)
{
    UNREFERENCED_PARAMETER(NxPrivateGlobals);
}

PAGED NTSTATUS
NxConfiguration::ReadConfiguration(
    _Out_ PNDIS_CONFIGURATION_PARAMETER *ParameterValue,
    _In_  PCUNICODE_STRING               Keyword,
    _In_  NDIS_PARAMETER_TYPE            ParameterType)
{
    NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
    NdisWdfReadConfiguration(
        &ndisStatus,
        ParameterValue,
        m_NdisConfigurationHandle,
        (PNDIS_STRING)Keyword,
        ParameterType);

    NTSTATUS status = STATUS_SUCCESS;
    switch (ndisStatus)
    {















    case NDIS_STATUS_FAILURE:
        status = STATUS_OBJECT_NAME_NOT_FOUND;
        break;
    default:
        status = NdisConvertNdisStatusToNtStatus(ndisStatus);
        break;
    }

    if (!NT_SUCCESS(status))
    {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
            "NdisWdfReadConfiguration failed to read keyword %wZ : status=%!STATUS!, ndisStatus=%!NDIS_STATUS!",
            Keyword, status, ndisStatus);
        return status;
    }

    return status;
}

NxConfiguration::~NxConfiguration()
{
    NT_ASSERT(m_NdisConfigurationHandle == NULL);

    if (m_Key)
    {
        WdfRegistryClose(m_Key);
    }
}

VOID
NxConfiguration::_EvtCleanup(
    _In_  WDFOBJECT Configuration
)
/*++
Routine Description:
    A WDF Event Callback

    This routine is called when the client driver WDFDEVICE object is being
    deleted. This happens when WDF is processing the REMOVE irp for the client.

--*/
{
    auto nxConfiguration = GetNxConfigurationFromHandle((NETCONFIGURATION)Configuration);
    if (nxConfiguration->m_NdisConfigurationHandle) {
        //
        // Close Ndis Configuration
        //
        NdisCloseConfiguration(nxConfiguration->m_NdisConfigurationHandle);
        nxConfiguration->m_NdisConfigurationHandle = NULL;
    }

    return;
}

NTSTATUS
NxConfiguration::_Create(
    _In_     NX_PRIVATE_GLOBALS *     PrivateGlobals,
    _In_     NxDevice *               NxDevice,
    _In_opt_ WDF_OBJECT_ATTRIBUTES *  ConfigurationAttributes,
    _In_opt_ NxConfiguration *        ParentNxConfiguration,
    _Out_    NxConfiguration **       NxConfigurationArg
)
/*++
Routine Description:
    Static method that creates the NETCONFIGURATION object
    and opens it.

    This is the internal implementation of the NetDeviceOpenConfiguration
    public API.

--*/
{
    // The WDFOBJECT created by NetAdapterCx representing the Net Device
    NETCONFIGURATION netConfiguration;

    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // Create a WDFOBJECT for the NxConfiguration
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxConfiguration);
    if (ParentNxConfiguration != NULL)
    {
        attributes.ParentObject = ParentNxConfiguration->GetFxObject();
    }
    else
    {
        attributes.ParentObject = NxDevice->GetFxObject();
    }

    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxConfiguration::_EvtCleanup;

    //
    // Ensure that the destructor would be called when this object is destroyed.
    //
    NxConfiguration::_SetObjectAttributes(&attributes);

    CX_RETURN_IF_NOT_NT_SUCCESS(WdfObjectCreate(&attributes, (WDFOBJECT*)&netConfiguration));

    //
    // Since we just created the NetDevice, the NxConfiguration object has
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
        NxDevice);

    __analysis_assume(nxConfiguration != NULL);

    if (ConfigurationAttributes != WDF_NO_OBJECT_ATTRIBUTES)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(nxConfiguration->AddAttributes(ConfigurationAttributes));
    }

    NTSTATUS status = nxConfiguration->Open();
    if (!NT_SUCCESS(status))
    {
        nxConfiguration->Close();
        return status;
    }

    *NxConfigurationArg = nxConfiguration;

    return status;
}

NTSTATUS
NxConfiguration::_Create(
    _In_     NX_PRIVATE_GLOBALS *     PrivateGlobals,
    _In_     NxAdapter *              NxAdapter,
    _In_opt_ NxConfiguration *        ParentNxConfiguration,
    _Out_    NxConfiguration **       NxConfigurationArg
)
/*++
Routine Description:
    Static method that creates the NETCONFIGURATION object
    and opens it.

    This is the internal implementation of the NetAdapterOpenConfiguration
    public API.

--*/
{
    NTSTATUS                  status;

    // The WDFOBJECT created by NetAdapterCx representing the Net Adapter
    NETCONFIGURATION          netConfiguration;

    WDF_OBJECT_ATTRIBUTES     attributes;

    //
    // Create a WDFOBJECT for the NxConfiguration
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxConfiguration);
    if (ParentNxConfiguration != NULL) {
        attributes.ParentObject = ParentNxConfiguration->GetFxObject();
    } else {
        attributes.ParentObject = NxAdapter->GetFxObject();
    }

    #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
    attributes.EvtCleanupCallback = NxConfiguration::_EvtCleanup;

    //
    // Ensure that the destructor would be called when this object is destroyed.
    //
    NxConfiguration::_SetObjectAttributes(&attributes);

    status = WdfObjectCreate(&attributes, (WDFOBJECT*)&netConfiguration);
    if (!NT_SUCCESS(status)) {
        LogError(NxAdapter->GetRecorderLog(), FLAG_CONFIGURATION,
                 "WdfObjectCreate for NetConfiguration failed %!STATUS!", status);
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
                                                                  NxAdapter);

    __analysis_assume(nxConfiguration != NULL);

    NT_ASSERT(nxConfiguration);

    *NxConfigurationArg = nxConfiguration;

    return STATUS_SUCCESS;
}

NTSTATUS
NxConfiguration::Open(
    VOID
)
/*++
Routine Description:
    Method that opens the created configuration.

--*/
{
    NT_ASSERT(m_ParentNxConfiguration == NULL);

    if (m_IsDeviceConfig)
    {
        NTSTATUS status = WdfDeviceOpenRegistryKey(m_NxDevice->GetFxObject(),
            PLUGPLAY_REGKEY_DRIVER,
            KEY_READ | KEY_WRITE,
            WDF_NO_OBJECT_ATTRIBUTES,
            &m_Key);

        return status;
    }

    NDIS_STATUS               ndisStatus;

    // Opaque Handle for the Ndis Adapter Configuration given by ndis.sys
    NDIS_HANDLE               ndisConfigurationHandle;

    NDIS_CONFIGURATION_OBJECT configObject;

    //
    // Call NDIS to open the configuration
    //
    RtlZeroMemory(&configObject, sizeof(configObject));
    configObject.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
    configObject.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
    configObject.Header.Size = NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1;
    C_ASSERT(NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1 <= sizeof(NDIS_CONFIGURATION_OBJECT));

    configObject.NdisHandle = m_NxAdapter->GetNdisHandle();

    ndisStatus = NdisOpenConfigurationEx(&configObject,
                                         &ndisConfigurationHandle);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        LogError(m_NxAdapter->GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisOpenConfigurationEx failed ndis-status %!NDIS_STATUS!", ndisStatus);
        return NdisConvertNdisStatusToNtStatus(ndisStatus);
    }

    m_NdisConfigurationHandle = ndisConfigurationHandle;

    return STATUS_SUCCESS;
}

NTSTATUS
NxConfiguration::OpenAsSubConfiguration(
    PCUNICODE_STRING  SubConfigurationName
)
/*++
Routine Description:
    Method that opens the created sub configuration.

--*/
{
    if (m_IsDeviceConfig)
    {
        NTSTATUS status = WdfRegistryOpenKey(m_ParentNxConfiguration->m_Key,
            SubConfigurationName,
            KEY_READ | KEY_WRITE,
            WDF_NO_OBJECT_ATTRIBUTES,
            &m_Key);

        return status;
    }

    NTSTATUS                  status;
    NDIS_STATUS               ndisStatus;

    // Opaque Handle for the Sub Ndis Adapter Configuration given by ndis.sys
    NDIS_HANDLE               ndisConfigurationHandle;

    NT_ASSERT(m_ParentNxConfiguration);

    NdisOpenConfigurationKeyByName(&ndisStatus,
                                   m_ParentNxConfiguration->m_NdisConfigurationHandle,
                                   (PNDIS_STRING)SubConfigurationName,
                                   &ndisConfigurationHandle);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        LogError(m_NxAdapter->GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisOpenConfigurationKeyByName failed ndis-status %!NDIS_STATUS!", ndisStatus);
        return NdisConvertNdisStatusToNtStatus(ndisStatus);
    }

    m_NdisConfigurationHandle = ndisConfigurationHandle;

    status = STATUS_SUCCESS;

    return status;
}

VOID
NxConfiguration::DeleteFromFailedOpen(
    VOID
)
{
    NT_ASSERT(m_NdisConfigurationHandle == NULL);
    WdfObjectDelete(GetFxObject());
}

VOID
NxConfiguration::Close(
    VOID
)
{
    //
    // For this object Close is equivalent to Delete.
    // The Cleanup Callback closes the configuration.
    //
    WdfObjectDelete(GetFxObject());
}

NTSTATUS
NxConfiguration::AddAttributes(
    _In_ WDF_OBJECT_ATTRIBUTES * Attributes
)
{
    NTSTATUS status;
    status = WdfObjectAllocateContext(GetFxObject(), Attributes, NULL);
    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
            "WdfObjectAllocateContext Failed %!STATUS!", status);
    }

    return status;
}

RECORDER_LOG
NxConfiguration::GetRecorderLog(
    VOID
)
{
    if (m_IsDeviceConfig)
    {
        return m_NxDevice->GetRecorderLog();
    }

    return m_NxAdapter->GetRecorderLog();
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryUlong(
    _In_  NET_CONFIGURATION_QUERY_ULONG_FLAGS Flags,
    _In_  PCUNICODE_STRING                    ValueName,
    _Out_ PULONG                              Value
)
/*++
Routine Description:

    This method queries a Ulong value from the Configuration

Arguments:

    Flags - A value defined in the NET_CONFIGURATION_QUERY_ULONG_FLAGS enum

    ValueName - Name of the Ulong to be queried.

    Value - Address of the memory where the queried ULONG would be written

--*/
{
    NTSTATUS status;
    ULONG base = 16;

    if (m_IsDeviceConfig)
    {
        switch (Flags)
        {
            case NET_CONFIGURATION_QUERY_ULONG_NO_FLAGS:

                status = WdfRegistryQueryULong(m_Key, ValueName, Value);

                // If the ValueName is not ULONG, it may be a string. So read it as
                // a string and convert it to ULONG.
                CX_RETURN_NTSTATUS_IF(status, status != STATUS_OBJECT_TYPE_MISMATCH);
                base = 10;
                __fallthrough;

            case NET_CONFIGURATION_QUERY_ULONG_MAY_BE_STORED_AS_HEX_STRING:

                WDFSTRING wdfString;
                UNICODE_STRING unicodeString;

                if (NT_SUCCESS(QueryString(ValueName, WDF_NO_OBJECT_ATTRIBUTES, &wdfString)))
                {
                    WdfStringGetUnicodeString(wdfString, &unicodeString);
                    return RtlUnicodeStringToInteger(&unicodeString, base, Value);
                }

            default:
                return STATUS_INVALID_PARAMETER;
        }
    }

    PNDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;
    NDIS_PARAMETER_TYPE ndisParamType;

    *Value = 0;

    switch (Flags) {
    case NET_CONFIGURATION_QUERY_ULONG_NO_FLAGS:
        ndisParamType = NdisParameterInteger;
        break;
    case NET_CONFIGURATION_QUERY_ULONG_MAY_BE_STORED_AS_HEX_STRING:
        ndisParamType = NdisParameterHexInteger;
        break;
    default:
        status = STATUS_INVALID_PARAMETER;
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "Invalid Flags Value %!NET_CONFIGURATION_QUERY_ULONG_FLAGS!", Flags);
        return status;
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        ReadConfiguration(&ndisConfigurationParam, ValueName, ndisParamType));

    if ((NdisParameterInteger != ndisConfigurationParam->ParameterType) &&
        (NdisParameterHexInteger != ndisConfigurationParam->ParameterType)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "ReadConfiguration returned data of type %!NDIS_PARAMETER_TYPE!, but the caller requested an Integer type", ndisConfigurationParam->ParameterType);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    *Value = ndisConfigurationParam->ParameterData.IntegerData;

    LogVerbose(GetRecorderLog(), FLAG_CONFIGURATION,
               "Read Configuration Value %wZ : %d", ValueName, *Value);

    return STATUS_SUCCESS;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryString(
    _In_     PCUNICODE_STRING                      ValueName,
    _In_opt_ WDF_OBJECT_ATTRIBUTES *               StringAttributes,
    _Out_    WDFSTRING*                            WdfString
)
/*++
Routine Description:

    This method queries a String value from the Adapter Configuration

Arguments:

    ValueName - Name of the String to be queried.

    StringAttributes - optional WDF_OBJECT_ATTRIBUTES for the string

    WdfString - Output WDFSTRING object.

--*/
{
    WDF_OBJECT_ATTRIBUTES attribs;

    //
    // If the client didn't specify any specific string attributes
    // use a local copy
    //
    if (StringAttributes == WDF_NO_OBJECT_ATTRIBUTES) {
        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        StringAttributes = &attribs;
    }

    //
    // If the client didn't specify a parent object explicitly
    // parent this to the Configuration.
    //
    if (StringAttributes->ParentObject == NULL) {
        StringAttributes->ParentObject = GetFxObject();
    }

    if (m_IsDeviceConfig)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(WdfStringCreate(NULL,
            StringAttributes,
            WdfString));

        if (!NT_SUCCESS(WdfRegistryQueryString(m_Key, ValueName, *WdfString)))
        {
            WdfObjectDelete(*WdfString);
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS status;
    PNDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        ReadConfiguration(
            &ndisConfigurationParam,
            ValueName,
            NdisParameterString));

    if (NdisParameterString != ndisConfigurationParam->ParameterType) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "ReadConfiguration returned data of type %!NDIS_PARAMETER_TYPE!, but the caller requested a String type",
                 ndisConfigurationParam->ParameterType);
        status = STATUS_OBJECT_TYPE_MISMATCH;
        return status;
    }

    LogVerbose(GetRecorderLog(), FLAG_CONFIGURATION, "Read Configuratration Value %wZ : %wZ",
               ValueName, (PUNICODE_STRING)&ndisConfigurationParam->ParameterData.StringData);

    status = WdfStringCreate((PCUNICODE_STRING)&ndisConfigurationParam->ParameterData.StringData,
                             StringAttributes,
                             WdfString);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "WdfStringCreate failed %!STATUS!", status);
    }

    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryMultiString(
    _In_     PCUNICODE_STRING                      ValueName,
    _In_opt_ WDF_OBJECT_ATTRIBUTES *               StringsAttributes,
    _In_     WDFCOLLECTION                         Collection
)
/*++
Routine Description:

    This method queries a Multi String value from the Adapter Configuration

Arguments:

    ValueName - Name of the String to be queried.

    StringsAttributes - For each string in the multi-string, a WDFSTRING object
        is created and added to the input Collection. The StringsAttributes is
        used while allocating the string objects.
        These string objects are parented to the collection object unless
        the client explicitly specifies the Parent.

    Collection - Input collection to which the WDFSTRING objects are added.

--*/
{
    if (m_IsDeviceConfig)
    {
        return WdfRegistryQueryMultiString(m_Key, ValueName, StringsAttributes, Collection);
    }

    NTSTATUS status;
    PNDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        ReadConfiguration(
            &ndisConfigurationParam,
            ValueName,
            NdisParameterMultiString));

    if (NdisParameterMultiString != ndisConfigurationParam->ParameterType) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "ReadConfiguration returned data of type %!NDIS_PARAMETER_TYPE!, but the caller requested a MultiString type",
                 ndisConfigurationParam->ParameterType);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    status = NxWdfCollectionAddMultiSz(Collection,
                                       StringsAttributes,
                                       (PWCHAR)ndisConfigurationParam->ParameterData.StringData.Buffer,
                                       ndisConfigurationParam->ParameterData.StringData.Length,
                                       GetRecorderLog());

    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryBinary(
    _In_     PCUNICODE_STRING                      ValueName,
    _Strict_type_match_ _In_
             POOL_TYPE                             PoolType,
    _In_opt_ WDF_OBJECT_ATTRIBUTES *               MemoryAttributes,
    _Out_    WDFMEMORY*                            WdfMemory
)
/*++
Routine Description:

    This method queries a binary data from the Configuration

Arguments:

    ValueName - Name of the Data to be queried.

    PoolType - PoolType as described by the documentation of WdfMemoryCreate

    MemoryAttributes - optional WDF_OBJECT_ATTRIBUTES for the memory object
        that is allocated and returned as part of this function.
        The memory objects is parented to the Configuration object unless
        the client explicitly specifies the Parent.

    WdfMemory - Output WDFMEMORY object representing the Data read

--*/
{
    WDF_OBJECT_ATTRIBUTES attribs;

    //
    // If the client didn't specify any specific string attributes
    // use a local copy
    //
    if (MemoryAttributes == WDF_NO_OBJECT_ATTRIBUTES) {
        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        MemoryAttributes = &attribs;
    }

    //
    // If the client didn't specify a parent object explicitly
    // parent this to the Configuration.
    //
    if (MemoryAttributes->ParentObject == NULL) {
        MemoryAttributes->ParentObject = GetFxObject();
    }

    if (m_IsDeviceConfig)
    {
        return WdfRegistryQueryMemory(m_Key, ValueName, PoolType, MemoryAttributes, WdfMemory, NULL);
    }

    NTSTATUS status;
    PNDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;
    PVOID buffer;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        ReadConfiguration(
            &ndisConfigurationParam,
            ValueName,
            NdisParameterBinary));

    if (NdisParameterBinary != ndisConfigurationParam->ParameterType) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "ReadConfiguration returned data of type %!NDIS_PARAMETER_TYPE!, but the caller requested a Binary type",
                 ndisConfigurationParam->ParameterType);
        status = STATUS_NOT_FOUND;
        return status;
    }

    //
    // Create a new memory object.
    // We dont use WdfMemoryCreatePreallocated since the lifetime of the
    // memory object being created may be different from the
    // memory returned by NdisWdfReadConfiguration. Instead we crate a
    // new memory object and copy the contents to it.
    //
    status = WdfMemoryCreate(MemoryAttributes,
                             PoolType,
                             0,  /*Use Default Tag for this Driver*/
                             ndisConfigurationParam->ParameterData.BinaryData.Length,
                             WdfMemory,
                             &buffer);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "WdfMemoryCreate failed %!STATUS!", status);
    }

    RtlCopyMemory(buffer,
                  ndisConfigurationParam->ParameterData.BinaryData.Buffer,
                  ndisConfigurationParam->ParameterData.BinaryData.Length);

    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryLinkLayerAddress(
    _Out_    NET_ADAPTER_LINK_LAYER_ADDRESS         *LinkLayerAddress
)
/*++
Routine Description:

    This method queries from registry the link layer address of the adapter.
    This is a part of the adapter configuration, so it should be called
    after the adapter is created.

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
    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    PVOID networkAddressBuffer;
    UINT resultLength;

    NdisReadNetworkAddress(&ndisStatus,
                           &networkAddressBuffer,
                           &resultLength,
                           m_NdisConfigurationHandle);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {

        LogInfo(GetRecorderLog(), FLAG_CONFIGURATION,
                 "Failed to read network address. Likely there is no address stored for this adapter. Status %!STATUS! ndisStatus %!NDIS_STATUS!",
                  status,
                  ndisStatus);

        return status;
    }

    if (resultLength > NDIS_MAX_PHYS_ADDRESS_LENGTH) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
            "NdisReadNetworkAddress returned too many bytes, resultLength=%u, NDIS_MAX_PHYS_ADDRESS_LENGTH=%u",
            resultLength,
            NDIS_MAX_PHYS_ADDRESS_LENGTH);

        return STATUS_BUFFER_OVERFLOW;
    }

    NET_ADAPTER_LINK_LAYER_ADDRESS_INIT(
        LinkLayerAddress,
        static_cast<USHORT>(resultLength),
        static_cast<UCHAR *>(networkAddressBuffer));

    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::AssignUlong(
    _In_  PCUNICODE_STRING                      ValueName,
    _In_  ULONG                                 Value
)
/*++
Routine Description:

    This method assigns a Ulong value to the Configuration

Arguments:

    ValueName - Name of the Ulong to be assigned.

    Value - The value of the ULONG

--*/
{
    if (m_IsDeviceConfig)
    {
        return WdfRegistryAssignULong(m_Key, ValueName, Value);
    }

    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    NDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;

    RtlZeroMemory(&ndisConfigurationParam, sizeof(NDIS_CONFIGURATION_PARAMETER));

    ndisConfigurationParam.ParameterType = NdisParameterInteger;
    ndisConfigurationParam.ParameterData.IntegerData = Value;

    NdisWriteConfiguration(&ndisStatus,
                           m_NdisConfigurationHandle,
                           (PNDIS_STRING)ValueName,
                           &ndisConfigurationParam);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWriteConfiguration failed with status %!STATUS! ndisStatus %!NDIS_STATUS!", status, ndisStatus);
        return status;
    }

    status = STATUS_SUCCESS;

    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::AssignUnicodeString(
    _In_  PCUNICODE_STRING                      ValueName,
    _In_  PCUNICODE_STRING                      Value
)
/*++
Routine Description:

    This method assigns a Ulong value to the Configuration

Arguments:

    ValueName - Name of the Unicode String to be assigned.

    Value - The Unicode String to be assigned

--*/
{
    if (m_IsDeviceConfig)
    {
        return WdfRegistryAssignUnicodeString(m_Key, ValueName, Value);
    }

    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    NDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;

    RtlZeroMemory(&ndisConfigurationParam, sizeof(NDIS_CONFIGURATION_PARAMETER));

    ndisConfigurationParam.ParameterType = NdisParameterString;
    ndisConfigurationParam.ParameterData.StringData.Buffer = Value->Buffer;
    ndisConfigurationParam.ParameterData.StringData.Length = Value->Length;
    ndisConfigurationParam.ParameterData.StringData.MaximumLength = Value->MaximumLength;

    NdisWriteConfiguration(&ndisStatus,
                           m_NdisConfigurationHandle,
                           (PNDIS_STRING)ValueName,
                           &ndisConfigurationParam);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWriteConfiguration failed with status %!STATUS! ndisStatus %!NDIS_STATUS!", status, ndisStatus);
        return status;
    }

    status = STATUS_SUCCESS;

    return status;
}


_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::AssignBinary(
    _In_                                PCUNICODE_STRING    ValueName,
    _In_reads_bytes_(BufferLength)      PVOID               Buffer,
    _In_                                ULONG               BufferLength
)
/*++
Routine Description:

    This method assigns binary data to the Configuration

Arguments:

    ValueName - Name of the Binary Data to be assigned.

    Buffer - The buffer holding the binary data

    BufferLength - The length of the buffer in bytes.
--*/
{
    NTSTATUS status;

    if (m_IsDeviceConfig)
    {
        WDFMEMORY memory;
        CX_RETURN_IF_NOT_NT_SUCCESS(WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES, Buffer, BufferLength, &memory));

        status = WdfRegistryAssignMemory(m_Key, ValueName, REG_BINARY, memory, nullptr);
        if (!NT_SUCCESS(status))
        {
            WdfObjectDelete(memory);
        }

        return status;
    }

    NDIS_STATUS ndisStatus;
    NDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;

    RtlZeroMemory(&ndisConfigurationParam, sizeof(NDIS_CONFIGURATION_PARAMETER));

    ndisConfigurationParam.ParameterType = NdisParameterBinary;

    if (BufferLength >= USHORT_MAX) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "Buffer size is too long: %d", BufferLength);
        return STATUS_BUFFER_OVERFLOW;
    }

    ndisConfigurationParam.ParameterData.BinaryData.Buffer = Buffer;
    ndisConfigurationParam.ParameterData.BinaryData.Length = (USHORT)BufferLength;

    NdisWriteConfiguration(&ndisStatus,
                           m_NdisConfigurationHandle,
                           (PNDIS_STRING)ValueName,
                           &ndisConfigurationParam);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWriteConfiguration failed with status %!STATUS! ndisStatus %!NDIS_STATUS!", status, ndisStatus);
        return status;
    }

    status = STATUS_SUCCESS;

    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::AssignMultiString(
    _In_  PCUNICODE_STRING                      ValueName,
    _In_  WDFCOLLECTION                         StringsCollection
)
/*++
Routine Description:

    This method assigns a Multi String to the Configuration

Arguments:

    ValueName - Name of the Binary Data to be assigned.

    StringsCollection - Collection of WDFSTRINGs that would form the
        multisz string

Comments:

    This routine generates a MULTI_SZ style string from all the input strings
    present in the WDFCOLLECTION.

    Step1 :
    Loop through all the WDFSTRINGs in the
    input collection and determine the size of final multi string

    Step2 :
    Allocate the buffer to hold the Multi String

    Step3 :
    Loop through the WDFSTRINGs in the collections again and this time
    build the multi string

    The resulting string is of the following form
    <String1>W'\0'<String2>W'\0'...<StringN>W'\0'W'\0'

    Step4 :
    Call NdisWriteConfiguration to assing the multi string
--*/
{
    if (m_IsDeviceConfig)
    {
        return WdfRegistryAssignMultiString(m_Key, ValueName, StringsCollection);
    }

    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    NDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;
    PUCHAR strBuffer = NULL;
    ULONG  strBufferLength;

    ULONG numberOfStrings;
    ULONG index;

    numberOfStrings = WdfCollectionGetCount(StringsCollection);
    ULONG cbLength;

    WDFSTRING wdfstring;
    UNICODE_STRING unicodeString;

    //
    // ** Step1 :
    // Loop through all the WDFSTRINGs in the
    // input collection and determine the size of final multi string
    //

    //
    // Account for the 2nd Trailing NULL.
    //
    cbLength = sizeof(WCHAR);

    //
    // Calculate the size of the needed buffer
    //
    for (index = 0; index < numberOfStrings; index++) {
        wdfstring = (WDFSTRING) WdfCollectionGetItem(StringsCollection, index);
        WdfStringGetUnicodeString(wdfstring, &unicodeString);

        //
        // Add the length of this unicode string to cbLength
        //
        cbLength += unicodeString.Length;

        //
        // Add Null Terminator since that is not included as part of length
        //
        cbLength += sizeof(WCHAR);

        if (cbLength >= USHORT_MAX) {
            //
            // Overflow Error
            //
            LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                     "Too long multi string");
            status = STATUS_BUFFER_OVERFLOW;
            goto Exit;
        }

    }

    //
    // ** Step2 :
    // Allocate the buffer to hold the Multi String
    //

    strBufferLength = cbLength;

    strBuffer = (PUCHAR)ExAllocatePoolWithTag(NonPagedPoolNx,
                                              strBufferLength,
                                              NETADAPTERCX_TAG);

    if (strBuffer == NULL) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "ExAllocatePool failed for size %d", cbLength);
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    //
    // Zero out the memory so that we dont have to explicitly program the
    // Null characters.
    //
    RtlZeroMemory(strBuffer, strBufferLength);

    //
    // ** Step3 :
    // Loop through the WDFSTRINGs in the collections again and this time
    // build the multi string
    //

    cbLength = 0;

    //
    // Build the Multi String
    //
    for (index = 0; index < numberOfStrings; index++) {

        wdfstring = (WDFSTRING) WdfCollectionGetItem(StringsCollection, index);
        WdfStringGetUnicodeString(wdfstring, &unicodeString);

        if (cbLength + unicodeString.Length + sizeof(WCHAR) >= strBufferLength) {
            //
            // Something Nasty Happened. The buffer we allocated
            // to generate the multi string is not sufficient anymore.
            // Maybe the client changed the collection
            // contents while we were processing this function, or there is
            // but in this function
            //
            LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                     "Buffer length (%d) that we allocated was not sufficient", strBufferLength);
            status = STATUS_BUFFER_OVERFLOW;
            goto Exit;
        }

        RtlCopyMemory(strBuffer + cbLength,
                      unicodeString.Buffer,
                      unicodeString.Length);

        cbLength += unicodeString.Length;

        //
        // Also account for a NULL char.
        //

        cbLength += sizeof(WCHAR);

    }

    //
    // Account for and Confirm that there was a space left for
    // the trailing null for the multi-sz string
    //
    cbLength += sizeof(WCHAR);

    NT_ASSERT(cbLength == strBufferLength);

    if (cbLength > strBufferLength) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "Buffer length (%d) that we allocated was not sufficient. "
                 "There is no space for the 2nd trailing NULL", strBufferLength);
        status = STATUS_BUFFER_OVERFLOW;
        goto Exit;
    }

    //
    // ** Step4 :
    // Call NdisWriteConfiguration to assing the multi string
    //

    RtlZeroMemory(&ndisConfigurationParam, sizeof(NDIS_CONFIGURATION_PARAMETER));

    ndisConfigurationParam.ParameterType = NdisParameterMultiString;

    //
    // Build the StringData.
    // Note: It is ok to cast the ULONG cbLength to USHORT. We have already
    // checked above that the cbLength was smaller than the MAX_USHORT
    //
    ndisConfigurationParam.ParameterData.StringData.Buffer = (PWCH)strBuffer;
    ndisConfigurationParam.ParameterData.StringData.Length = (USHORT)cbLength;
    ndisConfigurationParam.ParameterData.StringData.MaximumLength = (USHORT)cbLength;

    NdisWriteConfiguration(&ndisStatus,
                           m_NdisConfigurationHandle,
                           (PNDIS_STRING)ValueName,
                           &ndisConfigurationParam);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWriteConfiguration failed with status %!STATUS! ndisStatus %!NDIS_STATUS!", status, ndisStatus);
        goto Exit;
    }

    status = STATUS_SUCCESS;

Exit:

    if (strBuffer) {
        ExFreePool(strBuffer);
    }
    return status;
}

