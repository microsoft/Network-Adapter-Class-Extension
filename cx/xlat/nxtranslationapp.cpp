// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Implements the driver-global state for the NBL-to-NET_PACKET translation
    app in NetAdapterCx.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndisoidtypes.h>

#include "NxTranslationApp.tmh"

#include "NxXlat.hpp"
#include "NxTranslationApp.hpp"
#include "NxPerfTuner.hpp"

static
NTSTATUS
NetClientAdapterCreateDatapath(
    _In_ PVOID ClientContext
    )
{
    return reinterpret_cast<NxTranslationApp *>(ClientContext)->CreateDatapath();
}

static
void
NetClientAdapterDestroyDatapath(
    _In_ PVOID ClientContext
    )
{
    reinterpret_cast<NxTranslationApp *>(ClientContext)->DestroyDatapath();
}

static
BOOLEAN
NetClientAdapterNdisOidRequestHandler(
    _In_ PVOID ClientContext,
    _In_ NDIS_OID_REQUEST * Request,
    _Out_ NTSTATUS * Status
    )
{
    auto app = reinterpret_cast<NxTranslationApp *>(ClientContext);
    bool handled = false;

    switch (Request->RequestType)
    {

    case NdisRequestSetInformation:

        switch (Request->DATA.SET_INFORMATION.Oid)
        {

        case OID_GEN_RECEIVE_SCALE_PARAMETERS:
            *Status = app->ReceiveScalingParametersSet(*Request);
            handled = true;
            break;

        }
        break;

    }

    return handled;
}

static
BOOLEAN
NetClientAdapterNdisDirectOidRequestHandler(
    _In_ PVOID ClientContext,
    _In_ NDIS_OID_REQUEST * Request,
    _Out_ NTSTATUS * Status
    )
{
    UNREFERENCED_PARAMETER(ClientContext);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Status);

    return false;
}

static
BOOLEAN
NetClientAdapterNdisSynchronousOidRequestHandler(
    _In_ PVOID ClientContext,
    _In_ NDIS_OID_REQUEST * Request,
    _Out_ NTSTATUS * Status
    )
{
    UNREFERENCED_PARAMETER(ClientContext);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Status);

    return false;
}

static const NET_CLIENT_CONTROL_DISPATCH ControlDispatch =
{
    sizeof(NET_CLIENT_CONTROL_DISPATCH),
    &NetClientAdapterCreateDatapath,
    &NetClientAdapterDestroyDatapath,
    &NetClientAdapterNdisOidRequestHandler,
    &NetClientAdapterNdisDirectOidRequestHandler,
    &NetClientAdapterNdisSynchronousOidRequestHandler,
};

NxTranslationAppFactory::~NxTranslationAppFactory()
{
    NxPerfTunerCleanup();
}

NTSTATUS
NxTranslationAppFactory::Initialize()
{
    CX_RETURN_IF_NOT_NT_SUCCESS(NxPerfTunerInitialize());

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationAppFactory::CreateApp(
    NET_CLIENT_DISPATCH const * Dispatch,
    NET_CLIENT_ADAPTER Adapter,
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch,
    void ** ClientContext,
    NET_CLIENT_CONTROL_DISPATCH const ** ClientDispatch
    )
{
    auto newApp = wil::make_unique_nothrow<NxTranslationApp>(Dispatch, Adapter, AdapterDispatch);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !newApp);

    *ClientContext = newApp.release();
    *ClientDispatch = &ControlDispatch;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxTranslationApp::NxTranslationApp(
    NET_CLIENT_DISPATCH const * Dispatch,
    NET_CLIENT_ADAPTER Adapter,
    NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
    ) noexcept :
    m_dispatch(Dispatch),
    m_adapter(Adapter),
    m_adapterDispatch(AdapterDispatch)
{
}

PAGED 
NTSTATUS
NxTranslationApp::StartTx(
    void
    )
{
    auto txQueue = wil::make_unique_nothrow<NxTxXlat>(m_dispatch, m_adapter, m_adapterDispatch);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !txQueue);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        txQueue->StartQueue(),
        "Failed to initializate the Tx translation queue. NxTranslationApp=%p", this);

    m_txQueue = wistd::move(txQueue);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PAGEDX
