// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"
#include "IrqlController.h"

_Use_decl_annotations_
IrqlController::IrqlController(
    ExecutionContext::RunContext Context,
    bool RunWorkerThreadAtDispatch
)
    : m_raiseToDispatchRequested(
        RunWorkerThreadAtDispatch &&
        Context == ExecutionContext::RunContext::WorkerThread)
{
    switch (Context)
    {
        case ExecutionContext::RunContext::ArbitraryContextAtDispatch:

            [[fallthrough]];

        case ExecutionContext::RunContext::NotifyDpc:

            [[fallthrough]];

        case ExecutionContext::RunContext::WorkerDpc:

            m_originalIrql = DISPATCH_LEVEL;
            break;

        case ExecutionContext::RunContext::ArbitraryContextAtPassive:

            [[fallthrough]];

        case ExecutionContext::RunContext::WorkerThread:

            m_originalIrql = PASSIVE_LEVEL;
            break;

        case ExecutionContext::RunContext::ArbitraryContextAtApc:

            m_originalIrql = APC_LEVEL;
            break;

        default:

            NT_FRE_ASSERTMSG(
                "Invalid RunContext passed to IrqlController",
                false);

            break;
    }

    if (m_originalIrql < DISPATCH_LEVEL && m_raiseToDispatchRequested)
    {
        auto const previousIrql = KeRaiseIrqlToDpcLevel();
        NT_FRE_ASSERT(previousIrql == m_originalIrql);
        m_effectiveIrql = DISPATCH_LEVEL;
    }
    else
    {
        m_effectiveIrql = m_originalIrql;
    }
}

_Use_decl_annotations_
IrqlController::~IrqlController(
    void
)
{
    if (m_effectiveIrql != m_originalIrql)
    {
        KeLowerIrql(m_originalIrql);
    }
}

_Use_decl_annotations_
KIRQL
IrqlController::GetIrql(
    void
) const
{
    return m_effectiveIrql;
}
