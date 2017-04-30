// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

template<typename TElement, typename FAdvance>
class KIntrusiveListIterator
{
public:
    using value_type = TElement;
    using pointer = value_type *;

private:
    pointer m_pElement;

    pointer GetNext()
    {
        return FAdvance()(m_pElement);
    }

public:

    KIntrusiveListIterator() : m_pElement(nullptr) {}
    KIntrusiveListIterator(pointer pElement) : m_pElement(pElement) {}

    KIntrusiveListIterator & operator++()
    {
        m_pElement = GetNext();
        return *this;
    }

    KIntrusiveListIterator operator++(int)
    {
        auto copy = *this;
        ++*this;
        return copy;
    }

    value_type & operator*()
    {
        return *m_pElement;
    }

    value_type const & operator*() const
    {
        return *m_pElement;
    }

    pointer operator->() const
    {
        return m_pElement;
    }

    bool operator==(KIntrusiveListIterator const &other) const
    {
        return m_pElement == other.m_pElement;
    }

    bool operator!=(KIntrusiveListIterator const &other) const
    {
        return m_pElement != other.m_pElement;
    }
};

#define MAKE_INTRUSIVE_LIST_ENUMERABLE_FUNCTOR(Type, Functor) \
    namespace std \
    { \
        inline auto begin(Type *pHead) { return KIntrusiveListIterator<Type, Functor>(pHead); } \
        inline auto end(Type *)   { return KIntrusiveListIterator<Type, Functor>();      } \
    }

#define MAKE_INTRUSIVE_LIST_ENUMERABLE(Type, Field) \
    struct _K_ ## Type ## _Advance \
    { \
        Type *operator()(Type *current) const \
        { \
            return current->Field; \
        } \
    }; \
    \
    MAKE_INTRUSIVE_LIST_ENUMERABLE_FUNCTOR(Type, _K_ ## Type ## _Advance)