NTSTATUS
NxTranslationApp::StartRx(
    void
    )
{
    auto rxQueue = wil::make_unique_nothrow<NxRxXlat>(m_dispatch, m_adapter, m_adapterDispatch);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        !rxQueue);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        rxQueue->StartQueue(),
        "Failed to initializate the Rx translation queue. NxTranslationApp=%p", this);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_rxQueues.append(wistd::move(rxQueue)));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::CreateDatapath(void)
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        StartTx(),
        "Failed to start Tx translation queue. NxTranslationApp=%p,Adapter=%p", this, m_adapter);

    NET_CLIENT_ADAPTER_PROPERTIES adapterProperties;
    m_adapterDispatch->GetProperties(m_adapter, &adapterProperties);
    m_NblDispatcher = static_cast<INxNblDispatcher *>(adapterProperties.NblDispatcher);
    m_NblDispatcher->SetRxHandler(&m_rxBufferReturn);

    if (m_receiveScaleMappings.count() == 0)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            StartRx(),
            "Failed to start Rx translation queue. NxTranslationApp=%p,Adapter=%p", this, m_adapter);

        return STATUS_SUCCESS;
    }

    // receive scaling is enabled

    NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES table = { 0, {} };

    for (size_t i = 0; i < m_receiveScaleMappings.count(); i++)
    {
        auto & mapping = m_receiveScaleMappings[i];

        // start a queue
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            StartRx(),
            "Failed to start Rx translation queue. NxTranslationApp=%p,Adapter=%p", this, m_adapter);

        mapping.QueueIndex = m_rxQueues.count() - 1;

        auto & queue = m_rxQueues[mapping.QueueIndex];

        // only used when for dynamic receive scaling.
        mapping.QueueId = queue->GetQueueId();
        mapping.QueueCreated = true;

        // affinity to processor
        GROUP_AFFINITY affinity = { 1ULL << mapping.Number.Number, mapping.Number.Group };
        (void)queue->SetGroupAffinity(affinity);

        for (auto const index : mapping.HashIndexes)
        {
            NT_FRE_ASSERT(table.Count < ARRAYSIZE(table.Entries));

            table.Entries[table.Count++] = { queue->GetQueue(), STATUS_SUCCESS, index, };

        }
    }

    return m_adapterDispatch->ReceiveScalingSetIndirectionEntries(m_adapter, &table);
}

bool
NxTranslationApp::ReceiveScalingEvaluateDisable(
    NDIS_RECEIVE_SCALE_PARAMETERS const & Parameters
    )
{
    bool const disableFlag = WI_IsFlagSet(Parameters.Flags, NDIS_RSS_PARAM_FLAG_DISABLE_RSS);
    bool const hashInfoZero = (! WI_IsFlagSet(Parameters.Flags, NDIS_RSS_PARAM_FLAG_HASH_INFO_UNCHANGED)) &&
        Parameters.HashInformation == 0;
    if (disableFlag || hashInfoZero)
    {
        m_adapterDispatch->ReceiveScalingDisable(m_adapter);

        return true;
    }

    return false;
}

