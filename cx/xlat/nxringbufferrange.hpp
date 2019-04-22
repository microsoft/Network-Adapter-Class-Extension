// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    The NxRingBufferView allows iteration over the elements of a NET_RING.

--*/

#pragma once

#include <net/ring.h>
#include <net/packet.h>

#ifdef XLAT_UNIT_TEST
#include <iterator>
#endif

template<typename T>
class NetRingBufferRange;

template<typename T>
class NetRingBufferIterator
#ifdef XLAT_UNIT_TEST
    : public std::iterator<std::bidirectional_iterator_tag, T, UINT32>
#endif
{
    friend class NetRingBufferRange<T>;

    NET_RING const & m_rb;
    UINT32 m_index;

    static NET_RING * Handle(NET_RING const & rb) { return const_cast<NET_RING *>(&rb); }

public:

    NetRingBufferIterator(NET_RING const & rb, UINT32 index) :
        m_rb(rb),
        m_index(index)
    {
        NT_ASSERT(index < rb.NumberOfElements);
    }

    T &operator*()
    {
        return *(T*)NetRingGetElementAtIndex(Handle(m_rb), m_index);
    }

    T const &operator*() const
    {
        return *(T*)NetRingGetElementAtIndex(Handle(m_rb), m_index);
    }

    T *operator->()
    {
        return (T*)NetRingGetElementAtIndex(Handle(m_rb), m_index);
    }

    T const *operator->() const
    {
        return (T*)NetRingGetElementAtIndex(Handle(m_rb), m_index);
    }

    NetRingBufferIterator &operator++()
    {
        m_index = (m_index + 1) & m_rb.ElementIndexMask;
        return *this;
    }

    NetRingBufferIterator operator++(int)
    {
        auto it = *this;
        ++(*this);
        return it;
    }

    NetRingBufferIterator &operator--()
    {
        m_index = (m_index - 1) & m_rb.ElementIndexMask;
        return *this;
    }

    NetRingBufferIterator operator--(int)
    {
        auto it = *this;
        --(*this);
        return it;
    }

    bool operator==(NetRingBufferIterator const &other) const
    {
        NT_ASSERT(&m_rb == &other.m_rb);

        return m_index == other.m_index;
    }

    bool operator!=(NetRingBufferIterator const &other) const
    {
        NT_ASSERT(&m_rb == &other.m_rb);

        return m_index != other.m_index;
    }

    NetRingBufferIterator GetPrevious() const
    {
        auto previous = *this;
        return --previous;
    }

    NetRingBufferIterator GetNext() const
    {
        auto next = *this;
        return ++next;
    }

    UINT32 GetDistanceFrom(NetRingBufferIterator const &other) const
    {
        return NetRingGetRangeCount(Handle(m_rb), other.m_index, m_index);
    }

    UINT32 GetIndex() const
    {
        return m_index;
    }
};

template<typename T>
class NetRingBufferRange
{
    NET_RING const & m_rb;
    UINT32 m_begin, m_end;

public:
    using iterator = NetRingBufferIterator<T>;

    NetRingBufferRange(NET_RING const &rb, UINT32 begin, UINT32 end) :
        m_rb(rb),
        m_begin(begin),
        m_end(end)
    {
        NT_ASSERT(m_begin < rb.NumberOfElements);
        NT_ASSERT(m_end < rb.NumberOfElements);
    }

    NetRingBufferRange(iterator begin, iterator end) :
        NetRingBufferRange(begin.m_rb, begin.m_index, end.m_index)
    {
        NT_ASSERT(&begin.m_rb == &end.m_rb);
    }

    iterator begin() const { return iterator(m_rb, m_begin); }
    iterator end()   const { return iterator(m_rb, m_end);   }

    UINT32 Count() const
    {
        return (m_end - m_begin) & m_rb.ElementIndexMask;
    }

    bool HasElements() const
    {
        return m_begin != m_end;
    }

    T *At(UINT32 n)
    {
        NT_ASSERT(n < Count());

        return (T*)NetRingGetElementAtIndex(
            const_cast<NET_RING *>(&m_rb),
            (m_begin + n) & m_rb.ElementIndexMask);
    }

    T const *At(UINT32 n) const
    {
        NT_ASSERT(n < Count());

        return (T const *)NetRingGetElementAtIndex(
            const_cast<NET_RING *>(&m_rb),
            (m_begin + n) & m_rb.ElementIndexMask);
    }

    NET_RING const & RingBuffer() const
    {
        return m_rb;
    }

    static NetRingBufferRange OsRange(NET_RING const & rb)
    {
        // view only shows moveable elements
        return NetRingBufferRange(
            iterator(rb, rb.EndIndex),
            iterator(rb, rb.BeginIndex).GetPrevious());
    }

    static NetRingBufferRange ClientRange(NET_RING const & rb)
    {
        return NetRingBufferRange(rb, rb.BeginIndex, rb.EndIndex);
    }
};

using NetRbPacketRange = NetRingBufferRange<NET_PACKET>;
using NetRbPacketIterator = NetRingBufferIterator<NET_PACKET>;
using NetRbFragmentIterator = NetRingBufferIterator<NET_FRAGMENT>;
using NetRbFragmentRange = NetRingBufferRange<NET_FRAGMENT>;
