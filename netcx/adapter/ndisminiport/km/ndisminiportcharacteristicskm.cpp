// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include <ndis/lwminiportapi.h>

#include "NdisMiniportCharacteristics.hpp"
#include "NxAdapter.hpp"

#include "NdisMiniportCharacteristicsKm.tmh"

DECLARE_CONST_UNICODE_STRING(
    NetAdapterCxServiceName,
    L"NetAdapterCx");

static
MINIPORT_INITIALIZE
    EvtNdisInitializeEx;

static
MINIPORT_HALT
    EvtNdisHaltEx;

static
MINIPORT_PAUSE
    EvtNdisPause;

static
MINIPORT_RESTART
    EvtNdisRestart;

static
MINIPORT_SEND_NET_BUFFER_LISTS
    EvtNdisSendNetBufferLists;

static
MINIPORT_RETURN_NET_BUFFER_LISTS
    EvtNdisReturnNetBufferLists;

static
MINIPORT_CANCEL_SEND
    EvtNdisCancelSend;

static
MINIPORT_DEVICE_PNP_EVENT_NOTIFY
    EvtNdisDevicePnpEventNotify;

static
MINIPORT_SHUTDOWN
    EvtNdisShutdownEx;

static
MINIPORT_OID_REQUEST
    EvtNdisOidRequest;

static
MINIPORT_DIRECT_OID_REQUEST
    EvtNdisDirectOidRequest;

static
MINIPORT_CANCEL_OID_REQUEST
    EvtNdisCancelOidRequest;

static
MINIPORT_CANCEL_DIRECT_OID_REQUEST
    EvtNdisCancelDirectOidRequest;

static
MINIPORT_SYNCHRONOUS_OID_REQUEST
    EvtNdisSynchronousOidRequestHandler;

_Use_decl_annotations_
void
NdisMiniportDriverCharacteristicsInitialize(
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS & Characteristics
)
{
    Characteristics = {};

    Characteristics.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    Characteristics.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3;
    Characteristics.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_3;
    Characteristics.MajorNdisVersion = NDIS_MINIPORT_MAJOR_VERSION;
    Characteristics.MinorNdisVersion = NDIS_MINIPORT_MINOR_VERSION;

    //
    // NDIS should not hold driver version information of WDF managed miniports
    //
    Characteristics.MajorDriverVersion = 0;
    Characteristics.MinorDriverVersion = 0;

    Characteristics.Flags = 0;
    Characteristics.Flags |= NDIS_NETCX_DRIVER; // prevent NDIS from owning the driver dispatch table
    Characteristics.Flags |= NDIS_DRIVER_POWERMGMT_PROXY; // this is a virtual device so need updated capabilities

    Characteristics.InitializeHandlerEx = EvtNdisInitializeEx;
    Characteristics.HaltHandlerEx = EvtNdisHaltEx;
    Characteristics.PauseHandler = EvtNdisPause;
    Characteristics.RestartHandler = EvtNdisRestart;
    Characteristics.OidRequestHandler = EvtNdisOidRequest;
    Characteristics.DirectOidRequestHandler = EvtNdisDirectOidRequest;
    Characteristics.SendNetBufferListsHandler = EvtNdisSendNetBufferLists;
    Characteristics.ReturnNetBufferListsHandler = EvtNdisReturnNetBufferLists;
    Characteristics.CancelSendHandler = EvtNdisCancelSend;
    Characteristics.DevicePnPEventNotifyHandler = EvtNdisDevicePnpEventNotify;
    Characteristics.ShutdownHandlerEx = EvtNdisShutdownEx;
    Characteristics.CancelOidRequestHandler = EvtNdisCancelOidRequest;
    Characteristics.CancelDirectOidRequestHandler = EvtNdisCancelDirectOidRequest;
    Characteristics.SynchronousOidRequestHandler = EvtNdisSynchronousOidRequestHandler;
}

