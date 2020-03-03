// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxDeviceApi.tmh"

#include "NxDevice.hpp"
#include "NxConfiguration.hpp"
#include "verifier.hpp"
#include "version.hpp"

WDFAPI
_Must_inspect_result_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NETEXPORT(NetDeviceOpenConfiguration)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ WDFDEVICE Device,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * ConfigurationAttributes,
    _Out_ NETCONFIGURATION* Configuration
)
/*++
Routine Description:

    This routine opens the default configuration of the Device object
Arguments:

    Device - Pointer to the Device created in a prior call

    ConfigurationAttributes - A pointer to a WDF_OBJECT_ATTRIBUTES structure
        that contains driver-supplied attributes for the new configuration object.
        This parameter is optional and can be WDF_NO_OBJECT_ATTRIBUTES.

        The Configuration object is parented to the Device object thus
        the caller must not specify a parent object.

    Configuration - (output) Address of a location that receives a handle to the
        device configuration object.

Returns:
     NTSTATUS

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    *Configuration = NULL;

    NxConfiguration * nxConfiguration;
    CX_RETURN_IF_NOT_NT_SUCCESS(NxConfiguration::_Create(pNxPrivateGlobals,
        GetNxDeviceFromHandle(Device),
        ConfigurationAttributes,
        NULL,
        &nxConfiguration));

    *Configuration = nxConfiguration->GetFxObject();

    return STATUS_SUCCESS;
}

WDFAPI
_Must_inspect_result_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NETEXPORT(NetDeviceAssignSupportedOidList)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ WDFDEVICE Device,
    _In_ NDIS_OID const * SupportedOids,
    _In_ SIZE_T SupportedOidsCount
)
/*++
Routine Description:

    This routine sets the OIDs supported by the device

Arguments:

    Device - Pointer to the Device created in a prior call

    SupportedOids - A pointer to the OID list that the device supports

    SupportedOidsCount - Count of OIDs supported

--*/
{
    auto pNxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyIsMediaExtension(pNxPrivateGlobals);
    Verifier_VerifyIrqlPassive(pNxPrivateGlobals);

    auto nxDevice = GetNxDeviceFromHandle(Device);
    return nxDevice->AssignSupportedOidList(SupportedOids, SupportedOidsCount);
}

