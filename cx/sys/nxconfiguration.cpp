/*++
 
Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxConfiguration.cpp

Abstract:

    This implements the Nx Adapter Configuration object.





Environment:

    kernel mode only

Revision History:

--*/

#include "Nx.hpp"

// Tracing support
extern "C" {
#include "NxConfiguration.tmh"
}

NxConfiguration::NxConfiguration(
    _In_ PNX_PRIVATE_GLOBALS      NxPrivateGlobals,
    _In_ NETCONFIGURATION         Configuration,
    _In_ PNxConfiguration         ParentNxConfiguration,
    _In_ PNxAdapter               NxAdapter) : 
    CFxObject(Configuration), 
    m_ParentNxConfiguration(ParentNxConfiguration),
    m_NxAdapter(NxAdapter),
    m_NdisConfigurationHandle(NULL)
{
    FuncEntry(FLAG_CONFIGURATION);

    UNREFERENCED_PARAMETER(NxPrivateGlobals);

    FuncExit(FLAG_CONFIGURATION);
}

NxConfiguration::~NxConfiguration()
{
    FuncEntry(FLAG_CONFIGURATION);

    NT_ASSERT(m_NdisConfigurationHandle == NULL);

    FuncExit(FLAG_CONFIGURATION);
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
    FuncEntry(FLAG_CONFIGURATION);

    PNxConfiguration nxConfiguration = GetNxConfigurationFromHandle((NETCONFIGURATION)Configuration);

    if (nxConfiguration->m_NdisConfigurationHandle) {
        //
        // Close Ndis Configuration
        //
        NdisCloseConfiguration(nxConfiguration->m_NdisConfigurationHandle);
        nxConfiguration->m_NdisConfigurationHandle = NULL;
    }

    FuncExit(FLAG_CONFIGURATION);
    return;
}

NTSTATUS
NxConfiguration::_Create(
    _In_     PNX_PRIVATE_GLOBALS      PrivateGlobals,
    _In_     PNxAdapter               NxAdapter,
    _In_opt_ PNxConfiguration         ParentNxConfiguration,
    _Out_    PNxConfiguration*        NxConfigurationArg
)
/*++
Routine Description: 
    Static method that creates the NETCONFIGURATION object
    and opens it. 
 
    This is the internal implementation of the NetAdapterOpenConfiguration
    public API.
 
--*/
{
    FuncEntry(FLAG_CONFIGURATION);

    NTSTATUS                  status;

    // The WDFOBJECT created by NetAdapterCx representing the Net Adapter
    NETCONFIGURATION          netConfiguration;

    PNxConfiguration          nxConfiguration;
    PVOID                     nxConfigurationMemory;
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
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    //
    // Since we just created the NetAdapter, the NxConfiguration object has 
    // yet not been constructed. Get the nxConfiguration's memory. 
    //
    nxConfigurationMemory = (PVOID) GetNxConfigurationFromHandle(netConfiguration);

    //
    // Use the inplacement new and invoke the constructor on the 
    // NxConfiguration's memory
    //
    nxConfiguration = new (nxConfigurationMemory) NxConfiguration(PrivateGlobals,
                                                                  netConfiguration,
                                                                  ParentNxConfiguration,
                                                                  NxAdapter);
    
    __analysis_assume(nxConfiguration != NULL);

    NT_ASSERT(nxConfiguration);
    
    *NxConfigurationArg = nxConfiguration;

    status = STATUS_SUCCESS;

    FuncExit(FLAG_CONFIGURATION);
    return status;
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
    FuncEntry(FLAG_CONFIGURATION);

    NTSTATUS                  status;
    NDIS_STATUS               ndisStatus;

    // Opaque Handle for the Ndis Adapter Configuration given by ndis.sys
    NDIS_HANDLE               ndisConfigurationHandle;
                              
    NDIS_CONFIGURATION_OBJECT configObject;

    NT_ASSERT(m_ParentNxConfiguration == NULL);

    //
    // Call NDIS to open the configuration
    //
    RtlZeroMemory(&configObject, sizeof(configObject));
    configObject.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
    configObject.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
    configObject.Header.Size = NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1;
    C_ASSERT(NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1 <= sizeof(NDIS_CONFIGURATION_OBJECT));

    configObject.NdisHandle = m_NxAdapter->m_NdisAdapterHandle;

    ndisStatus = NdisOpenConfigurationEx(&configObject, 
                                         &ndisConfigurationHandle);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        LogError(m_NxAdapter->GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisOpenConfigurationEx failed ndis-status %!NDIS_STATUS!", ndisStatus);
        FuncExit(FLAG_CONFIGURATION);
        return NdisConvertNdisStatusToNtStatus(ndisStatus);
    }

    m_NdisConfigurationHandle = ndisConfigurationHandle;
    
    status = STATUS_SUCCESS;

    FuncExit(FLAG_CONFIGURATION);
    return status;
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
    FuncEntry(FLAG_CONFIGURATION);

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
        FuncExit(FLAG_CONFIGURATION);
        return NdisConvertNdisStatusToNtStatus(ndisStatus);
    }

    m_NdisConfigurationHandle = ndisConfigurationHandle;
    
    status = STATUS_SUCCESS;

    FuncExit(FLAG_CONFIGURATION);
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
    _In_ PWDF_OBJECT_ATTRIBUTES Attributes
    )
{
    FuncEntry(FLAG_CONFIGURATION);
    NTSTATUS status;
    status = WdfObjectAllocateContext(GetFxObject(), Attributes, NULL);
    if (!NT_SUCCESS(status)) {
        LogError(m_NxAdapter->GetRecorderLog(), FLAG_CONFIGURATION,
                 "WdfObjectAllocateContext Failed %!STATUS!", status);
    }

    FuncExit(FLAG_CONFIGURATION);
    return status;
}

