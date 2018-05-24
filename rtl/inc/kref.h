/*++
Copyright (c) Microsoft Corporation

Module Name:

    KRef.h

Abstract:

    Implements a C++ smart pointer analogous to std::shared_ptr

Environment:

    Kernel mode or usermode unittest

Usage:

    Because kernel C++ doesn't support exceptions, we can't use the STL directly
    in kernelmode.  Therefore, this class provides a very limited and slightly
    modified subset of the STL's std::shared_ptr.

    If you're not familiar with std::shared_ptr, you should go read up on it
    first: http://msdn.microsoft.com/library/bb982026

    The KRef class is conceptually meant to be:

        1.  A pointer to something
        2.  One of multiple owners of that thing's lifetime
        3.  Able to automatically clean up that thing when you're done using it
        4.  As efficient as manual interlocked reference-counting
        5.  Safe (common errors result in compile errors)

    KRef is appropriate when ownership of a thing is complicated, because
    there are multiple owners, and these owners may go away in a
    nondeterministic order.  If ownership is simpler, you should see KPtr
    instead.  KPtr is more efficient in CPU time and space than KRef.

    To use a KRef, you need to allocate the first instance with the allocate()
    method.  This method indirectly creates an instance of your thing:

        {
            KRef<SOME_TYPE> ref1;

            if (!ref1.allocate(L"abc", 123))
                return STATUS_INSUFFICIENT_RESOURCES;

            {
                KRef<SOME_TYPE> ref2 = ref1;  // refcount is bumped to 2

                // <ref2> goes out of scope, so refcount drops to 1
            }

            // <ref1> goes out of scope, so refcount drops to 0 and
            // the object instance is deleted
        }

    If you want to peek at a pointer to the target object, you can use
    the get() method:

        {
            KRef<SOME_TYPE> ref;

            if (!ref.allocate(L"abc", 123))
                return STATUS_INSUFFICIENT_RESOURCES;

            SOME_TYPE *p1 = ref.get();
        }

    KRef uses some compiler magic to elide some interlocked operations when
    feasible.  However, it's still possible to (ab)use KRef to cause a bunch
    of unneccesary interlocked operations.  Try to avoid copying/assigning
    KRefs.  For example, it's usually wasteful to pass a KRef by value:

        void Foo()
        {
            KRef<ITEM> ref = . . .;

            // Wasteful - does an extra pair of interlocked ref/deref
            Bar(ref);
        }

        void Bar(KRef<ITEM> ref)
        {
            . . .;
        }

    You can avoid the extra interlocked operations simply by passing the KRef
    as a C++ reference:

        void Bar(KRef<ITEM> &ref)

    You should usually only assign or copy a KRef in places where you would have
    written an InterlockedIncrement in the equivalent manual reference-counting
    code.

--*/

#pragma once

#include "KMacros.h"
#include "KDeletePolicy.h"
#include "KNew.h"
#include <cstddef>
#include <wil\wistd_type_traits.h>

class ndisTriageClass;
extern "C" VOID ndisInitGlobalTriageBlock();

template<typename T>
class KRTL_CLASS KRef
{
    class KRTL_CLASS KRefHolder :
        public KALLOCATOR<T::AllocationTag, T::AllocationArena>,
        public NdisDebugBlock<T::AllocationTag>
    {
    public:

        template<typename... Args>
        PAGED KRefHolder(Args&&... args) noexcept : _t(wistd::forward<Args>(args)...), RefCount(1) { }

        PAGED ~KRefHolder() { ASSERT_VALID(); ASSERT(RefCount == 0); }

        PAGED void AddRef()
        {
            ASSERT_VALID();
            ASSERT(RefCount > 0);
            InterlockedIncrement((PLONG)&RefCount);
        }

        PAGED void Release()
        {
            ASSERT_VALID();
            if (0 == InterlockedDecrement((PLONG)&RefCount))
            {
                delete this;
            }
        }

        PAGED T &operator*() { ASSERT_VALID(); return _t; }
        PAGED T const &operator*() const { ASSERT_VALID(); return _t; }

        PAGED static KRefHolder *from_pointer(T *t)
        {
            KRefHolder *This = CONTAINING_RECORD(t, KRefHolder, _t);
            This->ASSERT_VALID();
            return This;
        }

        friend class ::ndisTriageClass;
        friend VOID ::ndisInitGlobalTriageBlock();

    private:

        T _t;
        ULONG RefCount;
    };

    typedef KRefHolder * pointer;

public:

    PAGED KRef() noexcept : _p(pointer()) { }
    PAGED KRef(nullptr_t) noexcept : _p(pointer()) { }

    PAGED KRef(KRef<T> const &rhs) noexcept : _p(rhs._p) { ref(); }
    PAGED KRef<T> &operator=(KRef<T> &rhs)
    {
        if (this != &rhs)
            reset(rhs._p);
        return *this;
    }

    PAGED KRef(KRef<T> &&rhs) noexcept : _p(rhs.steal()) { }
    PAGED KRef<T> &operator=(KRef<T> &&rhs)
    {
        if (this != &rhs)
            take(rhs.steal());
        return *this;
    }

    PAGED ~KRef() { unref(); }

    PAGED bool operator==(KRef<T> const &rhs) const
    {
        return _p == rhs._p;
    }

    template<typename... Args>
    PAGED bool allocate(Args&&... args)
    {
        return allocate_inner(new (std::nothrow) KRefHolder(wistd::forward<Args>(args)...));
    }

// Several inline routines are in a separate file for better debugging
#include "KRefInline.h"
    PAGED operator bool() const { return _p != pointer(); }

    PAGED void clear()
    {
        unref();
        _p = pointer();
    }

    PAGED void assign_unsafe(T *t)
    {
        reset(KRefHolder::from_pointer(t));
    }

private:

    PAGED void ref()
    {
        if (_p != pointer())
        {
            _p->AddRef();
        }
    }

    PAGED void unref()
    {
        if (_p != pointer())
        {
            _p->Release();
        }
    }

    PAGED void reset(pointer p = pointer())
    {
        if (p != _p)
        {
            unref();
            _p = p;
            ref();
        }
    }

    PAGED void take(pointer p)
    {
        clear();
        _p = p;
    }

    PAGED pointer steal()
    {
        pointer p = _p;
        _p = pointer();
        return p;
    }

    PAGED bool allocate_inner(pointer p)
    {
        reset();

        if (!p)
            return false;

        take(p);
        return true;
    }

    friend class ::ndisTriageClass;
    friend VOID ::ndisInitGlobalTriageBlock();

    KRef(T &) { static_assert(false, "Cannot construct from a pre-existing object; use allocate() method instead"); }
    KRef(T const &) { static_assert(false, "Cannot construct from a pre-existing object; use allocate() method instead"); }
    T & operator=(const T&) { static_assert(false, "Cannot assign from a pre-existing object; use allocate() method instead"); }

    pointer _p;
};

