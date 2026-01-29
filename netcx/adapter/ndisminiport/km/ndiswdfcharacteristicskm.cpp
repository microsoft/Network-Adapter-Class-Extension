// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NdisWdfCharacteristics.hpp"
#include "NxAdapter.hpp"
#include "NxDevice.hpp"

#include "NdisWdfCharacteristicsKm.tmh"

static
EVT_NDIS_WDF_MINIPORT_GET_NDIS_HANDLE_FROM_DEVICE_OBJECT
    EvtNdisWdmDeviceGetNdisAdapterHandle;

static
EVT_NDIS_WDF_MINIPORT_GET_DEVICE_OBJECT
    EvtNdisGetDeviceObject;

static
EVT_NDIS_WDF_MINIPORT_GET_NEXT_DEVICE_OBJECT
    EvtNdisGetNextDeviceObject;

static
EVT_NDIS_WDF_MINIPORT_GET_ASSIGNED_FDO_NAME
    EvtNdisGetAssignedFdoName;

static
EVT_NDIS_WDF_MINIPORT_POWER_REFERENCE
    EvtNdisPowerReference;

static
EVT_NDIS_WDF_MINIPORT_POWER_DEREFERENCE
    EvtNdisPowerDereference;

static
EVT_NDIS_WDF_MINIPORT_UPDATE_IDLE_CONDITION
    EvtNdisUpdateIdleCondition;

static
EVT_NDIS_WDF_MINIPORT_AOAC_ENGAGE
    EvtNdisAoAcEngage;

static
EVT_NDIS_WDF_MINIPORT_AOAC_DISENGAGE
    EvtNdisAoAcDisengage;

static
EVT_NDIS_WDF_MINIPORT_UPDATE_PM_PARAMETERS
    EvtNdisUpdatePMParameters;

static
EVT_NDIS_WDF_MINIPORT_ALLOCATE_BLOCK
    EvtNdisAllocateMiniportBlock;

static
EVT_NDIS_WDF_MINIPORT_COMPLETE_ADD
    EvtNdisMiniportCompleteAdd;

static
EVT_NDIS_WDF_DEVICE_START_COMPLETE
    EvtNdisDeviceStartComplete;

static
EVT_NDIS_WDF_MINIPORT_DEVICE_RESET
    EvtNdisMiniportDeviceReset;

static
EVT_NDIS_WDF_MINIPORT_QUERY_DEVICE_RESET_SUPPORT
    EvtNdisMiniportQueryDeviceResetSupport;

static
EVT_NDIS_WDF_GET_WMI_EVENT_GUID
    EvtNdisGetWmiEventGuid;

_Use_decl_annotations_
void
NdisWdfCxCharacteristicsInitialize(
    NDIS_WDF_CX_CHARACTERISTICS & Characteristics
)
{
    NDIS_WDF_CX_CHARACTERISTICS_INIT(&Characteristics);

    Characteristics.EvtCxGetDeviceObject = EvtNdisGetDeviceObject;
    Characteristics.EvtCxGetNextDeviceObject = EvtNdisGetNextDeviceObject;
    Characteristics.EvtCxGetAssignedFdoName = EvtNdisGetAssignedFdoName;
    Characteristics.EvtCxPowerReference = EvtNdisPowerReference;
    Characteristics.EvtCxPowerDereference = EvtNdisPowerDereference;
    Characteristics.EvtCxUpdateIdleCondition = EvtNdisUpdateIdleCondition;
    Characteristics.EvtCxGetNdisHandleFromDeviceObject = EvtNdisWdmDeviceGetNdisAdapterHandle;
    Characteristics.EvtCxUpdatePMParameters = EvtNdisUpdatePMParameters;
    Characteristics.EvtCxAllocateMiniportBlock = EvtNdisAllocateMiniportBlock;
    Characteristics.EvtCxMiniportCompleteAdd = EvtNdisMiniportCompleteAdd;
    Characteristics.EvtCxDeviceStartComplete = EvtNdisDeviceStartComplete;
    Characteristics.EvtCxMiniportDeviceReset = EvtNdisMiniportDeviceReset;
    Characteristics.EvtCxMiniportQueryDeviceResetSupport = EvtNdisMiniportQueryDeviceResetSupport;
    Characteristics.EvtCxGetWmiEventGuid = EvtNdisGetWmiEventGuid;
}

_Use_decl_annotations_
DEVICE_OBJECT *
EvtNdisGetDeviceObject(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the WDM Device Object for the clients FDO

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    return WdfDeviceWdmGetDeviceObject(static_cast<NxAdapter *>(Adapter)->GetDevice());
}