RECORDER_LOG
NxConfiguration::GetRecorderLog(
    VOID
    )
{
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

    This method is queries a Ulong value from the Adapter Configuration
 
Arguments:

    Flags - A value defined in the NET_CONFIGURATION_QUERY_ULONG_FLAGS enum
 
    ValueName - Name of the Ulong to be querried.
 
    Value - Address of the memory where the querried ULONG would be written
 
--*/
{
    FuncEntry(FLAG_CONFIGURATION);
    
    NTSTATUS status;
    NDIS_STATUS ndisStatus;
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
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    NdisWdfReadConfiguration(&ndisStatus,
                             &ndisConfigurationParam,
                             m_NdisConfigurationHandle,
                             (PNDIS_STRING)ValueName,
                             ndisParamType);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWdfReadConfiguration failed with status %!STATUS! ndisStatus %!NDIS_STATUS!", status, ndisStatus);
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    if ((ndisParamType != NdisParameterInteger) && 
        (ndisParamType != NdisParameterHexInteger)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWdfReadConfiguration succeeded but returned data of incorrect type %!NDIS_PARAMETER_TYPE!", ndisParamType);
        FuncExit(FLAG_CONFIGURATION);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    *Value = ndisConfigurationParam->ParameterData.IntegerData;

    LogVerbose(GetRecorderLog(), FLAG_CONFIGURATION,
               "Read Configuration Value %wZ : %d", ValueName, *Value);

    status = STATUS_SUCCESS;
    
    FuncExit(FLAG_CONFIGURATION);
    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryString(
    _In_     PCUNICODE_STRING                      ValueName,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES                StringAttributes,
    _Out_    WDFSTRING*                            WdfString
    )
/*++
Routine Description:

    This method is queries a String value from the Adapter Configuration
 
Arguments:

    ValueName - Name of the String to be querried.
 
    StringAttributes - optional WDF_OBJECT_ATTRIBUTES for the string
 
    WdfString - Output WDFSTRING object. 
 
--*/
{
    FuncEntry(FLAG_CONFIGURATION);
    
    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    PNDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;
    WDF_OBJECT_ATTRIBUTES attribs;

    NdisWdfReadConfiguration(&ndisStatus,
                             &ndisConfigurationParam,
                             m_NdisConfigurationHandle,
                             (PNDIS_STRING)ValueName,
                             NdisParameterString);
    
    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWdfReadConfiguration failed with status %!STATUS! ndisStatus %!NDIS_STATUS!", status, ndisStatus);
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    if (NdisParameterString != ndisConfigurationParam->ParameterType) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "The Read Parameter is not of type String however is of type %!NDIS_PARAMETER_TYPE!",
                 ndisConfigurationParam->ParameterType);
        FuncExit(FLAG_CONFIGURATION);
        status = STATUS_OBJECT_TYPE_MISMATCH;
        return status;
    }

    LogVerbose(GetRecorderLog(), FLAG_CONFIGURATION, "Read Configuratration Value %wZ : %wZ", 
               ValueName, (PUNICODE_STRING)&ndisConfigurationParam->ParameterData.StringData);

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

    status = WdfStringCreate((PCUNICODE_STRING)&ndisConfigurationParam->ParameterData.StringData, 
                             StringAttributes,
                             WdfString);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "WdfStringCreate failed %!STATUS!", status);
    }
    
    FuncExit(FLAG_CONFIGURATION);
    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryMultiString(
    _In_     PCUNICODE_STRING                      ValueName,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES                StringsAttributes,
    _In_     WDFCOLLECTION                         Collection
    )
