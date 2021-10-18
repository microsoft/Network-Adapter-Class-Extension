
// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <knew.h>
#include <KPushLock.h>
#include <KArray.h>
#include "Histogram.h"

namespace Statistics
{
    class Hub
        : public NONPAGED_OBJECT<'bhTS'>
    {
    public:
        PAGED
        bool
        AddHistogramReference(
            _In_ Histogram& histogram
        );

        PAGED
        void
        RemoveHistogramReference(
            _In_ Histogram& histogram
        );

        PAGED
        void
        ResetHistogramsValues(
            void
        );

        template<typename T>
        PAGED
        ULONG
        SerializedSize(
            void
        ) const;

        template<typename T>
        PAGED
        NTSTATUS
        Serialize(
            _Out_writes_bytes_(cbOut) T *out,
            _In_ ULONG cbOut,
            _Out_ ULONG &bytesNeeded
        ) const;

    private:
        _Guarded_by_(m_histogramsListLock)
        Rtl::KArray<Histogram*>
            m_histogramsArray { 50 };

        mutable KPushLock
            m_histogramsListLock;
    };
}
