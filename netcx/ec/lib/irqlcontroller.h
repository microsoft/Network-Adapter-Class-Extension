// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

class IrqlController
{
public:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    explicit
    IrqlController(
        _In_ bool RaiseToDispatch
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
    bool
    WasRaised(
        void
    ) const;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    void
    Lower(
        void
    ) const;

private:

    KIRQL
        m_prevIrql {};

    bool const
        m_raiseToDispatchRequested;
};
