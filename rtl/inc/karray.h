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
    first: http://msdn.microsoft.com/library/sxcsf7y7

--*/

#pragma once

#include "KMacros.h"
#include "KNew.h"
#include <ntintsafe.h>
#include <wil\wistd_type_traits.h>

class ndisTriageClass;
extern "C" VOID ndisInitGlobalTriageBlock();

namespace Rtl
{

template<typename T>
class PAGED KArray :
    public PAGED_OBJECT<'rrAK'>
{
public:

    // This iterator is not a full implementation of a STL-style iterator.
    // This is the bare minimum to get C++'s syntax "for(x : y)" to work.
    class iterator
    {
    public:

        PAGED iterator(KArray<T> *a, ULONG i) : _a(a), _i(i)
        { }

        PAGED void operator++() { _i++; }

        PAGED T &operator*() { return (*_a)[_i]; }

        PAGED bool operator==(iterator const &rhs) const { return rhs._i == _i; }
        PAGED bool operator!=(iterator const &rhs) const { return !(rhs == *this); }

    private:

        KArray<T> *_a;
        ULONG _i;
    };

    PAGED KArray(size_t sizeHint = 0) WI_NOEXCEPT :    
        m_bufferSize(0),
        m_numElements(0),
        _p(nullptr)
    {
        (void)grow(sizeHint);
    }

    PAGED ~KArray()
    {
        reset();
    }

    PAGED KArray(
        _In_ KArray<T> &&rhs) WI_NOEXCEPT :
            _p(rhs._p),
            m_numElements(rhs.m_numElements),
            m_bufferSize(rhs.m_bufferSize)
    {
        rhs._p = nullptr;
        rhs.m_numElements = 0;
        rhs.m_bufferSize = 0;
    }
            
    PAGED KArray &operator=(
        _In_ KArray<T> &&rhs)
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
    
    PAGED size_t count() const
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

        T * p = (T*)ExAllocatePoolWithTag(PagedPool, bytesNeeded, 'rrAK');
        if (!p)
            return false;

        if (isBlittableType())
        {
            memcpy(p, _p, m_numElements * sizeof(T));
        }
        else
        {
            for (ULONG i = 0; i < m_numElements; i++)
                new (&p[i]) T(wistd::move(_p[i]));
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

        for (size_t i = m_numElements; i < count; i++)
        {
            new(&_p[i]) T;
        }

        for (size_t i = count; i < m_numElements; i++)
        {
            _p[i].~T();
        }

        m_numElements = static_cast<ULONG>(count);
        return true;
    }

    PAGED bool append(T const &t)
    {
        if (!grow(m_numElements+1))
            return false;
        
        new(&_p[m_numElements]) T(t);    
        ++m_numElements;
        return true;
    }

    PAGED bool append(T &&t)
    {
        if (!grow(m_numElements+1))
            return false;
        
        new(&_p[m_numElements]) T(wistd::move(t));
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

        new(&_p[index]) T(t);
        ++m_numElements;
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

        new(&_p[index]) T(wistd::move(t));
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
            RtlFailFast(0xBAD0FF);

        _p[index].~T();
        moveElements((ULONG)(index+1), (ULONG)index, (ULONG)(m_numElements - index - 1));
        --m_numElements;
    }

    PAGED T &operator[](size_t index) const
    {
        if (index >= m_numElements)
            RtlFailFast(0xBAD0FF);
        
        return _p[index];
    }

    PAGED iterator begin()
    {
        return iterator(this, 0);
    }

    PAGED iterator end()
    {
        return iterator(this, m_numElements);    
    }

private:

    PAGED void reset()
    {
        if (_p)
        {
            for (auto i = m_numElements; i > 0; i--)
            {
                _p[i-1].~T();
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
            NOTHING;
        }
        else if (isBlittableType())
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
                new (&_p[i - 1]) T(wistd::move(_p[i - delta - 1]));
            }

            for (NOTHING; i > to; i--)
            {
                _p[i - 1].~T();
                new (&_p[i - 1]) T(wistd::move(_p[i - delta - 1]));
            }

            for (NOTHING; i > from; i--)
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
                new (&_p[i]) T(wistd::move(_p[i + delta]));
            }

            for (NOTHING; i < to + number; i++)
            {
                _p[i].~T();
                new (&_p[i]) T(wistd::move(_p[i + delta]));
            }

            for (NOTHING; i < from + number; i++)
            {
                _p[i].~T();
            }
        }
    }

    PAGED bool isBlittableType() const
    {
        // can also add is_integral to this, but we'd have to implement
        // it ourselves.
        return __is_pod(T) || __is_enum(T);
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

    // Disabled

    KArray(KArray<T> &);

    KArray &operator=(KArray<T> &);

    ULONG m_bufferSize;
    ULONG m_numElements;
    T *_p;
};

}
