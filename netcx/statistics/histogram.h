// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>
#include <KPtr.h>
#include <KHistogram.h>

namespace Statistics
{
    const UINT32 HIST_NAME_SIZE = 128;

    class Histogram
        : public NONPAGED_OBJECT<'mrGH'>
    {
        friend class Hub;
    public:
        NONPAGED
        Histogram(
            // 600 buckets with 50 ms width cover 30 seconds
            _In_ ULONG64 HistogramSmallestValue = 0,
            _In_ ULONG64 HistogramBucketWidth = 50,
            _In_ ULONG HistogramNumBuckets = 600
        );

        NONPAGED
        void
        AddValue(
            _In_ ULONG64 Value
        );

        PAGED
        void
        SetName(
            _In_ UNICODE_STRING const & Name
        );

        PAGED
        void
        ResetValues(
            void
        );

        template<typename T>
        PAGED
        ULONG
        SerializedSize() const;

        template<typename T>
        PAGED
        NTSTATUS
        Serialize(
            _Out_writes_bytes_(cbOut) T *out,
            _In_ ULONG cbOut,
            _Out_ ULONG &bytesNeeded
        ) const;

    private:
        KPtr<KHistogram>
            m_histogram;

        WCHAR
            m_name[HIST_NAME_SIZE] = {};
    };
}