_Use_decl_annotations_
DEVICE_OBJECT *
EvtNdisGetNextDeviceObject(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the attached WDM Device Object for the clients FDO

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    return WdfDeviceWdmGetAttachedDevice(static_cast<NxAdapter *>(Adapter)->GetDevice());
}

_Use_decl_annotations_
NTSTATUS
EvtNdisGetAssignedFdoName(
    NDIS_HANDLE Adapter,
    UNICODE_STRING * FdoName
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine returns the name NetAdapterCx assigned to this device. NDIS
    needs to know this name to be able to create a symbolic link for the miniport.

Arguments:
    Adapter            - The NxAdapter object reported as context to NDIS
    FdoName            - UNICODE_STRING that will receive the FDO name
--*/
{
    NTSTATUS       status;
    WDFSTRING      fdoName;
    UNICODE_STRING usFdoName;

    status = WdfStringCreate(
        NULL,
        WDF_NO_OBJECT_ATTRIBUTES,
        &fdoName);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = WdfDeviceRetrieveDeviceName(static_cast<NxAdapter *>(Adapter)->GetDevice(), fdoName);

    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    WdfStringGetUnicodeString(fdoName, &usFdoName);

    if (FdoName->MaximumLength < usFdoName.Length)
    {
        status = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    RtlCopyUnicodeString(FdoName, &usFdoName);

Exit:

    WdfObjectDelete(fdoName);
    return status;
}

_Use_decl_annotations_
NTSTATUS
EvtNdisPowerReference(
    NDIS_HANDLE Adapter,
    BOOLEAN WaitForD0,
    BOOLEAN InvokeCompletionCallback
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine is called to acquire a power reference on the miniport.
    Every call to EvtNdisPowerReference must be matched by a call
    to EvtNdisPowerDereference. However, this function
    keeps tracks of failures from the underlying call to WdfDeviceStopIdle. So the
    caller only needs to make sure they always call power EvtNdisPowerDereference
    after calling EvtNdisPowerReference without keeping track of returned failures.

Arguments:
    Adapter   - The NxAdapter object reported as context to NDIS
    WaitForD0 - If WaitFromD0 is false and STATUS_PENDING status is returned
        the Cx will call back into NDIS once power up is complete.
    InvokeCompletionCallback - IFF WaitForD0 is FALSE (i.e. Async power ref),
        then the caller can ask for a callback when power up is complete
--*/
{
    NT_ASSERTMSG("Invalid parameter to NdisPowerRef",
                !(WaitForD0 && InvokeCompletionCallback));

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    if (InvokeCompletionCallback)
    {
        auto const ntStatus = nxAdapter->PowerReference(false);

        if (ntStatus == STATUS_PENDING)
        {
            // If the device is resuming from low power we need to schedule a work
            // item to invoke the NDIS completion callback.
            nxAdapter->QueuePowerReferenceWorkItem();
            nxAdapter->SetWakeReason(NetWakeReasonTypeNdis);
        }

        return ntStatus;
    }
    else
    {
        return nxAdapter->PowerReference(WaitForD0);
    }
}

_Use_decl_annotations_
NTSTATUS
EvtNdisAllocateMiniportBlock(
    NDIS_HANDLE Adapter,
    ULONG Size,
    void ** MiniportBlock
)
/*++
Routine Description:
    This function will allocate the memory for the adapter's miniport block as
    a context in the NETADAPTER object. This way we can ensure the miniport block
    will be valid memory during the adapter's life time.
--*/
{

    NTSTATUS status = STATUS_SUCCESS;

    WDF_OBJECT_ATTRIBUTES contextAttributes;

    // Initialize contextAttributes with our dummy type
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &contextAttributes,
        NDIS_MINIPORT_BLOCK_TYPE);

    // Override the memory size to allocate with what NDIS told us
    contextAttributes.ContextSizeOverride = Size;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    status = WdfObjectAllocateContext(nxAdapter->GetFxObject(), &contextAttributes, MiniportBlock);

    if (!NT_SUCCESS(status))
    {
        LogError(FLAG_ADAPTER,
            "WdfObjectAllocateContext failed to allocate the miniport block, %!STATUS!", status);

        return status;
    }

    return status;
}

_Use_decl_annotations_
void
EvtNdisMiniportCompleteAdd(
    NDIS_HANDLE Adapter,
    NDIS_WDF_COMPLETE_ADD_PARAMS * Params
)
/*++
Routine Description:
    NDIS uses this callback to report certain adapter parameters to the NIC
    after the Miniport side of device add has completed.
--*/
{

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->NdisMiniportCreationComplete(*Params);
}

_Use_decl_annotations_
void
EvtNdisDeviceStartComplete(
    NDIS_HANDLE Adapter
)
/*
Description:

    ATTENTION: This is being used by NDIS to indicate a stuck NBL was detected on this adapter.
*/
{
    UNREFERENCED_PARAMETER(Adapter);
}


_Use_decl_annotations_
NTSTATUS
EvtNdisMiniportDeviceReset(
    NDIS_HANDLE Adapter,
    NET_DEVICE_RESET_TYPE NetDeviceResetType
)
/*
Routine Description:

    NDIS uses this callback to indicate the underlying device is in a bad state
    and it needs to be reset in order to recover.

Arguments:
    Adapter            - The NxAdapter object reported as context to NDIS
    NetDeviceResetType - The type of reset operation requested (by default this is PLDR)
*/
{
    DEVICE_RESET_TYPE DeviceResetType;
    NxAdapter *nxAdapter = static_cast<NxAdapter *>(Adapter);
    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    nxDevice->SetFailingDeviceRequestingResetFlag();

    // Trigger reset of the device as we are told to do so
    switch (NetDeviceResetType)
    {
    case FunctionLevelReset:
        DeviceResetType = FunctionLevelDeviceReset;
        break;
    case PlatformLevelReset:
        DeviceResetType = PlatformLevelDeviceReset;
        break;
    default:
        DeviceResetType = PlatformLevelDeviceReset;
        break;
    };

    return nxDevice->DispatchDeviceReset(DeviceResetType);
}

_Use_decl_annotations_
NTSTATUS
EvtNdisMiniportQueryDeviceResetSupport(
    NDIS_HANDLE Adapter,
    PULONG SupportedNetDeviceResetTypes
)
/*
Routine Description:

    NDIS uses this callback to query whether Device Reset is supported by the underlying
    device for which there is a NetAdapter based driver present

Arguments:
    Adapter                      - The NxAdapter object reported as context to NDIS
    SupportedNetDeviceResetTypes - Out parameter which will have the information about what all reset types is supported
*/
{
    NxAdapter *nxAdapter = static_cast<NxAdapter *>(Adapter);
    NxDevice *nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    if (nxDevice->GetSupportedDeviceResetType(SupportedNetDeviceResetTypes))
    {
        return STATUS_SUCCESS;
    }
    else
    {
        return STATUS_NOT_SUPPORTED;
    }
}

_Use_decl_annotations_
void
EvtNdisPowerDereference(
    NDIS_HANDLE Adapter
)
/*++
Routine Description:
    A Ndis Event Callback

    This routine is called to release a power reference on the
    miniport that it previously acquired. Every call to EvtNdisPowerReference
    MUST be matched by a call to EvtNdisPowerDereference.
    However, since failures are tracked from calls to EvtNdisPowerReference, the
    caller can call EvtNdisPowerDereference without having to keep track of
    failures from calls to EvtNdisPowerReference.

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->PowerDereference();
}

_Use_decl_annotations_
NTSTATUS
EvtNdisGetWmiEventGuid(
    NDIS_HANDLE Adapter,
    NTSTATUS GuidStatus,
    NDIS_GUID ** Guid
)
{
    auto const nxAdapter = static_cast<NxAdapter *>(Adapter);
    auto const nxDevice = GetNxDeviceFromHandle(nxAdapter->GetDevice());

    return nxDevice->WmiGetEventGuid(GuidStatus, Guid);
}

_Use_decl_annotations_
void
EvtNdisUpdateIdleCondition(
    NDIS_HANDLE Adapter,
    NDIS_IDLE_CONDITION IdleCondition
)
/*++
Routine Description:
    An Ndis Event Callback

    This routine is called to notify adapter of an update to the
    Idle Condition so that it release / acquire WDF power references
    based upon idle restriction policy.

Arguments:
    Adapter - The NxAdapter object reported as context to NDIS
    IdleCondition - New idle condition
--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->m_powerPolicy->SetCurrentIdleCondition(IdleCondition);
}

_Use_decl_annotations_
NDIS_HANDLE
EvtNdisWdmDeviceGetNdisAdapterHandle(
    DEVICE_OBJECT * DeviceObject
)
/*++
Routine Description:
    A Ndis Event Callback

    This event callback converts a WDF Device Object to the NDIS's
    miniport Adapter handle.
--*/
{
    // It does not make sense to get an adapter handle from a DEVICE_OBJECT.
    // This interface does not provide an adapter handle in this path.
    UNREFERENCED_PARAMETER(DeviceObject);
    return nullptr;
}

_Use_decl_annotations_
void
EvtNdisUpdatePMParameters(
    NDIS_HANDLE Adapter,
    NDIS_PM_PARAMETERS * PmParameters
)
{
    auto nxAdapter = reinterpret_cast<NxAdapter *>(Adapter);
    nxAdapter->UpdateNdisPmParameters(PmParameters);
}
