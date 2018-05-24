
#pragma once

#include "KMacros.h"

template <typename TCONTEXT, typename TBASE>
class KWorkItemBase
{
protected:

    typedef
    _IRQL_requires_(PASSIVE_LEVEL)
    void (TCONTEXT::*CallbackFunctionPtr)();

    PAGED KWorkItemBase(_In_ TCONTEXT *context, _In_ CallbackFunctionPtr callback) noexcept :
        m_context(context),
        m_callback(callback)
    {
#if _KERNEL_MODE
        ExInitializeWorkItem(&m_workitem, &CallbackThunk, this);
#else
        m_workitem = CreateThreadpoolWork(&CallbackThunk, this, nullptr);
        WIN_VERIFY(m_workitem);
#endif
    }

    PAGED ~KWorkItemBase()
    {
#ifndef _KERNEL_MODE
        CloseThreadpoolWork(m_workitem);
#endif
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool QueueInner()
    {
#if _KERNEL_MODE
        ExQueueWorkItem(&m_workitem, NormalWorkQueue);
#else
        SubmitThreadpoolWork(m_workitem);
#endif
        return true;
    }

    PAGED void InvokeInner()
    {
        (m_context->*m_callback)();
    }

private:

#if _KERNEL_MODE

    PAGED static VOID CallbackThunk(
        PVOID Context)
    {
        auto *This = reinterpret_cast<TBASE*>(Context);
        This->Invoke();
    }

#else

    static void CallbackThunk(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID Context,
        PTP_WORK Work)
    {
        UNREFERENCED_PARAMETER((Instance, Work));
        auto *This = reinterpret_cast<TBASE*>(Context);
        This->Invoke();
    }

#endif

#if _KERNEL_MODE
    WORK_QUEUE_ITEM m_workitem;
#else
    TP_WORK *m_workitem;
#endif

    KWorkItemBase(KWorkItemBase &);
    KWorkItemBase(KWorkItemBase &&);
    KWorkItemBase &operator=(KWorkItemBase &);
    KWorkItemBase &operator=(KWorkItemBase &&);

    TCONTEXT *m_context;
    CallbackFunctionPtr m_callback;
};

template<typename TCONTEXT>
class KWorkItem : public KWorkItemBase<TCONTEXT, KWorkItem<TCONTEXT>>
{
public:

    PAGED KWorkItem(_In_ TCONTEXT *context, _In_ CallbackFunctionPtr callback) noexcept :
        KWorkItemBase<TCONTEXT, KWorkItem<TCONTEXT>>(context, callback)
    {

    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void Queue()
    {
        QueueInner();
    }

    PAGED void Invoke()
    {
        InvokeInner();
    }
};

template<typename TCONTEXT>
class KCoalescingWorkItem : protected KWorkItemBase<TCONTEXT, KCoalescingWorkItem<TCONTEXT>>
{
public:

    PAGED KCoalescingWorkItem(_In_ TCONTEXT *context, _In_ CallbackFunctionPtr callback) :
        KWorkItemBase<TCONTEXT, KCoalescingWorkItem<TCONTEXT>>(context, callback),
        m_queued(false)
    {

    }

    PAGED ~KCoalescingWorkItem()
    {
        WIN_ASSERT(m_queued == false);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    bool Queue()
    {
        if (InterlockedCompareExchange(&m_queued, true, false))
            return false;

        QueueInner();
        return true;
    }

    PAGED void Invoke()
    {
        WIN_VERIFY(InterlockedExchange(&m_queued, false));
        InvokeInner();
    }

private:

    _Interlocked_
    LONG m_queued;
};

template<typename TCONTEXT>
class KRepeatingWorkItem : protected KWorkItemBase<TCONTEXT, KCoalescingWorkItem<TCONTEXT>>
{
public:

    PAGED KRepeatingWorkItem(_In_ TCONTEXT *context, _In_ CallbackFunctionPtr callback) :
        KWorkItemBase<TCONTEXT, KCoalescingWorkItem<TCONTEXT>>(context, callback),
        m_count(0)
    {

    }

    PAGED ~KRepeatingWorkItem()
    {
        WIN_ASSERT(m_count == 0);
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void Queue()
    {
        if (0 == InterlockedIncrement(&m_count))
        {
            QueueInner();
        }
    }

    PAGED void Invoke()
    {
        do
        {
            InvokeInner();

        } while (InterlockedDecrement(&m_count));
    }

private:

    _Interlocked_
    LONG m_count;
};


