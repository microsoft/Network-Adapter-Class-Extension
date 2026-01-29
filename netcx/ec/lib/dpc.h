// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef _KERNEL_MODE
#include <wil/result.h>
#endif

enum class DpcImportance
{
#ifdef _KERNEL_MODE
    Low = _KDPC_IMPORTANCE::LowImportance,
    Medium = _KDPC_IMPORTANCE::MediumImportance,
    MediumHigh = _KDPC_IMPORTANCE::MediumHighImportance,
    High = _KDPC_IMPORTANCE::HighImportance,
#else
    Low,
    Medium,
    MediumHigh,
    High,
#endif
};

template <typename T, void (T::*Method)(void)>
class Dpc
{
public:

    _IRQL_requires_(DISPATCH_LEVEL)
    static
    void
    DpcThunk(
#ifdef _KERNEL_MODE
        _In_ KDPC * KDpc,
        _In_opt_ void * Context,
        _In_opt_ void * SystemArgument1,
        _In_opt_ void * SystemArgument2
#else
        _Inout_ PTP_CALLBACK_INSTANCE Instance,
        _Inout_opt_ void * Context,
        _Inout_ PTP_WORK Work
#endif
    )
    {
#ifdef _KERNEL_MODE
        UNREFERENCED_PARAMETER((KDpc, SystemArgument1, SystemArgument2));
#else
        UNREFERENCED_PARAMETER((Instance, Work));
#endif

        auto dpc = static_cast<Dpc *>(Context);
        dpc->OnDpc();
    }

    _IRQL_requires_(DISPATCH_LEVEL)
    void
    OnDpc(
        void
    )
    {
        (m_context.*Method)();
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    Dpc(
        _In_ T & Context
    )
        : m_context(Context)
    {
#ifdef _KERNEL_MODE
        KeInitializeDpc(
            &m_dpc,
            Dpc::DpcThunk,
            this);
#else
    m_mockDpc = CreateThreadpoolWork(
        Dpc::DpcThunk,
        this,
        nullptr);

    FAIL_FAST_IF_MSG(m_mockDpc == nullptr, "Failed to create threadpool work to mock DPC");
#endif
    }

#ifndef _KERNEL_MODE
    ~Dpc(
        void
    )
    {
        if (m_mockDpc != nullptr)
        {
            CloseThreadpoolWork(m_mockDpc);
        }
    }
#endif

    // Queues a new DPC
    _IRQL_requires_max_(HIGH_LEVEL)
    bool
    InsertQueueDpc(
        void
    )
    {
#ifdef _KERNEL_MODE
        return KeInsertQueueDpc(&m_dpc, NULL, NULL);
#else
        SubmitThreadpoolWork(m_mockDpc);
        return true;
#endif
    }

    _IRQL_requires_max_(HIGH_LEVEL)
    NTSTATUS
    SetTargetProcessorDpcEx(
        _In_ const PPROCESSOR_NUMBER ProcessorNumber
    )
    {
#ifdef _KERNEL_MODE
        return KeSetTargetProcessorDpcEx(&m_dpc, ProcessorNumber);
#else
        UNREFERENCED_PARAMETER(ProcessorNumber);
        return STATUS_UNSUCCESSFUL;
#endif
    }

    _IRQL_requires_max_(HIGH_LEVEL)
    void
    SetImportance(
        _In_ DpcImportance Importance
    )
    {
#ifdef _KERNEL_MODE
        KeSetImportanceDpc(
            &m_dpc,
            static_cast<KDPC_IMPORTANCE>(Importance));
#else
        UNREFERENCED_PARAMETER(Importance);
#endif
    }

private:

#ifdef _KERNEL_MODE
    KDPC
        m_dpc{};
#else
    PTP_WORK
        m_mockDpc{};
#endif

    T &
        m_context;
};
