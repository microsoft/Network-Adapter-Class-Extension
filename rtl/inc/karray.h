/*++
Copyright (c) Microsoft Corporation

Module Name:

    KArray.h

Abstract:

    Implements a C++ container analogous to std::vector

Environment:

    Kernel mode or usermode unittest

Notes:

    Because kernel C++ doesn't support exceptions, we can't use the STL directly
    in kernelmode.  Therefore, this class provides a limited and slightly
    modified subset of the STL's std::vector.

    If you're not familiar with std::vector, you should go read up on it
    first: https://docs.microsoft.com/en-us/cpp/standard-library/vector-class

--*/

#pragma once

#include <KMacros.h>
#include <KNew.h>
#include <ntintsafe.h>
#include <wil/wistd_type_traits.h>

#ifndef _KERNEL_MODE
#include "UmPool.h"
#endif // _KERNEL_MODE

class ndisTriageClass;
VOID ndisInitGlobalTriageBlock();

namespace Rtl
{

template<typename T, POOL_TYPE PoolType = PagedPool>
class KRTL_CLASS KArray :
    public PAGED_OBJECT<'rrAK'>
{
public:

    static_assert(((PoolType == NonPagedPoolNxCacheAligned) && (alignof(T) <= SYSTEM_CACHE_ALIGNMENT_SIZE)) ||
                  (alignof(T) <= MEMORY_ALLOCATION_ALIGNMENT), 
                  "This container allocates items with a fixed alignment");

    // This iterator is not a full implementation of a STL-style iterator.
    // Mostly this is only here to get C++'s syntax "for(x : y)" to work.
    class const_iterator
    {
    friend class KArray;
    protected:

        PAGED const_iterator(KArray const *a, size_t i) : _a{ const_cast<KArray*>(a) }, _i{ i } { ensure_valid(); }

    public:

        const_iterator() = delete;
        PAGED const_iterator(const_iterator const &rhs) : _a { rhs._a }, _i{ rhs._i } { }
        PAGED ~const_iterator() = default;

        PAGED const_iterator &operator=(const_iterator const &rhs) { _a = rhs._a; _i = rhs._i; return *this; }

        PAGED const_iterator &operator++() { _i++; return *this; }
        PAGED const_iterator operator++(int) { auto result = *this; ++(*this); return result; }

        PAGED const_iterator &operator+=(size_t offset) { _i += offset; return *this;}

        PAGED T const &operator*() const { return (*_a)[_i]; }
        PAGED T const *operator->() const { return &(*_a)[_i]; }

        PAGED bool operator==(const_iterator const &rhs) const { return rhs._i == _i; }
        PAGED bool operator!=(const_iterator const &rhs) const { return !(rhs == *this); }

    protected:

        PAGED void ensure_valid() const { if (_i > _a->count()) RtlFailFast(FAST_FAIL_RANGE_CHECK_FAILURE); }

        KArray *_a;
        size_t _i;
    };

    class iterator : public const_iterator
    {
    friend class KArray;
    protected:

        PAGED iterator(KArray *a, size_t i) : const_iterator{ a, i } {}

    public:

        PAGED T &operator*() const { return (*_a)[_i]; }
        PAGED T *operator->() const { return &(*_a)[_i]; }
    };

    PAGED KArray(size_t sizeHint = 0) noexcept
    {
        if (sizeHint)
            (void)grow(sizeHint);
    }

    NONPAGED ~KArray()
    {
        reset();
    }

    PAGED KArray(
        _In_ KArray &&rhs) noexcept :
            _p(rhs._p),
            m_numElements(rhs.m_numElements),
            m_bufferSize(rhs.m_bufferSize)
    {
        rhs._p = nullptr;
        rhs.m_numElements = 0;
        rhs.m_bufferSize = 0;
    }

    KArray(KArray &) = delete;

    KArray &operator=(KArray &) = delete;

    PAGED KArray &operator=(
        _In_ KArray &&rhs)
    {
        reset();

        this->_p = rhs._p;
        this->m_numElements = rhs.m_numElements;
        this->m_bufferSize = rhs.m_bufferSize;

        rhs._p = nullptr;
        rhs.m_numElements = 0;
        rhs.m_bufferSize = 0;

        return *this;
    }

    NONPAGED size_t count() const
    {
        return m_numElements;
    }

    PAGED bool reserve(size_t count)
    {
        if (m_bufferSize >= count)
            return true;

        if (count >= (ULONG)(-1))
            return false;

        size_t bytesNeeded;
        if (!NT_SUCCESS(RtlSIZETMult(sizeof(T), count, reinterpret_cast<SIZE_T*>(&bytesNeeded))))
            return false;

        T * p = (T*)ExAllocatePoolWithTag(PoolType, bytesNeeded, 'rrAK');
        if (!p)
            return false;

        if constexpr(__is_trivially_copyable(T))
        {
            memcpy(p, _p, m_numElements * sizeof(T));
        }
        else
        {
            for (ULONG i = 0; i < m_numElements; i++)
                new (wistd::addressof(p[i])) T(wistd::move(_p[i]));
        }

        if (_p)
        {
            for (ULONG i = 0; i < m_numElements; i++)
                _p[i].~T();

            ExFreePoolWithTag(_p, 'rrAK');
        }

        m_bufferSize = static_cast<ULONG>(count);
        _p = p;

        return true;
    }

    PAGED bool resize(size_t count)
    {
        if (!reserve(count))
            return false;

        if constexpr(wistd::is_trivially_default_constructible_v<T>)
        {
            if (count > m_numElements)
            {
                memset(wistd::addressof(_p[m_numElements]), 0, (count - m_numElements) * sizeof(T));
            }
        }
        else
        {
            for (size_t i = m_numElements; i < count; i++)
            {
                new(wistd::addressof(_p[i])) T();
            }
        }

        if constexpr(!wistd::is_trivially_destructible_v<T>)
        {
            for (size_t i = count; i < m_numElements; i++)
            {
                _p[i].~T();
            }
        }

        m_numElements = static_cast<ULONG>(count);
        return true;
    }

    PAGED void clear(void)
    {
        (void)resize(0);
    }

    PAGED bool append(T const &t)
    {
        if (!grow(m_numElements+1))
            return false;

        new(wistd::addressof(_p[m_numElements])) T(t);
        ++m_numElements;
        return true;
    }

    PAGED bool append(T &&t)
    {
        if (!grow(m_numElements+1))
            return false;

        new(wistd::addressof(_p[m_numElements])) T(wistd::move(t));
        ++m_numElements;
        return true;
    }

    PAGED bool insertAt(size_t index, T &t)
    {
        if (index > m_numElements)
            return false;

        if (!grow(m_numElements+1))
            return false;

        if (index < m_numElements)
            moveElements((ULONG)index, (ULONG)(index+1), (ULONG)(m_numElements - index));

        new(wistd::addressof(_p[index])) T(t);
        ++m_numElements;
        return true;
    }

    PAGED bool insertAt(const iterator &destination, const const_iterator &start, const const_iterator &end)
    {
        if (end._i < start._i || destination._a != this)
            RtlFailFast(FAST_FAIL_INVALID_ARG);
        if (end._i == start._i)
            return true;

        const size_t countToInsert = end._i - start._i;

        size_t countToGrow;
        if (!NT_SUCCESS(RtlSIZETAdd(m_numElements, countToInsert, reinterpret_cast<SIZE_T*>(&countToGrow))))
            return false;

        if (!grow(countToGrow))
            return false;

        moveElements((ULONG)destination._i, (ULONG)(destination._i+countToInsert), (ULONG)(m_numElements - destination._i));

        if constexpr(__is_trivially_copyable(T))
        {
            memcpy(_p + destination._i, wistd::addressof((*start._a)[start._i]), countToInsert * sizeof(T));
        }
        else
        {
            const_iterator readCursor(start);
            iterator writeCursor(destination);
            while (readCursor != end)
            {
                new(wistd::addressof(_p[writeCursor._i])) T(wistd::move((*readCursor._a)[readCursor._i]));
                writeCursor++;
                readCursor++;
            }
        }

        m_numElements += static_cast<ULONG>(countToInsert);
        return true;
    }

    PAGED bool insertAt(size_t index, T &&t)
    {
        if (index > m_numElements)
            return false;

        if (!grow(m_numElements+1))
            return false;

        if (index < m_numElements)
            moveElements((ULONG)index, (ULONG)(index+1), (ULONG)(m_numElements - index));

        new(wistd::addressof(_p[index])) T(wistd::move(t));
        ++m_numElements;
        return true;
    }

    PAGED bool insertSorted(T &t, bool (*lessThanPredicate)(T const&, T const&))
    {
        for (size_t i = 0; i < m_numElements; i++)
        {
            if (!lessThanPredicate(_p[i], t))
            {
                return insertAt(i, t);
            }
        }

        return append(t);
    }

    PAGED bool insertSorted(T &&t, bool (*lessThanPredicate)(T const&, T const&))
    {
        for (size_t i = 0; i < m_numElements; i++)
        {
            if (!lessThanPredicate(_p[i], t))
            {
                return insertAt(i, wistd::move(t));
            }
        }

        return append(wistd::move(t));
    }

    PAGED bool insertSortedUnique(T &t, bool (*lessThanPredicate)(T const&, T const&))
    {
        for (size_t i = 0; i < m_numElements; i++)
        {
            if (!lessThanPredicate(_p[i], t))
            {
                if (lessThanPredicate(t, _p[i]))
                    return insertAt(i, t);
                else
                    return true;
            }
        }

        return append(t);
    }

    PAGED bool insertSortedUnique(T &&t, bool (*lessThanPredicate)(T const&, T const&))
    {
        for (size_t i = 0; i < m_numElements; i++)
        {
            if (!lessThanPredicate(_p[i], t))
            {
                if (lessThanPredicate(t, _p[i]))
                    return insertAt(i, wistd::move(t));
                else
                    return true;
            }
        }

        return append(wistd::move(t));
    }

    PAGED void eraseAt(size_t index)
    {
        if (index >= m_numElements)
            RtlFailFast(FAST_FAIL_INVALID_ARG);

        _p[index].~T();
        moveElements((ULONG)(index+1), (ULONG)index, (ULONG)(m_numElements - index - 1));
        --m_numElements;
    }

    NONPAGED T &operator[](size_t index)
    {
        if (index >= m_numElements)
            RtlFailFast(FAST_FAIL_INVALID_ARG);

        return _p[index];
    }

    NONPAGED T const &operator[](size_t index) const
    {
        if (index >= m_numElements)
            RtlFailFast(FAST_FAIL_INVALID_ARG);

        return _p[index];
    }

    PAGED iterator begin()
    {
        return { this, 0 };
    }

    PAGED const_iterator begin() const
    {
        return { this, 0 };
    }

    PAGED iterator end()
    {
        return { this, m_numElements };
    }

    PAGED const_iterator end() const
    {
        return { this, m_numElements };
    }

private:

    NONPAGED void reset()
    {
        if (_p)
        {
            if constexpr(!wistd::is_trivially_destructible_v<T>)
            {
                for (auto i = m_numElements; i > 0; i--)
                {
                    _p[i-1].~T();
                }
            }

            ExFreePoolWithTag(_p, 'rrAK');
            _p = nullptr;
            m_numElements = 0;
            m_bufferSize = 0;
        }
    }

    PAGED void moveElements(ULONG from, ULONG to, ULONG number)
    {
        if (from == to || number == 0)
        {
            return;
        }
        else if constexpr(__is_trivially_copyable(T))
        {
            memmove(_p + to, _p + from, number * sizeof(T));
        }
        else if (from < to)
        {
            WIN_ASSERT(m_numElements == from + number);

            ULONG delta = to - from;
            ULONG i;

            for (i = to + number; i - 1 >= m_numElements; i--)
            {
                new (wistd::addressof(_p[i - 1])) T(wistd::move(_p[i - delta - 1]));
            }

            for (; i > to; i--)
            {
                _p[i - 1].~T();
                new (wistd::addressof(_p[i - 1])) T(wistd::move(_p[i - delta - 1]));
            }

            for (; i > from; i--)
            {
                _p[i - 1].~T();
            }
        }
        else
        {
            WIN_ASSERT(m_numElements == from + number);

            ULONG delta = from - to;
            ULONG i;

            for (i = to; i < from; i++)
            {
                new (wistd::addressof(_p[i])) T(wistd::move(_p[i + delta]));
            }

            for (; i < to + number; i++)
            {
                _p[i].~T();
                new (wistd::addressof(_p[i])) T(wistd::move(_p[i + delta]));
            }

            for (; i < from + number; i++)
            {
                _p[i].~T();
            }
        }
    }

    PAGED bool grow(size_t count)
    {
        if (m_bufferSize >= count)
            return true;

        if (count < 4)
            count = 4;

        size_t exponentialGrowth = m_bufferSize + m_bufferSize / 2;
        if (count < exponentialGrowth)
            count = exponentialGrowth;

        return reserve(count);
    }

    friend class ::ndisTriageClass;
    friend VOID ::ndisInitGlobalTriageBlock();

    ULONG m_bufferSize = 0;
    ULONG m_numElements = 0;
    T *_p = nullptr;
};

}
