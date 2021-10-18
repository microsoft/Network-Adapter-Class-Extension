// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <kmacros.h>
#include <krundown.h>

class PowerReferenceHolder
{
    friend class
        IdleStateMachine;

private:

    enum class Type
    {
        None,
        WDF,
        Rundown,
    };

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PowerReferenceHolder(
        _In_ KRundown & Rundown
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PowerReferenceHolder(
        _In_ WDFDEVICE Device,
        _In_ void const * Tag,
        _In_ NTSTATUS NtStatus
    );

public:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PowerReferenceHolder(
        void
    ) = default;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PowerReferenceHolder(
        PowerReferenceHolder const &
    ) = delete;

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PowerReferenceHolder(
        _Inout_ PowerReferenceHolder && Other
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PowerReferenceHolder &
    operator=(
        PowerReferenceHolder && Other
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    ~PowerReferenceHolder(
        void
    );

private:

    KRundown *
        m_rundown = nullptr;

    WDFDEVICE
        m_device = WDF_NO_HANDLE;

    Type
        m_type = Type::None;

    void const *
        m_tag = nullptr;

    NTSTATUS
        m_ntStatus = STATUS_UNSUCCESSFUL;
};
