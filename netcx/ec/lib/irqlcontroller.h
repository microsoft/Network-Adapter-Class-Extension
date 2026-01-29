// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "ExecutionContext.hpp"

class IrqlController
{
public:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    explicit
    IrqlController(
        _In_ ExecutionContext::RunContext Context,
        _In_ bool RunWorkerThreadAtDispatch
    );

    IrqlController(
        IrqlController const &
    ) = delete;

    IrqlController &
    operator=(
        IrqlController const &
    ) = delete;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ~IrqlController(
        void
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    KIRQL
    GetIrql(
        void
    ) const;

private:

    KIRQL
        m_originalIrql{};

    KIRQL
        m_effectiveIrql{};

    bool const
        m_raiseToDispatchRequested;
};
