// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "KMacros.h"

class KSpinLock
{
public:

    KSpinLock()
    {
#if _KERNEL_MODE
        KeInitializeSpinLock(&m_lock);
#else
        InitializeSRWLock(&m_lock);
#endif
    }

    _Requires_lock_not_held_(*this)
    _Acquires_lock_(*this)
    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_saves_
    _IRQL_raises_(DISPATCH_LEVEL)
    KIRQL Acquire()
    {
#if _KERNEL_MODE
        KIRQL oldIrql;
        KeAcquireSpinLock(&m_lock, &oldIrql);
        return oldIrql;
#else
        AcquireSRWLockExclusive(&m_lock);
        return 0;
#endif
    }

    _Requires_lock_held_(*this)
    _Releases_lock_(*this)
    _IRQL_requires_(DISPATCH_LEVEL)
    void Release(_In_ KIRQL oldIrql)
    {
#if _KERNEL_MODE
        return KeReleaseSpinLock(&m_lock, oldIrql);
#else
        UNREFERENCED_PARAMETER(oldIrql);
        ReleaseSRWLockExclusive(&m_lock);
#endif
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool IsAcquired()
    {
#if _KERNEL_MODE
        return !KeTestSpinLock(&m_lock);
#else
        if (!TryAcquireSRWLockExclusive(&m_lock))
            return false;

        ReleaseSRWLockExclusive(&m_lock);
        return true;
#endif
    }

private:

#if _KERNEL_MODE
    KSPIN_LOCK m_lock;
#else
    SRWLOCK m_lock;
#endif
};

class KAcquireSpinLock
{
public:

    _Requires_lock_not_held_(lock)
    _Acquires_lock_(lock)
    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_raises_(DISPATCH_LEVEL)
    KAcquireSpinLock(KSpinLock &lock) :
        m_lock(lock)
    {
        Acquire();
    }

    _Requires_lock_held_(this->m_lock)
    _Releases_lock_(this->m_lock)
    _IRQL_requires_(DISPATCH_LEVEL)
    ~KAcquireSpinLock()
    {
        if (IsAcquired())
            Release();
    }

    _Requires_lock_not_held_(this->m_lock)
    _Acquires_lock_(this->m_lock)
    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_raises_(DISPATCH_LEVEL)
    void Acquire()
    {
        WIN_ASSERT(!IsAcquired());
        m_oldIrql = m_lock.Acquire();
    }

    _Requires_lock_held_(this->m_lock)
    _Releases_lock_(this->m_lock)
    _IRQL_requires_(DISPATCH_LEVEL)
    void Release()
    {
        WIN_ASSERT(IsAcquired());
        m_lock.Release(m_oldIrql);
        m_oldIrql = NOT_ACQUIRED;
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool IsAcquired()
    {
        return m_oldIrql != NOT_ACQUIRED;
    }

private:

    static const KIRQL NOT_ACQUIRED = (KIRQL)-1;
    KIRQL m_oldIrql = NOT_ACQUIRED;
    KSpinLock &m_lock;
};
