#pragma once

// RtlHomogenousSequence is a utility class that detects whether a sequence of
// integers is homogenous, i.e., consists entirely of the same value repeated.
//
//     RtlHomogenousSequence<ULONG> s;
//     s.AddValue(42);
//     s.AddValue(42);
//     s.AddValue(42);
//     Assert(s.IsHomogenous());
//
//     s.AddValue(43);
//     Assert(!s.IsHomogenous());
//

template<typename TINTEGER>
class RtlHomogenousSequence
{
public:

    void AddValue(TINTEGER i)
    {
        m_setBits |= i;
        m_clearBits |= ~i;
    }

    void Combine(_In_ RtlHomogenousSequence& other)
    {
        m_setBits |= other.m_setBits;
        m_clearBits |= other.m_clearBits;
    }

    bool IsHomogenous() const
    {
        return m_setBits == static_cast<TINTEGER>(~m_clearBits);
    }

    TINTEGER GetSetBits() const
    {
        return m_setBits;
    }

    void Reset()
    {
        m_setBits = 0;
        m_clearBits = 0;
    }

private:

    TINTEGER m_setBits = 0;
    TINTEGER m_clearBits = 0;
};