_Use_decl_annotations_
void
NdisLWMiniportDriverCharacteristicsInitialize(
    NDIS_LWM_DRIVER_CHARACTERISTICS & Characteristics
)
{
    Characteristics = {};

    Characteristics.Header.Type = NDIS_OBJECT_TYPE_LWM_DRIVER_CHARACTERISTICS;
    Characteristics.Header.Revision = NDIS_LWM_DRIVER_CHARACTERISTICS_REVISION_1;
    Characteristics.Header.Size = NDIS_SIZEOF_LWM_DRIVER_CHARACTERISTICS_REVISION_1;

    Characteristics.MajorNdisVersion = NDIS_MINIPORT_MAJOR_VERSION;
    Characteristics.MinorNdisVersion = NDIS_MINIPORT_MINOR_VERSION;
    Characteristics.MajorDriverVersion = 0;
    Characteristics.MinorDriverVersion = 0;

    Characteristics.Flags = 0;
    Characteristics.Flags |= NDIS_NETCX_DRIVER;

    Characteristics.ServiceName = NetAdapterCxServiceName;
    Characteristics.PauseHandler = EvtNdisPause;
    Characteristics.RestartHandler = EvtNdisRestart;
    Characteristics.OidRequestHandler = EvtNdisOidRequest;
    Characteristics.SendNetBufferListsHandler = EvtNdisSendNetBufferLists;
    Characteristics.ReturnNetBufferListsHandler = EvtNdisReturnNetBufferLists;
    Characteristics.CancelSendHandler = EvtNdisCancelSend;
    Characteristics.CancelOidRequestHandler = EvtNdisCancelOidRequest;
    Characteristics.DirectOidRequestHandler = EvtNdisDirectOidRequest;
    Characteristics.CancelDirectOidRequestHandler = EvtNdisCancelDirectOidRequest;
    Characteristics.SynchronousOidRequestHandler = EvtNdisSynchronousOidRequestHandler;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisInitializeEx(
    NDIS_HANDLE NdisAdapterHandle,
    NDIS_HANDLE UnusedContext,
    NDIS_MINIPORT_INIT_PARAMETERS * MiniportInitParameters
)
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to initialize a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called ndisPnPStartDevice routine.

Arguments:

    NdisAdapterHandle - Opaque Ndis handle for the Adapter

Return Value:

    NDIS_STATUS_xxx code

--*/
{
    UNREFERENCED_PARAMETER((UnusedContext, MiniportInitParameters));

    auto nxAdapter = static_cast<NxAdapter *>(
        NdisWdfGetAdapterContextFromAdapterHandle(NdisAdapterHandle));

    return NdisConvertNtStatusToNdisStatus(nxAdapter->NdisInitialize());
}

_Use_decl_annotations_
void
EvtNdisHaltEx(
    NDIS_HANDLE Adapter,
    NDIS_HALT_ACTION HaltAction
)
/*++
Routine Description:

    A NDIS Event Callback.

    This Ndis event callback is called by Ndis to halt a
    new ndis miniport adapter.

    This callback runs at IRQL = PASSIVE_LEVEL.

    This callback is (must be) called on the same thread on which NetAdapterCx
    called NdisWdfPnpAddDevice routine.

Arguments:

    Adapter - NxAdapter reported as Context to NDIS.sys

--*/
{
    UNREFERENCED_PARAMETER(HaltAction);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportHalt);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisPause(
    NDIS_HANDLE Adapter,
    NDIS_MINIPORT_PAUSE_PARAMETERS * PauseParameters
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to pause the adapter.
    NetAdapterCx in response calls the clients EvtAdapterPause callback.

--*/
{
    UNREFERENCED_PARAMETER(PauseParameters);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportPause);

    return NDIS_STATUS_PENDING;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisRestart(
    NDIS_HANDLE Adapter,
    NDIS_MINIPORT_RESTART_PARAMETERS * RestartParameters
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to start (restart) the adapter.
    NetAdapterCx in response calls the clients EvtAdapterStart callback.

--*/
{
    UNREFERENCED_PARAMETER(RestartParameters);

    auto adapter = static_cast<NxAdapter *>(Adapter);
    adapter->EnqueueEventSync(NxAdapter::Event::NdisMiniportRestart);

    return NDIS_STATUS_PENDING;
}

_Use_decl_annotations_
void
EvtNdisSendNetBufferLists(
    NDIS_HANDLE Adapter,
    NET_BUFFER_LIST * NetBufferLists,
    NDIS_PORT_NUMBER PortNumber,
    ULONG SendFlags
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to send data over the network the adapter.

    NetAdapterCx in response calls the clients EvtAdapterSendNetBufferLists callback.

--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->SendNetBufferLists(
        NetBufferLists,
        PortNumber,
        SendFlags);
}

_Use_decl_annotations_
void
EvtNdisReturnNetBufferLists(
    NDIS_HANDLE Adapter,
    NET_BUFFER_LIST * NetBufferLists,
    ULONG ReturnFlags
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to return a NBL to the miniport.

    NetAdapterCx in response calls the clients EvtAdapterReturnNetBufferLists callback.

--*/
{
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);
    nxAdapter->ReturnNetBufferLists(
        NetBufferLists,
        ReturnFlags);
}

_Use_decl_annotations_
void
EvtNdisCancelSend(
    NDIS_HANDLE Adapter,
    void * CancelId
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to cancel a send NBL request.

--*/
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(CancelId);

    NT_ASSERTMSG("Missing Implementation", FALSE);
}

_Use_decl_annotations_
void
EvtNdisDevicePnpEventNotify(
    NDIS_HANDLE Adapter,
    NET_DEVICE_PNP_EVENT * NetDevicePnPEvent
)
/*++
Routine Description:

    A NDIS Event callback.

    This event callback is called by NDIS to report a Pnp Event.

--*/
{
    UNREFERENCED_PARAMETER(Adapter);

    if (NetDevicePnPEvent->DevicePnPEvent == NdisDevicePnPEventPowerProfileChanged) {
        if (NetDevicePnPEvent->InformationBufferLength == sizeof(ULONG)) {
            ULONG NdisPowerProfile = *((PULONG)NetDevicePnPEvent->InformationBuffer);

            if (NdisPowerProfile == NdisPowerProfileBattery) {
                LogInfo(FLAG_ADAPTER, "On NdisPowerProfileBattery");
            } else if (NdisPowerProfile == NdisPowerProfileAcOnLine) {
                LogInfo(FLAG_ADAPTER, "On NdisPowerProfileAcOnLine");
            } else {
                LogError(FLAG_ADAPTER, "Unexpected PowerProfile %d", NdisPowerProfile);
                NT_ASSERTMSG("Unexpected PowerProfile", FALSE);
            }
        }
    } else {
        LogInfo(FLAG_ADAPTER, "PnpEvent %!NDIS_DEVICE_PNP_EVENT! from Ndis.sys", NetDevicePnPEvent->DevicePnPEvent);
    }
}

_Use_decl_annotations_
void
EvtNdisShutdownEx(
    NDIS_HANDLE Adapter,
    NDIS_SHUTDOWN_ACTION ShutdownAction
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(ShutdownAction);

    NT_ASSERTMSG("Missing Implementation", FALSE);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisOidRequest(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * NdisOidRequest
)
{
    NDIS_STATUS ndisStatus;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    // intercept and forward to the NDIS translator.
    if (nxAdapter->NetClientOidPreProcess(*NdisOidRequest, ndisStatus))
    {
        return ndisStatus;
    }

    // NDIS_PM_PARAMETERS is always delivered through EvtNdisUpdatePMParameters, but in certain scenarios
    // NDIS will also send OID_PM_PARAMETERS with the same information to keep legacy behavior. There is
    // no reason to show that OID to media extensions though, so complete it right here to avoid confusion.
    if (NdisOidRequest->DATA.SET_INFORMATION.Oid == OID_PM_PARAMETERS)
    {
        return NDIS_STATUS_SUCCESS;
    }

    auto dispatchContext = new (&NdisOidRequest->MiniportReserved[0]) DispatchContext();
    dispatchContext->Signature = OidSignature;

    nxAdapter->DispatchOidRequest(NdisOidRequest, dispatchContext);

    return NDIS_STATUS_PENDING;
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisDirectOidRequest(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * Request
)
{
    NDIS_STATUS ndisStatus;

    // intercept and forward to the NDIS translator.
    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    if (nxAdapter->NetClientDirectOidPreProcess(*Request, ndisStatus))
    {
        return ndisStatus;
    }

    auto dispatchContext = new (&Request->MiniportReserved[0]) DispatchContext();
    dispatchContext->Signature = DirectOidSignature;

    auto const ntStatus = nxAdapter->DispatchDirectOidRequest(Request, dispatchContext);

    return NdisConvertNtStatusToNdisStatus(ntStatus);
}

_Use_decl_annotations_
void
EvtNdisCancelOidRequest(
    NDIS_HANDLE Adapter,
    void * RequestId
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(RequestId);
}

_Use_decl_annotations_
void
EvtNdisCancelDirectOidRequest(
    NDIS_HANDLE Adapter,
    void * RequestId
)
{
    UNREFERENCED_PARAMETER(Adapter);
    UNREFERENCED_PARAMETER(RequestId);
}

_Use_decl_annotations_
NDIS_STATUS
EvtNdisSynchronousOidRequestHandler(
    NDIS_HANDLE Adapter,
    NDIS_OID_REQUEST * Request
)
{
    NDIS_STATUS ndisStatus;

    auto nxAdapter = static_cast<NxAdapter *>(Adapter);

    // intercept and forward to the NDIS translator.
    if (nxAdapter->NetClientSynchrohousOidPreProcess(*Request, ndisStatus))
    {
        return ndisStatus;
    }

    return NdisConvertNtStatusToNdisStatus(STATUS_NOT_SUPPORTED);
}
