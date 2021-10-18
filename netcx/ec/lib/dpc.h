// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

template <typename ContextType>
class EcDpc
{
public:

    using DpcRoutine = void (*)(ContextType*);

    _IRQL_requires_(PASSIVE_LEVEL)
    EcDpc(
        _In_ DpcRoutine ExternalDpcRoutine,
        _In_ ContextType * Context
    )
        : m_externalDpcRoutine(ExternalDpcRoutine)
        , m_context(Context)
    {
#ifdef _KERNEL_MODE
        KeInitializeDpc(
            &m_dpc,
            EcDpc::InternalDpcRoutine,
            this);
#else
    m_mockDpc = CreateThreadpoolWork(
        EcDpc::InternalDpcRoutine,
        this,
        nullptr);

    FAIL_FAST_IF_MSG(m_mockDpc == nullptr, "Failed to create threadpool work to mock DPC");
#endif
    }

#ifndef _KERNEL_MODE
    ~EcDpc(
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

private:

#ifdef _KERNEL_MODE
    static
    void
    InternalDpcRoutine(
        PKDPC Dpc,
        PVOID DeferredContext,
        PVOID SystemArgument1,
        PVOID SystemArgument2
    )
    {
        UNREFERENCED_PARAMETER(Dpc);
        UNREFERENCED_PARAMETER(SystemArgument1);
        UNREFERENCED_PARAMETER(SystemArgument2);

        auto internalContext = static_cast<EcDpc *>(DeferredContext);
        internalContext->m_externalDpcRoutine(internalContext->m_context);
    }

    KDPC
        m_dpc{};
#else
    static
    void
    InternalDpcRoutine(
        _Inout_ PTP_CALLBACK_INSTANCE Instance,
        _Inout_opt_ void * Context,
        _Inout_ PTP_WORK Work
    )
    {
        UNREFERENCED_PARAMETER((Instance, Work));
        auto internalContext = static_cast<EcDpc *>(Context);
        internalContext->m_externalDpcRoutine(internalContext->m_context);
    }

    PTP_WORK
        m_mockDpc{};
#endif

    DpcRoutine const
        m_externalDpcRoutine;

    ContextType * const
        m_context;
};
