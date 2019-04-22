// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include <ndisoidtypes.h>

#include "NxReceiveScaling.tmh"
#include "NxReceiveScaling.hpp"

#include "NxTranslationApp.hpp"

#define SET_INDIRECTION_ENTRIES_RETRY 3

#ifdef _KERNEL_MODE
#include <ntddndis.h>
#include <affinity.h> // for MAXIMUM_GROUPS

_IRQL_requires_max_(HIGH_LEVEL)
static
NTSTATUS
NdisConvertNdisStatusToNtStatus(NDIS_STATUS NdisStatus)
{
    if (NT_SUCCESS(NdisStatus) &&
        NdisStatus != NDIS_STATUS_SUCCESS &&
        NdisStatus != NDIS_STATUS_PENDING &&
        NdisStatus != NDIS_STATUS_INDICATION_REQUIRED) {

        // Case where an NDIS error is incorrectly mapped as a success by NT_SUCCESS macro
        return STATUS_UNSUCCESSFUL;
    } else {
        switch (NdisStatus)
        {
        case NDIS_STATUS_BUFFER_TOO_SHORT:
            return STATUS_BUFFER_TOO_SMALL;
            break;
        default:
            return (NTSTATUS) NdisStatus;
            break;
        }
    }
}

#endif // _KERNEL_MODE

_Use_decl_annotations_
NxReceiveScaling::NxReceiveScaling(
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
NxReceiveScaling::GetNumberOfQueues(
    void
) const
{
    return m_numberOfQueues;
}

_Use_decl_annotations_
NTSTATUS
NxReceiveScaling::Initialize(
)
{
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

    auto const defaultQueues = static_cast<ULONG>(capabilities.NumberOfIndirectionQueues);
    ULONG numberOfQueues;
    NDIS_STRING NumRssQueues = NDIS_STRING_CONST("*NumRSSQueues");
    CX_RETURN_IF_NOT_NT_SUCCESS(
        readParameter(NumRssQueues, numberOfQueues, defaultQueues, defaultQueues));
    m_numberOfQueues = numberOfQueues;
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

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxReceiveScaling::EvaluateEnable(
    NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
)
{
    NT_FRE_ASSERT(WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS));

    //
    // if we are already enabled we do not enable receive scaling again.
    //
    // instead we check to see if we receive parameters that may only be
    // changed when receive scaling is first enabled.
    //
    // XXX it is unknown if there is hardware that can actually change
    //     HashInformation while receive scaling is enabled.
    //
    if (m_enabled)
    {
        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_HASH_INFO_CHANGED),
            "NDIS_RECEIVE_SCALE_PARAM_HASH_INFO_CHANGED when RSS already enabled.");

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            Parameters.HashInformation != 0,
            "HashInformation when RSS already enabled.");

        return STATUS_SUCCESS;
    }

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        ! WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_HASH_INFO_CHANGED),
        "NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS without NDIS_RECEIVE_SCALE_PARAM_HASH_INFO_CHANGED.");

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        Parameters.HashInformation == 0,
        "NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS without HashInformation.");

    m_hashFunction = NDIS_RSS_HASH_FUNC_FROM_HASH_INFO(Parameters.HashInformation);
    m_hashType = NDIS_RSS_HASH_TYPE_FROM_HASH_INFO(Parameters.HashInformation);

    return SetEnabled();
}

_Use_decl_annotations_
bool
NxReceiveScaling::EvaluateDisable(
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
NxReceiveScaling::EvaluateHashSecretKey(
    NDIS_RECEIVE_SCALE_PARAMETERS_V2 const & Parameters
)
{
    NT_FRE_ASSERT(WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_ENABLE_RSS));

    if (WI_IsFlagSet(Parameters.Flags, NDIS_RECEIVE_SCALE_PARAM_HASH_KEY_CHANGED))
    {
        auto const key = reinterpret_cast<UINT8 const *>(&Parameters) + Parameters.HashSecretKeyOffset;
        auto const size = Parameters.HashSecretKeySize;

        CX_RETURN_NTSTATUS_IF_MSG(
            STATUS_INVALID_PARAMETER,
            size != sizeof(m_hashSecretKey),
            "HashSecretKeySize != %ul", sizeof(m_hashSecretKey));

        RtlCopyMemory(m_hashSecretKey, key, size);

        return SetHashSecretKey();
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxReceiveScaling::SetParameters(
    NDIS_OID_REQUEST const & Request
)
{
    auto const buffer = static_cast<unsigned char const *>(Request.DATA.SET_INFORMATION.InformationBuffer);
    auto const length = Request.DATA.SET_INFORMATION.InformationBufferLength;
    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        length < NDIS_SIZEOF_RECEIVE_SCALE_PARAMETERS_V2_REVISION_1,
        "InformationBufferLength too small.");

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        ! (buffer + length > buffer),
        "InformationBuffer + InformationBufferLength results in integer overflow.");

    auto const parameters = reinterpret_cast<NDIS_RECEIVE_SCALE_PARAMETERS_V2 const *>(buffer);

    // HashSecretKeyOffset overflows the InformationBuffer
    auto const hashSecretKey = buffer + parameters->HashSecretKeyOffset;
    auto const hashSecretKeyEnd = hashSecretKey + parameters->HashSecretKeySize;
    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        ! (hashSecretKey > buffer),
        "HashSecretKeyOffset + HashSecretKeySize results in integer overflow.");

    // hashSecretKey starts and finishes within the InformationBuffer
    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        (! (hashSecretKeyEnd > hashSecretKey)) || hashSecretKeyEnd > buffer + length,
        "HashSecretKey does not start and finish within the InformationBuffer.");

    if (EvaluateDisable(*parameters))
    {
        return STATUS_SUCCESS;
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        EvaluateHashSecretKey(*parameters));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        EvaluateEnable(*parameters));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
size_t
NxReceiveScaling::EnumerateProcessor(
    UINT16 Group,
    UINT8 Number
) const
{
    return Group * m_maxGroupProcessorCount + Number;
}

