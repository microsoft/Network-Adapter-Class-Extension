// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

This module contains a base templated class for creating C++ objects
within a context of a WDFOBJECT.

--*/

#pragma once

#include "FxObjectBase.hpp"

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

#define CFX_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(_attributes, _contextType) MACRO_START \
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE((_attributes), _contextType); \
        _contextType::_SetObjectAttributes((_attributes)); \
    MACRO_END

