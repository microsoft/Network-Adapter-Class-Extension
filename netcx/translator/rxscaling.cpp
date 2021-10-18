// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndis/oidrequest.h>
#include <ndis/statusconvert.h>

#include "RxScaling.tmh"
#include "RxScaling.hpp"

#include "NxTranslationApp.hpp"

#define SET_INDIRECTION_ENTRIES_RETRY 3

#ifdef _KERNEL_MODE
#include <affinity.h> // for MAXIMUM_GROUPS

#endif // _KERNEL_MODE

_Use_decl_annotations_
RxScaling::RxScaling(
    NxTranslationApp & App,
    Rtl::KArray<wistd::unique_ptr<NxRxXlat>, NonPagedPoolNx> const & Queues,
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_DISPATCH const & Dispatch
) noexcept :
    m_app(App),
    m_queues(Queues),
    m_dispatch(Dispatch)
{
}

_Use_decl_annotations_
size_t
RxScaling::GetNumberOfQueues(
    void
) const
{
    return m_numberOfQueues;
}

_Use_decl_annotations_
NTSTATUS
RxScaling::Initialize(
)
{
    NT_FRE_ASSERT(! m_configured);

    auto const capabilities = m_app.GetReceiveScalingCapabilities();
#ifdef _KERNEL_MODE
    NDIS_CONFIGURATION_OBJECT configurationObject = {
        {
            NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT,
            NDIS_CONFIGURATION_OBJECT_REVISION_1,
            NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1
        },
        m_app.GetProperties().NdisAdapterHandle,
        0,
    };

    NDIS_HANDLE handle;
    NDIS_STATUS ndisStatus = NdisOpenConfigurationEx(&configurationObject, &handle);
    if (ndisStatus != NDIS_STATUS_SUCCESS)
    {
        return NdisConvertNdisStatusToNtStatus(ndisStatus);
    }

    auto configurationHandle = wil::unique_any<NDIS_HANDLE,
        decltype(&::NdisCloseConfiguration), &::NdisCloseConfiguration>(handle);
    auto const readParameter = [handle](
        NDIS_STRING & ParameterName,
        ULONG & ParameterValue,
        ULONG ParameterLimit,
        ULONG DefaultValue
    )
    {
        NDIS_STATUS status;
        NDIS_CONFIGURATION_PARAMETER * parameter;
        NdisReadConfiguration(&status, &parameter, handle, &ParameterName, NdisParameterInteger);
        switch (status)
        {
        case NDIS_STATUS_SUCCESS:
            ParameterValue = parameter->ParameterData.IntegerData;
            if (ParameterValue > ParameterLimit)
            {
                ParameterValue = ParameterLimit;
            }
            break;

        case NDIS_STATUS_FAILURE:
            ParameterValue = DefaultValue;
            return STATUS_SUCCESS;

        }
        return NdisConvertNdisStatusToNtStatus(status);
    };

    ULONG minGroup;
    NDIS_STRING RssBaseProcGroupStr = NDIS_STRING_CONST("*RssBaseProcGroup");
    CX_RETURN_IF_NOT_NT_SUCCESS(
        readParameter(RssBaseProcGroupStr, minGroup, MAXIMUM_GROUPS - 1, 0));

    ULONG maxGroup;
    NDIS_STRING RssMaxProcGroupStr = NDIS_STRING_CONST("*RssMaxProcGroup");
    CX_RETURN_IF_NOT_NT_SUCCESS(
        readParameter(RssMaxProcGroupStr, maxGroup, MAXIMUM_GROUPS - 1, 0));

    for (auto groupIndex = static_cast<UINT16>(minGroup); groupIndex <= maxGroup; groupIndex++)
    {
        auto count = KeQueryMaximumProcessorCountEx(groupIndex);
        if (count > m_maxGroupProcessorCount)
        {
            m_maxGroupProcessorCount = count;
        }
    }

    NT_FRE_ASSERT(0 != m_maxGroupProcessorCount);

    ULONG minNumber;
    NDIS_STRING RssBaseProcNumberStr = NDIS_STRING_CONST("*RssBaseProcNumber");
    CX_RETURN_IF_NOT_NT_SUCCESS(
        readParameter(RssBaseProcNumberStr, minNumber, MAXIMUM_PROC_PER_GROUP - 1, 0));

    ULONG maxNumber;
    NDIS_STRING RssMaxProcNumberStr = NDIS_STRING_CONST("*RssMaxProcNumber");
    CX_RETURN_IF_NOT_NT_SUCCESS(
        readParameter(RssMaxProcNumberStr, maxNumber, MAXIMUM_PROC_PER_GROUP - 1, MAXIMUM_PROC_PER_GROUP - 1));

    m_defaultProcessor = { static_cast<UINT16>(minGroup), static_cast<UINT8>(minNumber) };

    m_minProcessorIndex = EnumerateProcessor(
        static_cast<UINT16>(minGroup),
        static_cast<UINT8>(minNumber));

    m_maxProcessorIndex = EnumerateProcessor(
        static_cast<UINT16>(maxGroup),
        static_cast<UINT8>(maxNumber));

    m_numberOfQueues = capabilities.NumberOfIndirectionQueues;
#endif // _KERNEL_MODE

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_affinitizedQueues.resize(m_maxProcessorIndex - m_minProcessorIndex));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_indirectionTable.resize(capabilities.NumberOfIndirectionTableEntries));
    for (auto & entry : m_indirectionTable)
    {
        entry = 0U;
    }

    m_numberIndirectionTableEntries = capabilities.NumberOfIndirectionTableEntries;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
