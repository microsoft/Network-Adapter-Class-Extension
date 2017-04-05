/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name: 

    fxobject.h

Abstract:

    This module contains a base templated class for creating C++ objects 
    within a context of a WDFOBJECT.

Environment:

    kernel-mode only

Revision History:

--*/

#pragma once

//
// Base class for objects that reside within the context of a WDF object.
//

template<typename FX, 
         class    T,
         T* (GETCONTEXT)(__in FX FxObject),
         bool OVERRIDECLEANUP>
class CFxObject
{

private:

    const FX m_FxObject;

    bool m_Constructed;

    //
    // We do not want to duplicate an object of this class by the asignment operator
    // or the default copy constructor. Defining them in private scope indicates that 
    // this class object cannot be assigned/ copied. 
    //
    CFxObject& operator= (const CFxObject&);
    CFxObject (const CFxObject&);

protected:

    CFxObject(FX FxObject) : m_FxObject(FxObject),
                             m_Constructed(true)
    {
        return;
    }

    virtual ~CFxObject() {return;}

    virtual
    VOID
    OnCleanup(
        VOID
        )
    {
        return;
    }

public: 
    static T* _FromFxBaseObject(WDFOBJECT FxObject) 
    {
        return GETCONTEXT((FX) FxObject);
    }

    static T* _FromFxObject(FX FxObject) 
    {
        return GETCONTEXT(FxObject);
    }

    FX 
    FORCEINLINE
    GetFxObject(
        VOID
        ) 
    { 
        return m_FxObject; 
    }

    static
    VOID
    _SetObjectAttributes(
        __inout WDF_OBJECT_ATTRIBUTES *Attributes
        )
    {
        #pragma warning(suppress:4127) //Conditional Expression is constant
        if (OVERRIDECLEANUP) 
        {
            #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
            Attributes->EvtCleanupCallback = _OnCleanup;
        }

        #pragma prefast(suppress:__WARNING_FUNCTION_CLASS_MISMATCH_NONE, "can't add class to static member function")
        Attributes->EvtDestroyCallback = _OnDestroy;

    }

    bool
    IsConstructed(
        VOID
        )
    {
        //
        // NOTE: this depends on the object being zeroed out before 
        // construction
        //

        return m_Constructed;
    }

    static
    PVOID
    __cdecl
    operator new(
        size_t /* SizeCb */,
        PVOID  Memory
        )
    {
        return Memory;
    }

protected:

    static
    VOID
    __cdecl
    operator delete(
        PVOID /* Memory */
        )
    {
        return;
    }

    static
    __drv_functionClass(EVT_WDF_OBJECT_CONTEXT_CLEANUP)
    __drv_sameIRQL
    __drv_maxIRQL(DISPATCH_LEVEL)
    VOID
    _OnCleanup(
        __in WDFOBJECT Object
        )
    {
        CFxObject* o = _FromFxBaseObject(Object);
        if (o->m_FxObject != NULL)
        {
            NT_ASSERT(o->m_FxObject == Object);
            o->OnCleanup();
        }
    }

    //
    // This Ensures the destructor of the class is called
    // when this object is destroyed.
    //
    static
    __drv_functionClass(EVT_WDF_OBJECT_CONTEXT_DESTROY)
    __drv_sameIRQL
    __drv_maxIRQL(DISPATCH_LEVEL)
    VOID
    _OnDestroy(
        __in WDFOBJECT Object
        )
    {
        T* t = _FromFxBaseObject(Object);
        if (t->m_FxObject != NULL)
        {
            NT_ASSERT(t->m_FxObject == Object);
            delete t;
        }
    }
};

template<typename FX, 
         class    T,
         T* (GETCONTEXT)(__in FX FxObject),
         bool OVERRIDECLEANUP>
class CFxCancelableObject : public CFxObject<FX, T, GETCONTEXT, OVERRIDECLEANUP>
{

protected:

    CFxCancelableObject(FX FxObject) : 
        CFxObject(FxObject),
        m_Canceled(FALSE),
        m_EvtCancel(NULL)
    {
        KeInitializeSpinLock(&m_Lock);
        return;
    }

private: 
    BOOLEAN m_Canceled;

    KSPIN_LOCK m_Lock;

    PFN_NET_OBJECT_CANCEL m_EvtCancel;

public:

    VOID
    Cancel(
        VOID
        )
    /*++
    Routine Description:
        This method is called to cancel a CFxCancelableObject Object.
        If a completion routine had been setup for this object
        the completion routine is called.     
    --*/
    {
        KIRQL irql;

        PFN_NET_OBJECT_CANCEL evtCancel;

        KeAcquireSpinLock(&m_Lock, &irql);

        NT_ASSERT(m_Canceled == FALSE);

        m_Canceled = TRUE;
        evtCancel = m_EvtCancel;
        m_EvtCancel = NULL;

        KeReleaseSpinLock(&m_Lock, irql);

        if (evtCancel) {
            evtCancel((WDFOBJECT)GetFxObject());
        }
    }

    NTSTATUS
    MarkCancelable(
        PFN_NET_OBJECT_CANCEL EvtCancel
        )
    /*++
    Routine Description:
        This method sets a cancel routine for a CFxCancelableObject Object.
     
    Arguments: 
        EvtCancel - Pointer to the cancel routine.
     
    Return Value: 
        STATUS_SUCCESS if the cancel routine has been set.
        STATUS_CANCELLED if the CFxCancelableObject has already been cancelled
    --*/
    {
        KIRQL irql;
        NTSTATUS status;

        KeAcquireSpinLock(&m_Lock, &irql);
        if (m_Canceled) {
            status = STATUS_CANCELLED;
        } else {
            status = STATUS_SUCCESS;
            m_EvtCancel = EvtCancel;
        }
        KeReleaseSpinLock(&m_Lock, irql);

        return status;
    }

    NTSTATUS
    UnmarkCancelable(
        VOID
        )
    /*++
    Routine Description:
        This method clears a previously set  sets a cancel routine
        for a CFxCancelableObject Object.
    --*/
    {
        KIRQL irql;
        NTSTATUS status;

        KeAcquireSpinLock(&m_Lock, &irql);
        if (m_Canceled) {
            status = STATUS_CANCELLED;
        } else {
            m_EvtCancel = NULL;
            status = STATUS_SUCCESS;
        }
        KeReleaseSpinLock(&m_Lock, irql);

        return status;
    }
};


class CFxObjectDeleter
{
public:
    template<typename TFxObject>
    void operator()(TFxObject * object)
    {
        WdfObjectDelete(object->GetFxObject());
    }
};

#define CFX_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(_attributes, _contextType) MACRO_START \
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE((_attributes), _contextType); \
        _contextType::_SetObjectAttributes((_attributes)); \
    MACRO_END

