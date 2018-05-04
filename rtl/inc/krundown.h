
#pragma once

#if !_KERNEL_MODE
#include "KLockHolder.h"
#include "KWaitEvent.h"
#endif

class KRTL_CLASS KRundownBase
{
public:

    KRundownBase() = default;
    ~KRundownBase() = default;

    KRundownBase(KRundownBase &) = delete;
    KRundownBase(KRundownBase &&) = delete;
    KRundownBase & operator=(KRundownBase &) = delete;
    KRundownBase & operator=(KRundownBase &&) = delete;

    NONPAGED bool TryAcquire()
    {
#if _KERNEL_MODE
        return !!ExAcquireRundownProtection(&m_rundown);
#else
        return TryAcquire(1);
#endif
    }

    NONPAGED bool TryAcquire(ULONG count)
    {
#if _KERNEL_MODE
        return !!ExAcquireRundownProtectionEx(&m_rundown, count);
#else
        KLockThisExclusive lock(m_lock);
        if (m_closed)
            return false;
        m_references += count;
        m_ready.Clear();
        return true;
#endif
    }

    NONPAGED void Release()
    {
#if _KERNEL_MODE
        ExReleaseRundownProtection(&m_rundown);
#else
        Release(1);
#endif
    }

    NONPAGED void Release(ULONG count)
    {
#if _KERNEL_MODE
        ExReleaseRundownProtectionEx(&m_rundown, count);
#else
        KLockThisExclusive lock(m_lock);
        WIN_ASSERT(m_references >= count);
        m_references -= count;
        if (0 == m_references)
            m_ready.Set();
#endif
    }

    PAGED void CloseAndWait()
    {
        PAGED_CODE();
#if _KERNEL_MODE
        ExWaitForRundownProtectionRelease(&m_rundown);
#else
        KLockThisExclusive lock(m_lock);
        m_closed = true;
        lock.Release();
        m_ready.Wait();
#endif
    }

    NONPAGED void Reinitialize()
    {
#if _KERNEL_MODE
        ExReInitializeRundownProtection(&m_rundown);
#else
        KLockThisExclusive lock(m_lock);
        WIN_ASSERT(m_references == 0);
        m_closed = false;
#endif
    }

    NONPAGED bool Test()
    {
        if (!TryAcquire())
            return false;
        Release();
        return true;
    }

protected:

    PAGED void InitializeBase()
    {
        PAGED_CODE();
#if _KERNEL_MODE
        ExInitializeRundownProtection(&m_rundown);
#else
        m_ready.Set();
#endif
    }

private:

#ifdef _KERNEL_MODE
    EX_RUNDOWN_REF m_rundown;
#else
    KPushLock m_lock;
    KWaitEvent m_ready;
    ULONG m_references = 0;
    bool m_closed = false;
#endif
};

class KRTL_CLASS KRundown : public KRundownBase
{
public:

    PAGED KRundown()
    {
        PAGED_CODE();
        InitializeBase();
    }
};

class KRTL_CLASS KRundownManualConstruct : public KRundownBase
{
public:

    PAGED KRundownManualConstruct() = default;
    PAGED ~KRundownManualConstruct() = default;

    PAGED void Initialize()
    {
        PAGED_CODE();
        InitializeBase();
    }
};