RxScaling::EvaluateEnable(
    NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
)
{
    if (WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS))
    {
        return SetEnabled();
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
RxScaling::EvaluateHashInfo(
    NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
)
{
    NT_FRE_ASSERT(WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS));

    //
    // Hash information can be changed at the time RSS is enabled, or afterward
    //
    // XXX it is unknown if there is hardware that can actually change
    //     HashInformation while receive scaling is enabled.
    //
    if (WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_HASH_INFO_CHANGED))
    {
        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            Parameters.HashInformation == 0,
            "NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS without HashInformation.");

        auto const hashTypeMask = NDIS_HASH_IPV4 |
            NDIS_HASH_TCP_IPV4 |
            NDIS_HASH_UDP_IPV4 |
            NDIS_HASH_IPV6 |
            NDIS_HASH_TCP_IPV6 |
            NDIS_HASH_UDP_IPV6 |
            NDIS_HASH_IPV6_EX |
            NDIS_HASH_TCP_IPV6_EX |
            NDIS_HASH_UDP_IPV6_EX;

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            WI_AreAllFlagsClear(Parameters.HashInformation, hashTypeMask),
            "NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS with invalid HashType.");

        auto const reservedHashFunction = NdisHashFunctionReserved1 |
            NdisHashFunctionReserved2 |
            NdisHashFunctionReserved3;

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_NOT_SUPPORTED,
            WI_IsAnyFlagSet(Parameters.HashInformation, reservedHashFunction),
            "NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS with reserved HashFunction.");

        auto const hashFunctionMask = NdisHashFunctionToeplitz;

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            WI_IsFlagClear(Parameters.HashInformation, hashFunctionMask),
            "NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS with invalid HashFunction.");

        auto const hashFunction = NDIS_RSS_HASH_FUNC_FROM_HASH_INFO(Parameters.HashInformation);
        auto const hashType = NDIS_RSS_HASH_TYPE_FROM_HASH_INFO(Parameters.HashInformation);

        CX_RETURN_IF_NOT_NT_SUCCESS(
            SetHashInfo(hashFunction, hashType));

        m_hashFunction = hashFunction;
        m_hashType = hashType;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
bool
RxScaling::EvaluateDisable(
    NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
)
{
    if (! WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS))
    {
        SetDisabled();

        return true;
    }

    return false;
}

