// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include "IrqlController.h"

_Use_decl_annotations_
IrqlController::IrqlController(
    bool RaiseToDispatch
)
    : m_raiseToDispatchRequested(RaiseToDispatch)
{
#ifdef _KERNEL_MODE
    m_prevIrql = KeGetCurrentIrql();
    if (m_prevIrql < DISPATCH_LEVEL && m_raiseToDispatchRequested)
    {
        m_prevIrql = KeRaiseIrqlToDpcLevel();
    }
#else
    UNREFERENCED_PARAMETER(RaiseToDispatch);
#endif
}

_Use_decl_annotations_
IrqlController::~IrqlController(
    void
)
{
    Lower();
}

_Use_decl_annotations_
bool
IrqlController::WasRaised(
    void
) const
{
#ifdef _KERNEL_MODE
    return m_prevIrql < DISPATCH_LEVEL && m_raiseToDispatchRequested;
#else
    return false;
#endif
}

_Use_decl_annotations_
void
IrqlController::Lower(
    void
) const
{
#ifdef _KERNEL_MODE
    if (m_prevIrql < DISPATCH_LEVEL && m_raiseToDispatchRequested)
    {
        KeLowerIrql(m_prevIrql);
    }
#endif
}
