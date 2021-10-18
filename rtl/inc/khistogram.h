
#pragma once

#include <ntintsafe.h>

class KRTL_CLASS KHistogram final :
    public NONPAGED_OBJECT<'tsHK'>
{
private:

    PAGED KHistogram(ULONG64 smallestValue, ULONG64 bucketWidth, ULONG numBuckets) :
        m_smallestValue{ smallestValue },
        m_bucketWidth{ bucketWidth },
        m_numBuckets{ numBuckets }
    {
        RtlZeroMemory(m_buckets, m_numBuckets * sizeof(m_buckets[0]));
    }

public:

    KHistogram(KHistogram &) = delete;
    KHistogram(KHistogram &&) = delete;
    KHistogram & operator=(KHistogram &) = delete;
    KHistogram && operator=(KHistogram &&) = delete;

    static PAGED KHistogram *Create(ULONG64 smallestValue, ULONG64 bucketWidth, ULONG numBuckets)
    {
        // A histogram with 1 bucket isn't very useful.
        if (numBuckets < 2)
        {
            return nullptr;
        }

        // Check for overflow in (smallestValue + bucketWidth * numBuckets).
        ULONG64 tmp;
        if (STATUS_SUCCESS != RtlULongLongMult(bucketWidth, numBuckets, &tmp) ||
            STATUS_SUCCESS != RtlULongLongAdd(smallestValue, tmp, &tmp))
        {
            return nullptr;
        }

        // Check for overflow in (numBuckets * sizeof(bucket) + sizeof(KHistogram)).
        ULONG allocationSize;
        if (STATUS_SUCCESS != RtlULongMult(numBuckets, sizeof(KHistogram::m_buckets[0]), &allocationSize) ||
            STATUS_SUCCESS != RtlULongAdd(FIELD_OFFSET(KHistogram, m_buckets), allocationSize, &allocationSize))
        {
            return nullptr;
        }

        auto result = reinterpret_cast<KHistogram *>(ExAllocatePool2(POOL_FLAG_NON_PAGED, allocationSize, 'tsHK'));
        if (!result)
        {
            return nullptr;
        }

        new(result) KHistogram{ smallestValue, bucketWidth, numBuckets };
        return result;
    }

    NONPAGED void AddValue(ULONG64 value)
    {
        ULONG bias = (m_smallestValue > 0) ? 1 : 0;

        if (value < m_smallestValue)
        {
            // The 0th bucket means "smaller than the minimum value"
            IncrementBucket(0);
            return;
        }

        auto offset = value - m_smallestValue;
        auto largestValue = (m_numBuckets - 1 - bias) * m_bucketWidth;
        if (offset >= largestValue)
        {
            // The last bucket means "bigger than the maximum value"
            IncrementBucket(m_numBuckets - 1);
            return;
        }

        auto bucket = offset / m_bucketWidth;
        IncrementBucket(bucket + bias);
    }

    NONPAGED void ResetValues()
    {
        RtlZeroMemory(m_buckets, sizeof(m_buckets[0]) * m_numBuckets);
        MemoryBarrier();
    }

    NONPAGED ULONG GetBucketsMemorySize() const { return sizeof(m_buckets[0]) * m_numBuckets; }

    NONPAGED ULONG GetNumBuckets() const { return m_numBuckets; }
    NONPAGED ULONG64 GetBucketWidth() const { return m_bucketWidth; }
    NONPAGED ULONG64 GetSmallestValue() const { return m_smallestValue; }

    NONPAGED USHORT const volatile *GetBuckets() const { MemoryBarrier(); return m_buckets; }

private:

    NONPAGED void IncrementBucket(ULONG64 index)
    {
        WIN_ASSERT(index < m_numBuckets);

        auto &value = m_buckets[static_cast<size_t>(index)];

#ifndef _X86_
        auto inc = InterlockedIncrementNoFence16(reinterpret_cast<SHORT*>(&value));
#else
        auto inc = InterlockedIncrement16(reinterpret_cast<SHORT*>(&value));
#endif

        // Avoid wraparound
        if (static_cast<USHORT>(inc) > 0xFF00)
        {
#ifndef _X86_
            InterlockedDecrementNoFence16(reinterpret_cast<SHORT*>(&value));
#else
            InterlockedDecrement16(reinterpret_cast<SHORT*>(&value));
#endif
        }
    }

    ULONG64 m_smallestValue = 0;
    ULONG64 m_bucketWidth = 0;
    ULONG m_numBuckets = 0;

    USHORT m_buckets[ANYSIZE_ARRAY];
};