_Use_decl_annotations_
NTSTATUS
RxScaling::EvaluateHashSecretKey(
    NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters,
    const size_t BufferSize
)
{
    NT_FRE_ASSERT(WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS));

    if (WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_HASH_KEY_CHANGED))
    {
        auto const size = Parameters.HashSecretKeySize;

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            size != sizeof(m_hashSecretKey),
            "HashSecretKeySize != %ul", sizeof(m_hashSecretKey));

        auto const key = TryGetArray<UINT8>(reinterpret_cast<void const *>(&Parameters),
            BufferSize,
            Parameters.HashSecretKeyOffset,
            Parameters.HashSecretKeySize);

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            key == nullptr,
            "HashSecretKey does not start and finish within the InformationBuffer.");

        RtlCopyMemory(m_hashSecretKey, key, size);

        return SetHashSecretKey();
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
RxScaling::EvaluateIndirectionTableEntries(
    NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
)
{
    if (WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_NUMBER_OF_ENTRIES_CHANGED))
    {
        // If the number of entries increased, duplicate the current indirection table entry
        // upto the new number of entries.
        // If the entries decreased, upper layer ensures that the portion of the indirection table
        // which is being removed contains exact replicas of the remaining portion. Netcx has a fixed
        // size indirection table (= the max entries advertised by the client driver) so we do not
        // remove entries. We simply indicate the new number of entries to the client driver.
        if (m_numberIndirectionTableEntries < Parameters.NumberOfIndirectionTableEntries)
        {
            // Copy the indirection table entries until it is filled
            for (size_t i = 1; i < m_indirectionTable.count()/m_numberIndirectionTableEntries; i++)
            {
                memcpy(&m_indirectionTable[i * m_numberIndirectionTableEntries],
                    &m_indirectionTable[0],
                    m_numberIndirectionTableEntries * sizeof(m_indirectionTable[0]));
            }
        }

        TranslatedIndirectionEntries translatedEntries = {};
        for (size_t i = 0; i < Parameters.NumberOfIndirectionTableEntries; i++)
        {
            auto const queueIndex = m_indirectionTable[i];
            auto & queue = m_queues[queueIndex];

            translatedEntries.Restore[i] = static_cast<UINT32>(queueIndex);
            translatedEntries.Entries[i] = { queue->GetQueue(), STATUS_SUCCESS, static_cast<UINT32>(i) };
        }

        CX_RETURN_IF_NOT_NT_SUCCESS(
            SetIndirectionEntries(
                Parameters.NumberOfIndirectionTableEntries,
                0,
                translatedEntries));

        m_numberIndirectionTableEntries = Parameters.NumberOfIndirectionTableEntries;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
RxScaling::SetParameters(
    NDIS_OID_REQUEST const & Request
)
{
    if (! m_configured)
    {
        return STATUS_SUCCESS;
    }

    auto const buffer = static_cast<unsigned char const *>(Request.DATA.SET_INFORMATION.InformationBuffer);
    auto const length = Request.DATA.SET_INFORMATION.InformationBufferLength;
    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        length < NDIS_SIZEOF_RECEIVE_SCALE_PARAMETERS_V2_REVISION_1,
        "InformationBufferLength too small.");

    auto const parameters = reinterpret_cast<NDIS_RECEIVE_SCALE_PARAMETERS_V2 const *>(buffer);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        EvaluateIndirectionTableEntries(*parameters));

    if (EvaluateDisable(*parameters))
    {
        return STATUS_SUCCESS;
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        EvaluateHashSecretKey(*parameters, length));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        EvaluateHashInfo(*parameters));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        EvaluateEnable(*parameters));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
size_t
RxScaling::EnumerateProcessor(
    UINT16 Group,
    UINT8 Number
) const
{
    return Group * m_maxGroupProcessorCount + Number;
}