/*++
Routine Description:

    This method is queries a Multi String value from the Adapter Configuration
 
Arguments:

    ValueName - Name of the String to be querried.
 
    StringsAttributes - For each string in the multi-string, a WDFSTRING object
        is created and added to the input Collection. The StringsAttributes is
        used while allocating the string objects.
        These string objects are parented to the collection object unless
        the client explicitly specifies the Parent.
 
    Collection - Input collection to which the WDFSTRING objects are added. 
 
--*/
{
    FuncEntry(FLAG_CONFIGURATION);
    
    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    PNDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;

    NdisWdfReadConfiguration(&ndisStatus,
                             &ndisConfigurationParam,
                             m_NdisConfigurationHandle,
                             (PNDIS_STRING)ValueName,
                             NdisParameterMultiString);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWdfReadConfiguration failed with status %!STATUS! ndisStatus %!NDIS_STATUS!", status, ndisStatus);
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    if (NdisParameterMultiString != ndisConfigurationParam->ParameterType) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "The Read Parameter is not of type MultiString however is of type %!NDIS_PARAMETER_TYPE!",
                 ndisConfigurationParam->ParameterType);
        FuncExit(FLAG_CONFIGURATION);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    status = NxWdfCollectionAddMultiSz(Collection, 
                                       StringsAttributes,
                                       (PWCHAR)ndisConfigurationParam->ParameterData.StringData.Buffer,
                                       ndisConfigurationParam->ParameterData.StringData.Length,
                                       GetRecorderLog());
    
    FuncExit(FLAG_CONFIGURATION);
    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryBinary(
    _In_     PCUNICODE_STRING                      ValueName,
    _Strict_type_match_ _In_
             POOL_TYPE                             PoolType,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES                MemoryAttributes,
    _Out_    WDFMEMORY*                            WdfMemory
    )
/*++
Routine Description:

    This method is queries a binary datat from the Adapter Configuration
 
Arguments:

    ValueName - Name of the Data to be querried.
 
    PoolType - PoolType as described by the documentation of WdfMemoryCreate
 
    MemoryAttributes - optional WDF_OBJECT_ATTRIBUTES for the memory object
        that is allocated and returned as part of this function.
        The memory objects is parented to the Configuration object unless
        the client explicitly specifies the Parent.
 
    WdfMemory - Output WDFMEMORY object representing the Data read 
 
--*/
{
    FuncEntry(FLAG_CONFIGURATION);
    
    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    PNDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;
    WDF_OBJECT_ATTRIBUTES attribs;
    PVOID buffer;

    NdisWdfReadConfiguration(&ndisStatus,
                             &ndisConfigurationParam,
                             m_NdisConfigurationHandle,
                             (PNDIS_STRING)ValueName,
                             NdisParameterBinary);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "NdisWdfReadConfiguration failed with status %!STATUS! ndisStatus %!NDIS_STATUS!", status, ndisStatus);
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    if (NdisParameterBinary != ndisConfigurationParam->ParameterType) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "The Read Parameter is not of type Binary however is of type %!NDIS_PARAMETER_TYPE!",
                 ndisConfigurationParam->ParameterType);
        FuncExit(FLAG_CONFIGURATION);
        status = STATUS_NOT_FOUND;
        return status;
    }

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
        FuncExit(FLAG_CONFIGURATION);
    }
    
    RtlCopyMemory(buffer,
                  ndisConfigurationParam->ParameterData.BinaryData.Buffer,
                  ndisConfigurationParam->ParameterData.BinaryData.Length);

    FuncExit(FLAG_CONFIGURATION);
    return status;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
NxConfiguration::QueryNetworkAddress(
    _In_     ULONG                                 BufferLength,
    _Out_writes_bytes_to_(BufferLength,*ResultLength)
             PVOID                                 NetworkAddressBuffer,
    _Out_    PULONG                                ResultLength
    )