_Use_decl_annotations_
size_t
NxReceiveScaling::EnumerateProcessor(
    PROCESSOR_NUMBER const & Processor
) const
{
    return EnumerateProcessor(Processor.Group, Processor.Number) - m_minProcessorIndex;
}

_Use_decl_annotations_
NxRxXlat *
NxReceiveScaling::GetAffinitizedQueue(
    size_t Index
) const
{
    return m_affinitizedQueues[Index].Queue;
}

_Use_decl_annotations_
void
NxReceiveScaling::SetAffinitizedQueue(
    size_t Index,
    NxRxXlat * Queue,
    GROUP_AFFINITY const & Affinity
)
{
    Queue->SetGroupAffinity(Affinity);
    m_affinitizedQueues[Index] = { Queue, Queue->GetQueueId(), Affinity };
}

_Use_decl_annotations_
NxRxXlat *
NxReceiveScaling::MapAffinitizedQueue(
    size_t Index,
    GROUP_AFFINITY const & Affinity
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
        if (! searchQueue->IsGroupAffinitized())
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
NxReceiveScaling::SetEnabled(
    void
)
{
    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_dispatch.Enable(m_app.GetAdapter(), m_hashFunction, m_hashType));

    m_enabled = true;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
NxReceiveScaling::SetDisabled(
    void
)
{
    m_dispatch.Disable(m_app.GetAdapter());

    m_enabled = false;
}

_Use_decl_annotations_
NTSTATUS
NxReceiveScaling::SetHashSecretKey(
    void
)
{
    NET_CLIENT_RECEIVE_SCALING_HASH_SECRET_KEY const hashSecretKey = {
        &m_hashSecretKey[0],
        sizeof(m_hashSecretKey),
    };

    return m_dispatch.SetHashSecretKey(m_app.GetAdapter(), &hashSecretKey);
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
NxReceiveScaling::SetIndirectionEntries(
    size_t NumberOfEntries,
    size_t NumberOfRetries,
    TranslatedIndirectionEntries & TranslatedEntries
)
{
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
NxReceiveScaling::SetIndirectionEntries(
    NDIS_OID_REQUEST const & Request
)
{
    auto const buffer = static_cast<unsigned char const *>(Request.DATA.SET_INFORMATION.InformationBuffer);
    auto const length = Request.DATA.SET_INFORMATION.InformationBufferLength;

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        length < NDIS_SIZEOF_RSS_SET_INDIRECTION_ENTRIES_REVISION_1,
        "InformationBufferLength too small.");

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        ! (buffer + length > buffer),
        "InformationBuffer + InformationBufferLength results in integer overflow.");

    auto const parameters = reinterpret_cast<NDIS_RSS_SET_INDIRECTION_ENTRIES const *>(buffer);
    auto const table = buffer + parameters->RssEntryTableOffset;
    auto const tableEnd = table + parameters->NumberOfRssEntries * parameters->RssEntrySize;

    CX_RETURN_NTSTATUS_IF_MSG(
        STATUS_INVALID_PARAMETER,
        (! (tableEnd > table)) || tableEnd > buffer + length,
        "RssEntryTable does not start and finish within the InformationBuffer.");

    TranslatedIndirectionEntries translatedEntries = {};
    auto const entries = reinterpret_cast<NDIS_RSS_SET_INDIRECTION_ENTRY const *>(table);
    for (size_t i = 0; i < parameters->NumberOfRssEntries; i++)
    {
        auto const processorNumber = entries[i].TargetProcessorNumber;
        auto const processorIndex = EnumerateProcessor(processorNumber);
        auto const indirectionTableIndex = entries[i].IndirectionTableIndex;
        GROUP_AFFINITY const groupAffinity = {
            1ULL << processorNumber.Number,
            processorNumber.Group
        };

        //
        // get the queue mapped to the target processor. if no queue is mapped
        // then find a new queue to map to the target or remap the queue used
        // by the source processor to the target processor.
        //
        auto queue = GetAffinitizedQueue(processorIndex);
        if (! queue)
        {
            queue = MapAffinitizedQueue(processorIndex, groupAffinity);
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

    return SetIndirectionEntries(
        parameters->NumberOfRssEntries,
        0,
        translatedEntries);
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
NxReceiveScaling::Configure(
    void
)
{
    //
    // Affinitize all queues to the default group affinity if they aren't
    // already affinitized.
    //
    for (auto & queue : m_queues)
    {
        if (! queue->IsGroupAffinitized())
        {
            GROUP_AFFINITY const groupAffinity = {
                1ULL << m_defaultProcessor.Number,
                m_defaultProcessor.Group
                };

            (void)MapAffinitizedQueue(m_minProcessorIndex, groupAffinity);
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
            affinitizedQueue.Queue->SetGroupAffinity(affinitizedQueue.Affinity);
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

    return STATUS_SUCCESS;
}