_Use_decl_annotations_
size_t
RxScaling::EnumerateProcessor(
    PROCESSOR_NUMBER const & Processor
) const
{
    return EnumerateProcessor(Processor.Group, Processor.Number) - m_minProcessorIndex;
}

_Use_decl_annotations_
NxRxXlat *
RxScaling::GetAffinitizedQueue(
    size_t Index
) const
{
    return m_affinitizedQueues[Index].Queue;
}

_Use_decl_annotations_
void
RxScaling::SetAffinitizedQueue(
    size_t Index,
    NxRxXlat * Queue,
    PROCESSOR_NUMBER const & Affinity
)
{
    Queue->SetAffinity(Affinity);
    m_affinitizedQueues[Index] = { Queue, Queue->GetQueueId(), Affinity };
}

_Use_decl_annotations_
NxRxXlat *
RxScaling::MapAffinitizedQueue(
    size_t Index,
    PROCESSOR_NUMBER const & Affinity
)
{
    KAcquireSpinLock lock(m_receiveScalingLock);

    //
    // repeat search, MapAffinitizedQueue may have already completed
    // on another processor.
    //
    auto queue = GetAffinitizedQueue(Index);
    if (queue)
    {
        return queue;
    }

    //
    // find an unmapped queue and map it to the target processor
    //
    for (size_t i = 0; i < m_queues.count(); i++)
    {
        auto & searchQueue = m_queues[i];
        if (! searchQueue->IsAffinitized())
        {
            SetAffinitizedQueue(Index, searchQueue.get(), Affinity);

            return searchQueue.get();
        }
    }

#ifdef _KERNEL_MODE
    //
    // if we reach here there are no unmapped queues. remap the
    // queue mapped to the source processor to the target processor.
    //
    PROCESSOR_NUMBER processorNumber;
    (void)KeGetCurrentProcessorNumberEx(&processorNumber);
    auto const processorIndex = EnumerateProcessor(processorNumber);
    auto sourceQueue = GetAffinitizedQueue(processorIndex);

    m_affinitizedQueues[processorIndex] = {};
    SetAffinitizedQueue(Index, sourceQueue, Affinity);

    return sourceQueue;
#else
    return nullptr;
#endif // _KERNEL_MODE
}

_Use_decl_annotations_
NTSTATUS
RxScaling::SetEnabled(
    void
)
{
    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_dispatch.Enable(
            m_app.GetAdapter()));

    m_enabled = true;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
RxScaling::SetDisabled(
    void
)
{
    m_dispatch.Disable(m_app.GetAdapter());

    m_enabled = false;
}

_Use_decl_annotations_
NTSTATUS
RxScaling::SetHashSecretKey(
    void
)
{
    NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const hashSecretKey = {
        &m_hashSecretKey[0],
        sizeof(m_hashSecretKey),
    };

    return m_dispatch.SetHashSecretKey(m_app.GetAdapter(), &hashSecretKey);
}

_Use_decl_annotations_
NTSTATUS
RxScaling::SetHashInfo(
    ULONG HashFunction,
    ULONG HashType
)
{
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_HASH_TYPE hashType = NetClientAdapterReceiveScalingHashTypeNone;
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE protocolType = NetClientAdapterReceiveScalingProtocolTypeNone;

    TranslateHashInfo(
        HashFunction,
        HashType,
        &hashType,
        &protocolType);

    NET_CLIENT_RECEIVE_SCALING_HASH_INFO const hashInfo = {
        sizeof(NET_CLIENT_RECEIVE_SCALING_HASH_INFO),
        hashType,
        protocolType};

    return m_dispatch.SetHashInfo(m_app.GetAdapter(),
        &hashInfo);
}