/*++
Routine Description:

    This method is queries the network address (typically the mac) address
    of the adapter
 
Arguments:
    BufferLength - The size, in bytes, of the buffer that
        is pointed to by NetworkAddressBuffer.
 
    NetworkAddressBuffer [out] - A caller-supplied pointer to a caller-allocated
        buffer that receives the requested network address as a byte array.
        The pointer can be NULL if the BufferLength parameter is zero.
 
    ResultLength [out] - A caller-supplied location that, on return,
        contains the size, in bytes, of the information that the method stored
        in PropertyBuffer. If the function's return value is
        STATUS_BUFFER_TOO_SMALL, this location receives the required buffer size.
 
Returns : 
    STATUS_BUFFER_TOO_SMALL - If the supplied buffer is too small to recieve
        the information. 

--*/
{
    FuncEntry(FLAG_CONFIGURATION);
    
    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    PVOID networkAddressBufferLocal;
    UINT resultLengthLocal;

    *ResultLength = 0;

    NdisReadNetworkAddress(&ndisStatus,
                           &networkAddressBufferLocal,
                           &resultLengthLocal,
                           m_NdisConfigurationHandle);

    status = NdisConvertNdisStatusToNtStatus(ndisStatus);

    if (!NT_SUCCESS(status)) {

        NT_ASSERT(status != STATUS_BUFFER_TOO_SMALL);

        //
        // As per our API contract, the STATUS_BUFFER_TOO_SMALL has 
        // special meaning. We cannot ensure that NDIS would not return
        // that particular status to us in this call. 
        //
        if (status == STATUS_BUFFER_TOO_SMALL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }

        LogInfo(GetRecorderLog(), FLAG_CONFIGURATION,
                 "Failed to read network address. Likely there is no address stored for this adapter. Status %!STATUS! ndisStatus %!NDIS_STATUS!", 
                  status, 
                  ndisStatus);

        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    *ResultLength = (ULONG)resultLengthLocal;

    if (resultLengthLocal > BufferLength) {
        FuncExit(FLAG_CONFIGURATION);
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlCopyMemory(NetworkAddressBuffer, 
                  networkAddressBufferLocal, 
                  resultLengthLocal);

    FuncExit(FLAG_CONFIGURATION);
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

    This method is assigns a Ulong value to the Adapter Configuration
 
Arguments:

    ValueName - Name of the Ulong to be assigned.
 
    Value - The value of the ULONG
 
--*/
{
    FuncEntry(FLAG_CONFIGURATION);
    
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
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    status = STATUS_SUCCESS;
    
    FuncExit(FLAG_CONFIGURATION);
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

    This method is assigns a Ulong value to the Adapter Configuration
 
Arguments:

    ValueName - Name of the Unicode String to be assigned.
 
    Value - The Unicode String to be assigned
 
--*/
{
    FuncEntry(FLAG_CONFIGURATION);
    
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
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    status = STATUS_SUCCESS;
    
    FuncExit(FLAG_CONFIGURATION);
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

    This method is assigns binary data to the Adapter Configuration
 
Arguments:

    ValueName - Name of the Binary Data to be assigned.
 
    Buffer - The buffer holding the binary data
 
    BufferLength - The length of the buffer in bytes. 
--*/
{
    FuncEntry(FLAG_CONFIGURATION);
    
    NTSTATUS status;
    NDIS_STATUS ndisStatus;
    NDIS_CONFIGURATION_PARAMETER ndisConfigurationParam;

    RtlZeroMemory(&ndisConfigurationParam, sizeof(NDIS_CONFIGURATION_PARAMETER));

    ndisConfigurationParam.ParameterType = NdisParameterBinary; 

    if (BufferLength >= USHORT_MAX) {
        LogError(GetRecorderLog(), FLAG_CONFIGURATION,
                 "Buffer size is too long: %d", BufferLength);
        FuncExit(FLAG_CONFIGURATION);
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
        FuncExit(FLAG_CONFIGURATION);
        return status;
    }

    status = STATUS_SUCCESS;
    
    FuncExit(FLAG_CONFIGURATION);
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

    This method is assigns a Multi String to the Adapter Configuration
 
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
    FuncEntry(FLAG_CONFIGURATION);
    
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
    FuncExit(FLAG_CONFIGURATION);
    return status;
}

