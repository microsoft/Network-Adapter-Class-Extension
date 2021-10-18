// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include "Histogram.h"
#include <NdisStatisticalIoctls.h>

namespace Statistics
{
    NONPAGEDX
    _Use_decl_annotations_
    Histogram::Histogram(
        ULONG64 HistogramSmallestValue,
        ULONG64 HistogramBucketWidth,
        ULONG HistogramNumBuckets
    )
    {
        m_histogram.reset(KHistogram::Create(
            HistogramSmallestValue,
            HistogramBucketWidth,
            HistogramNumBuckets
        ));
    }

    NONPAGEDX
    _Use_decl_annotations_
    void
    Histogram::AddValue(
        ULONG64 value
    )
    {
        if (m_histogram)
        {
            m_histogram->AddValue(value);
        }
    }

    PAGEDX
    _Use_decl_annotations_
    void
    Histogram::SetName(
        _In_ UNICODE_STRING const & Name
    )
    {
        auto const cbLength = min(Name.Length, sizeof(m_name) - sizeof(UNICODE_NULL));
        NT_FRE_ASSERT((cbLength % sizeof(WCHAR)) == 0);

        RtlCopyMemory(m_name, Name.Buffer, cbLength);

        auto const cchLastChar = cbLength / sizeof(WCHAR);
        m_name[cchLastChar] = UNICODE_NULL;
    }

    PAGED
    void
    Histogram::ResetValues(
        void
    )
    {
        if (m_histogram)
        {
            m_histogram->ResetValues();
        }
    }

    template<>
    PAGEDX
    _Use_decl_annotations_
    ULONG
    Histogram::SerializedSize<NDIS_SINGLE_HISTOGRAM_ENTRY>() const
    {
        ULONG numBuckets = m_histogram ? m_histogram->GetNumBuckets() : 0;
        // the size of Histogram for NDIS_SINGLE_HISTOGRAM_ENTRY form is
        // numBuckets * sizeof(bucket) + sizeof(entry)
        return numBuckets * sizeof(NDIS_SINGLE_HISTOGRAM_ENTRY::Buckets[0]) + FIELD_OFFSET(NDIS_SINGLE_HISTOGRAM_ENTRY, Buckets);
    }

    template<>
    PAGEDX
    _Use_decl_annotations_
    NTSTATUS
    Histogram::Serialize<NDIS_SINGLE_HISTOGRAM_ENTRY>(
        NDIS_SINGLE_HISTOGRAM_ENTRY *out,
        ULONG cbOut,
        ULONG &bytesNeeded
    ) const
    {
        bytesNeeded = SerializedSize<NDIS_SINGLE_HISTOGRAM_ENTRY>();

        if (bytesNeeded > cbOut)
        {
            RtlZeroMemory(out, cbOut);
            return STATUS_BUFFER_OVERFLOW;
        }

        const ULONG bytesForBuckets = m_histogram ? m_histogram->GetBucketsMemorySize() : 0;
        NT_FRE_ASSERT(FIELD_OFFSET(NDIS_SINGLE_HISTOGRAM_ENTRY, Buckets) + bytesForBuckets <= cbOut);

        // These fields are not (yet?) supported by TimeHistogram
        out->TimestampUs = 0;
        out->AuxiliaryDataType = NDIS_HISTOGRAM_AUXILIARY_DATATYPE::None;

        static_assert(sizeof(out->Name) >= sizeof(m_name));

        wcscpy_s(out->Name, m_name);

        if (m_histogram)
        {
            RtlCopyMemory(out->Buckets, const_cast<USHORT const *>(m_histogram->GetBuckets()), bytesForBuckets);
        }
        return STATUS_SUCCESS;
    }
}