_Use_decl_annotations_
void
RxScaling::TranslateHashInfo(
    ULONG HashFunction,
    ULONG HashType,
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_HASH_TYPE * const NetClientHashType,
    NET_CLIENT_ADAPTER_RECEIVE_SCALING_PROTOCOL_TYPE * const NetClientProtocolType
)
{
    // translate ndis -> net adapter
    WI_SetFlagIf(*NetClientHashType,
        NetClientAdapterReceiveScalingHashTypeToeplitz,
        WI_IsFlagSet(HashFunction, NdisHashFunctionToeplitz));

    auto const protocolTypeIpv4 = NDIS_HASH_IPV4 | NDIS_HASH_TCP_IPV4 | NDIS_HASH_UDP_IPV4;
    WI_SetFlagIf(*NetClientProtocolType,
        NetClientAdapterReceiveScalingProtocolTypeIPv4,
        WI_IsAnyFlagSet(HashType, protocolTypeIpv4));

    auto const protocolTypeIpv6 = NDIS_HASH_IPV6 | NDIS_HASH_TCP_IPV6 | NDIS_HASH_UDP_IPV6;
    WI_SetFlagIf(*NetClientProtocolType,
        NetClientAdapterReceiveScalingProtocolTypeIPv6,
        WI_IsAnyFlagSet(HashType, protocolTypeIpv6));

    auto const protocolTypeIpv6Ext = NDIS_HASH_IPV6_EX | NDIS_HASH_TCP_IPV6_EX | NDIS_HASH_UDP_IPV6_EX;
    WI_SetFlagIf(*NetClientProtocolType,
        NetClientAdapterReceiveScalingProtocolTypeIPv6Extensions,
        WI_IsAnyFlagSet(HashType, protocolTypeIpv6Ext));

    auto const protocolTypeTcp = NDIS_HASH_TCP_IPV4 | NDIS_HASH_TCP_IPV6 | NDIS_HASH_TCP_IPV6_EX;
    WI_SetFlagIf(*NetClientProtocolType,
        NetClientAdapterReceiveScalingProtocolTypeTcp,
        WI_IsAnyFlagSet(HashType, protocolTypeTcp));

    auto const protocolTypeUdp = NDIS_HASH_UDP_IPV4 | NDIS_HASH_UDP_IPV6 | NDIS_HASH_UDP_IPV6_EX;
    WI_SetFlagIf(*NetClientProtocolType,
        NetClientAdapterReceiveScalingProtocolTypeUdp,
        WI_IsAnyFlagSet(HashType, protocolTypeUdp));
}

//
// Update the adapter indirection table and make our cache coherent if
// any failures occur.
//
// When we are called within the context of the OID we do not retry since
// the ULP has retry logic.
//
// When we are called within the context of Configure we retry
// NumberOfRetries times.
//
_Use_decl_annotations_
NTSTATUS
RxScaling::SetIndirectionEntries(
    size_t NumberOfEntries,
    size_t NumberOfRetries,
    TranslatedIndirectionEntries & TranslatedEntries
)
{
    if (! m_configured)
    {
        return STATUS_SUCCESS;
    }

    NTSTATUS status = STATUS_SUCCESS;
    do {
        NET_CLIENT_RECEIVE_SCALING_INDIRECTION_ENTRIES entries = {
            TranslatedEntries.Entries,
            NumberOfEntries,
        };

        status = m_dispatch.SetIndirectionEntries(m_app.GetAdapter(), &entries);
        if (NT_SUCCESS(status))
        {
            return status;
        }

        //
        // some or all entries failed to update, prepare to retry or restore
        // cached indirection table state to match adapter.
        //

        size_t j = 0;
        for (size_t i = 0; i < NumberOfEntries; i++)
        {
            auto const & entry = TranslatedEntries.Entries[i];
            if (! NT_SUCCESS(entry.Status))
            {
                if (NumberOfRetries == 0)
                {
                    //
                    // restore state of indirection table
                    //
                    m_indirectionTable[entry.Index] = TranslatedEntries.Restore[i];
                }
                else
                {
                    //
                    // prepare failed entries for retry
                    //
                    TranslatedEntries.Restore[j] = TranslatedEntries.Restore[i];
                    TranslatedEntries.Entries[j] = TranslatedEntries.Entries[i];
                    TranslatedEntries.Entries[j].Status = STATUS_SUCCESS;
                    j++;
                }
            }
        }
        NumberOfEntries = j;

    } while (NumberOfRetries--);

    return status;
}

