
#pragma once

#include "KRundown.h"
#include <wil\wistd_type_traits.h>

class KRTL_CLASS KRundownHolder
{
public:

    PAGED KRundownHolder(_In_ KRundown &rundown) :
        m_rundown(rundown) 
    {
        PAGED_CODE(); 
    }

    PAGED ~KRundownHolder()
    {
        PAGED_CODE();

        if (m_count)
            Release(m_count);
    }

    PAGED KRundownHolder(KRundownHolder &&rhs) :
        m_rundown(rhs.m_rundown),
        m_count(rhs.m_count)
    {
        rhs.m_count = 0;
    }

    KRundownHolder(KRundownHolder &) = delete;
    KRundownHolder &operator= (KRundownHolder &) = delete;
    KRundownHolder &operator= (KRundownHolder &&) = delete;

    PAGED bool TryAcquire()
    {
        PAGED_CODE();
        
        if (!m_rundown.TryAcquire())
            return false;

        m_count++;
        return true;
    }

    PAGED bool TryAcquire(ULONG count)
    {
        PAGED_CODE();
        
        if (!m_rundown.TryAcquire(count))
            return false;

        m_count += count;
        return true;
    }

    PAGED void Release()
    {
        PAGED_CODE();
        
        WIN_ASSERT(m_count > 0);
        m_count--;
        m_rundown.Release();
    }

    PAGED void Release(ULONG count)
    {
        PAGED_CODE();
        
        WIN_ASSERT(m_count >= count);
        m_count -= count;
        m_rundown.Release(count);
    }

private:

    KRundown &m_rundown;
    ULONG m_count = 0;
};

template<typename T>
class KRTL_CLASS KRundownPtr
{
public:

    PAGED KRundownPtr(nullptr_t) :
        _p(nullptr),
        m_rundown(*reinterpret_cast<KRundown*>(this))
    {
        // The conversion of this to a KRundown above is totally bogus.
        // But we have to give m_rundown's constructor *something* since it
        // takes a reference.  We know that the rundown won't actually
        // be used when _p is null, so it's sort of harmless to do this.
        PAGED_CODE();
    }

    PAGED KRundownPtr(T *p, KRundown &rundown) :
        _p(nullptr),
        m_rundown(rundown)
    {
        PAGED_CODE();

        if (p && m_rundown.TryAcquire())
            _p = p;
    }

    PAGED KRundownPtr(KRundownPtr &&rhs) :
        _p(nullptr),
        m_rundown(wistd::move(rhs.m_rundown))
    {
        PAGED_CODE();

        _p = rhs._p;
        rhs._p = nullptr;
    }

    KRundownPtr(KRundownPtr &) = delete;
    KRundownPtr &operator=(KRundownPtr &) = delete;
    KRundownPtr &operator=(KRundownPtr &&) = delete;

    PAGED operator bool() const { return _p != nullptr; }

    PAGED T &operator*() const { return *_p; }

    PAGED T *operator->() const { return &**this; }

    PAGED KRundownPtr<T> &operator=(nullptr_t)
    {
        if (_p != nullptr)
        {
            _p = nullptr;
            m_rundown.Release();
        }
        return *this;
    }
    
private:

    T *_p;
    KRundownHolder m_rundown;
};

