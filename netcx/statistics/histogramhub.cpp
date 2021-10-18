// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include "HistogramHub.h"
#include <NdisStatisticalIoctls.h>
#include <KLockHolder.h>

namespace Statistics
{
    PAGEDX
    _Use_decl_annotations_
    bool
    Hub::AddHistogramReference(
        Histogram& histogram
    )
    {
        KLockThisExclusive lock { m_histogramsListLock };
        return m_histogramsArray.append(&histogram);
    }

    PAGEDX
    _Use_decl_annotations_
    void
    Hub::RemoveHistogramReference(
        Histogram& histogram
    )
    {
        KLockThisExclusive lock { m_histogramsListLock };
        bool histogramFound = false;

        size_t i = m_histogramsArray.count();
        while (i != 0)
        {
            i--;
            if (m_histogramsArray[i] == &histogram)
            {
                m_histogramsArray.eraseAt(i);
                histogramFound = true;
            }
        }

        if (!histogramFound)
        {
            __fastfail(FAST_FAIL_INVALID_ARG);
        }
    }

    PAGEDX
    _Use_decl_annotations_
    void
    Hub::ResetHistogramsValues(
        void
    )
    {
        KLockThisExclusive lock { m_histogramsListLock };
        for (auto histogram : m_histogramsArray)
        {
            histogram->ResetValues();
        }
    }

    template<>
    PAGEDX
    _Use_decl_annotations_
    ULONG
    Hub::SerializedSize<NDIS_COLLECT_HISTOGRAM_OUT>(
        void
    ) const
    {
        ULONG bytes = 0L;
        bytes += FIELD_OFFSET(NDIS_COLLECT_HISTOGRAM_OUT, Histograms);
        if (m_histogramsArray.count() != 0)
        {
            bytes += m_histogramsArray[0]->SerializedSize<NDIS_SINGLE_HISTOGRAM_ENTRY>() * static_cast<ULONG>(m_histogramsArray.count());
        }
        return bytes;
    }

    template<>
    PAGEDX
    _Use_decl_annotations_
    NTSTATUS
    Hub::Serialize<NDIS_COLLECT_HISTOGRAM_OUT>(
        NDIS_COLLECT_HISTOGRAM_OUT *out,
        ULONG cbOut,
        ULONG &bytesNeeded
    ) const
    {
        KLockThisExclusive lock { m_histogramsListLock };
        bytesNeeded = SerializedSize<NDIS_COLLECT_HISTOGRAM_OUT>();

        if (bytesNeeded > cbOut)
        {
            RtlZeroMemory(out, cbOut);
            return STATUS_BUFFER_OVERFLOW;
        }

        RtlZeroMemory(out, cbOut);

        wcscpy_s(out->Name, L"EC statistics");

        if (m_histogramsArray.count() == 0)
        {
            return STATUS_SUCCESS;
        }

        auto const firstHistogram = m_histogramsArray[0];

        out->NumHistograms = static_cast<ULONG>(m_histogramsArray.count());
        out->HistogramEntryStride = firstHistogram->SerializedSize<NDIS_SINGLE_HISTOGRAM_ENTRY>();

        out->BucketWidth = firstHistogram->m_histogram->GetBucketWidth();
        out->NumBucketsPerHistogram = firstHistogram->m_histogram->GetNumBuckets();
        out->SmallestBucketValue = firstHistogram->m_histogram->GetSmallestValue();

        UINT32 count = 0;
        for (const auto entry : m_histogramsArray)
        {
            // the limitation of NDIS_COLLECT_HISTOGRAM_OUT format is that every histogram
            // must be of the same size with the same # of buckets
            NT_FRE_ASSERT(out->BucketWidth == entry->m_histogram->GetBucketWidth());
            NT_FRE_ASSERT(out->NumBucketsPerHistogram == entry->m_histogram->GetNumBuckets());
            NT_FRE_ASSERT(out->SmallestBucketValue == entry->m_histogram->GetSmallestValue());

            auto destination =  reinterpret_cast<NDIS_SINGLE_HISTOGRAM_ENTRY*>(
                reinterpret_cast<UCHAR*>(out->Histograms) + out->HistogramEntryStride * count++
            );

            const ULONG bytesLeftInBuffer = (ULONG)(cbOut - ((UCHAR*)destination - (UCHAR*)out));
            ULONG bytesNeededForHistogram = 0;
            auto status = entry->Serialize<NDIS_SINGLE_HISTOGRAM_ENTRY>(destination, bytesLeftInBuffer, bytesNeededForHistogram);

            // this should not happen; in the beginning of the function we ensured there's enough memory for all the histograms
            NT_FRE_ASSERT(status != STATUS_BUFFER_OVERFLOW);
        }

        return STATUS_SUCCESS;
    }
}