bool
RxScaling::UpdateIndirectionTableEntryStatus(
    size_t const NumberOfEntries,
    TranslatedIndirectionEntries const & TranslatedEntries,
    NDIS_RSS_SET_INDIRECTION_ENTRY * Entries
)
{
    for (size_t i = 0; i < NumberOfEntries; i++)
    {
        Entries[i].EntryStatus = TranslatedEntries.Entries[i].Status;
    }

    return true;
}

//
// SetIndirectionEntries translates NDIS_RSS_SET_INDIRECTION_ENTRY
// structures into NET_CLIENT_ADAPTER_RECEIVE_SCALE_ENTRY structures.
//
// NDIS RSSv2 describes mappings of indirection table indexes that map
// to PROCESSOR_NUMBER while NetAdapterCx describes mappings of indirection
// table indexes to queues. In order to satisfy the NDIS RSSv2 interface
// the translator performs mapping between queues and processors.
//
// To map an indirection table index to a processor number for NDIS RSSv2
// we must first establish a mapping from a queue to a processor. The mapping
// is achieved by affinitizing a queue (in translator) to a specific processor.
//
_Use_decl_annotations_
NTSTATUS
RxScaling::SetIndirectionEntries(
    NDIS_OID_REQUEST & Request
)
{
    auto const buffer = static_cast<unsigned char *>(Request.DATA.SET_INFORMATION.InformationBuffer);
    auto const length = Request.DATA.SET_INFORMATION.InformationBufferLength;

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        length < NDIS_SIZEOF_RSS_SET_INDIRECTION_ENTRIES_REVISION_1,
        "InformationBufferLength too small.");

    auto const parameters = reinterpret_cast<NDIS_RSS_SET_INDIRECTION_ENTRIES const *>(buffer);

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        parameters->RssEntrySize != sizeof(NDIS_RSS_SET_INDIRECTION_ENTRY),
        "RssEntrySize != %ul", sizeof(NDIS_RSS_SET_INDIRECTION_ENTRY));

    auto entries = const_cast<NDIS_RSS_SET_INDIRECTION_ENTRY *>(TryGetArray<NDIS_RSS_SET_INDIRECTION_ENTRY>(parameters,
        length,
        parameters->RssEntryTableOffset,
        parameters->NumberOfRssEntries));

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        entries == nullptr,
        "RssEntryTable does not start and finish within the InformationBuffer");

    TranslatedIndirectionEntries translatedEntries = {};
    for (size_t i = 0; i < parameters->NumberOfRssEntries; i++)
    {
        auto const processorNumber = entries[i].TargetProcessorNumber;
        auto const processorIndex = EnumerateProcessor(processorNumber);
        auto const indirectionTableIndex = entries[i].IndirectionTableIndex;

        entries[i].EntryStatus = STATUS_SUCCESS;

        //
        // get the queue mapped to the target processor. if no queue is mapped
        // then find a new queue to map to the target or remap the queue used
        // by the source processor to the target processor.
        //
        auto queue = GetAffinitizedQueue(processorIndex);
        if (! queue)
        {
            queue = MapAffinitizedQueue(processorIndex, processorNumber);
        }

        NT_FRE_ASSERT(queue);

        //
        // build an indirection entry for the Cx, keep the current queue
        // at the cached indirection table index in case we need to restore it.
        //
        translatedEntries.Restore[i] = static_cast<UINT32>(m_indirectionTable[indirectionTableIndex]);
        translatedEntries.Entries[i] = { queue->GetQueue(), STATUS_SUCCESS, indirectionTableIndex };

        m_indirectionTable[indirectionTableIndex] = queue->GetQueueId();
    }

    auto status = SetIndirectionEntries(
        parameters->NumberOfRssEntries,
        0,
        translatedEntries);

    NT_FRE_ASSERT(status != STATUS_PENDING);

    if (status != STATUS_SUCCESS)
    {
        CX_RETURN_NTSTATUS_IF_MSG(
            status,
            UpdateIndirectionTableEntryStatus(
                parameters->NumberOfRssEntries,
                translatedEntries,
                entries),
            "Unable to set indirection table entries");
    }

    m_numberIndirectionTableEntries = parameters->NumberOfRssEntries;


    return status;
}