NTSTATUS
NxTranslationApp::ReceiveScalingEvaluateEnable(
    NDIS_RECEIVE_SCALE_PARAMETERS const & Parameters
    )
{
    NT_FRE_ASSERT(! WI_IsFlagSet(Parameters.Flags, NDIS_RSS_PARAM_FLAG_DISABLE_RSS));
    NT_FRE_ASSERT(! WI_IsFlagSet(Parameters.Flags, NDIS_RSS_PARAM_FLAG_HASH_INFO_UNCHANGED));
    NT_FRE_ASSERT(Parameters.HashInformation != 0);

    auto const hashFunction = NDIS_RSS_HASH_FUNC_FROM_HASH_INFO(Parameters.HashInformation);
    auto const hashType = NDIS_RSS_HASH_TYPE_FROM_HASH_INFO(Parameters.HashInformation);

    return m_adapterDispatch->ReceiveScalingEnable(m_adapter, hashFunction, hashType);
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReceiveScalingParametersSet(
    NDIS_OID_REQUEST & Request
    )
{
    auto const & data = Request.DATA.SET_INFORMATION;
    auto const & parameters = *static_cast<NDIS_RECEIVE_SCALE_PARAMETERS const *>(data.InformationBuffer);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        data.InformationBufferLength < sizeof(parameters));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        parameters.HashSecretKeyOffset < sizeof(parameters));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        parameters.IndirectionTableOffset < sizeof(parameters));

    CX_RETURN_NTSTATUS_IF(
        STATUS_SUCCESS,
        ReceiveScalingEvaluateDisable(parameters));

    size_t size = sizeof(parameters);
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlSizeTAdd(
            parameters.HashSecretKeySize,
            size, &size));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlSizeTAdd(
            parameters.IndirectionTableSize,
            size, &size));

    ULONG_PTR parameterBufferEnd;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        RtlULongPtrAdd(
            (ULONG_PTR)data.InformationBuffer,
            (ULONG_PTR)size,
            &parameterBufferEnd));

    ULONG_PTR hashSecretKeyBuffer;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        RtlULongPtrAdd(
            (ULONG_PTR)data.InformationBuffer,
            (ULONG_PTR)parameters.HashSecretKeyOffset,
            &hashSecretKeyBuffer));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        ! (hashSecretKeyBuffer < parameterBufferEnd));

    ULONG_PTR indirectionEntriesBuffer;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        RtlULongPtrAdd(
            (ULONG_PTR)data.InformationBuffer,
            (ULONG_PTR)parameters.IndirectionTableOffset,
            &indirectionEntriesBuffer));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        ! (indirectionEntriesBuffer < parameterBufferEnd));

    // XXX we should also check that hash secret key and indirection table
    //     do not overlap ...

    NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const hashSecretKey = {
        parameters.HashSecretKeySize,
        reinterpret_cast<unsigned char const *>(hashSecretKeyBuffer)
    };

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_adapterDispatch->ReceiveScalingSetHashSecretKey(m_adapter, &hashSecretKey));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        ReceiveScalingEvaluateIndirectionEntries(
            reinterpret_cast<PROCESSOR_NUMBER *>(indirectionEntriesBuffer),
            parameters.IndirectionTableSize / sizeof(PROCESSOR_NUMBER)));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        ReceiveScalingEvaluateEnable(parameters));

    return STATUS_SUCCESS;
}

static
NTSTATUS
InsertIndirectionEntry(
    Rtl::KArray<ReceiveScaleMapping> & Mappings,
    UINT16 Index,
    PROCESSOR_NUMBER Number
    )
{
    for (auto & mapping : Mappings)
    {
        if (Number.Group == mapping.Number.Group && Number.Number == mapping.Number.Number)
        {
            CX_RETURN_NTSTATUS_IF(
                STATUS_INSUFFICIENT_RESOURCES,
                ! mapping.HashIndexes.append(Index));

            return STATUS_SUCCESS;
        }
    }

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! Mappings.resize(Mappings.count() + 1));

    auto & mapping = Mappings[Mappings.count() - 1];
    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! mapping.HashIndexes.append(Index));

    mapping.Number = Number;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReceiveScalingEvaluateIndirectionEntries(
    PROCESSOR_NUMBER const * ProcessorNumbers,
    size_t Count
    )
{
    Rtl::KArray<ReceiveScaleMapping> mappings;
    for (UINT16 i = 0; i < Count; i++)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            InsertIndirectionEntry(mappings, i, ProcessorNumbers[i]));
    }

    // no rx queues, datapath has not been created
    // cache mappings for when the datapath is created
    if (m_rxQueues.count() == 0)
    {
        m_receiveScaleMappings = wistd::move(mappings);

        return STATUS_SUCCESS;
    }

    // dynamic update after datapath created
    return STATUS_NOT_IMPLEMENTED;
}

_Use_decl_annotations_
NTSTATUS
NxTranslationApp::ReceiveScalingIndirectionTableEntriesSet(
    NDIS_OID_REQUEST & Request
    )
{
    auto const & data = Request.DATA.SET_INFORMATION;
    auto const & parameters = *static_cast<NDIS_RSS_SET_INDIRECTION_ENTRIES *>(data.InformationBuffer);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        data.InformationBufferLength < sizeof(parameters));

    size_t tableSize;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        RtlSizeTMult(
            parameters.RssEntrySize,
            parameters.NumberOfRssEntries,
            &tableSize));

    size_t size;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        RtlSizeTAdd(
            sizeof(parameters),
            tableSize,
            &size));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        data.InformationBufferLength < size);

    ULONG_PTR indirectionEntriesBuffer;
    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        RtlULongPtrAdd(
            (ULONG_PTR)data.InformationBuffer,
            (ULONG_PTR)parameters.RssEntryTableOffset,
            &indirectionEntriesBuffer));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxTranslationApp::DestroyDatapath(void)
{
    m_txQueue = nullptr;

    // Close a hard gate that prevent new NBLs from being indicated to NDIS, then wait
    // for all NBLs to be returned.
    m_NblDispatcher->SetRxHandler(nullptr);
    m_rxQueues.clear();
}