//
// Configure the adapter to match cached configuration.
//
// Configure is only called while the data path is available. If the adapter
// is unable to accept configuration receive scaling shall be revoked by
// the caller.
//
_Use_decl_annotations_
NTSTATUS
RxScaling::Configure(
    void
)
{
    //
    // Affinitize all queues to the default group affinity if they aren't
    // already affinitized.
    //
    for (auto & queue : m_queues)
    {
        if (! queue->IsAffinitized())
        {
            (void)MapAffinitizedQueue(m_minProcessorIndex, m_defaultProcessor);
        }
    }

    //
    // Restore the pointers to translator queues.
    //
    // This re-establishes the association between the translator queues
    // and any processors the queues had been affinitized to.
    //
    for (size_t i = 0; i < m_affinitizedQueues.count(); i++)
    {
        auto & affinitizedQueue = m_affinitizedQueues[i];
        if (affinitizedQueue.Queue)
        {
            affinitizedQueue.Queue = m_queues[affinitizedQueue.QueueId].get();
            affinitizedQueue.Queue->SetAffinity(affinitizedQueue.Affinity);
        }
    }

    //
    // Restore indirection table entries.
    //
    // This pushes the cached indirection table back down to the adapter. This
    // is done even if receive scaling is disabled.
    //
    TranslatedIndirectionEntries translatedEntries = {};
    for (size_t i = 0; i < m_indirectionTable.count(); i++)
    {
        auto const queueIndex = m_indirectionTable[i];
        auto & queue = m_queues[queueIndex];

        translatedEntries.Restore[i] = static_cast<UINT32>(queueIndex);
        translatedEntries.Entries[i] = { queue->GetQueue(), STATUS_SUCCESS, static_cast<UINT32>(i) };
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        SetIndirectionEntries(
            m_indirectionTable.count(),
            SET_INDIRECTION_ENTRIES_RETRY,
            translatedEntries));

    //
    // Restore receive scaling on the adapter, if enabled.
    //
    if (m_enabled)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            SetHashSecretKey());

        CX_RETURN_IF_NOT_NT_SUCCESS(
            SetEnabled());
    }

    m_configured = true;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
bool
RxScaling::IsContained(
    size_t const BufferSize,
    size_t const ElementOffset,
    size_t const ElementSize
)
{
    const size_t end = ElementOffset + ElementSize;

    if (end < ElementOffset)
    {
        return false;
    }

    if (end > BufferSize)
    {
        return false;
    }

    return true;
}

_Use_decl_annotations_
bool
RxScaling::IsContained(
    size_t const BufferSize,
    size_t const ElementOffset,
    size_t const ElementSize,
    size_t const ElementCount
)
{
   size_t end;

    if (STATUS_SUCCESS != RtlSizeTMult(ElementSize, ElementCount, &end))
    {
        return false;
    }

   return IsContained(BufferSize, ElementOffset, end);
}

_Use_decl_annotations_
template<typename T>
T const *
RxScaling::TryGetArray(
    void const * Buffer,
    size_t const BufferSize,
    size_t const ElementOffset,
    size_t const ElementCount
)
{
    if (!IsContained(BufferSize, ElementOffset, sizeof(T), ElementCount))
    {
        return nullptr;
    }

    return reinterpret_cast<T const *>(reinterpret_cast<unsigned char const *>(Buffer) + ElementOffset);
}
