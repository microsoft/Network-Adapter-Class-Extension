// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include <kptr.h>
#include "Dispatcher.hpp"
#include "DispatcherSubsystem.hpp"
#include "CpuKnobs.hpp"

#ifdef _KERNEL_MODE
#define GetActiveProcessorCount KeQueryActiveProcessorCountEx
#define GetProcessorIndexFromNumber(_processor) KeGetProcessorIndexFromNumber((_processor))
#else
inline
NTSTATUS
KeGetProcessorNumberFromIndex (
    _In_ ULONG ProcIndex,
    _Out_ PPROCESSOR_NUMBER ProcNumber
    )
{
    ProcNumber->Number = static_cast<UCHAR>(ProcIndex);
    ProcNumber->Group = 0;
    ProcNumber->Reserved = 0; 

    return STATUS_SUCCESS;
}

#define GetProcessorIndexFromNumber(_processor) (_processor)->Number
#endif

Dispatcher* s_DispatcherArray{};
CpuKnobs* s_CpuKnobsArray{};

size_t s_MaxDispatcherCore;

NTSTATUS
InitializeKnobs(int cpuCount)
{
    auto knobsArray = wil::make_unique_nothrow<CpuKnobs[]>(cpuCount);
    if (!knobsArray)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (int i = 0; i < cpuCount; ++i)
    {
        auto ntStatus = knobsArray[i].Initialize(i);
        if (ntStatus != STATUS_SUCCESS)
        {
            return ntStatus;
        }
    }

    s_CpuKnobsArray = knobsArray.release();
    return STATUS_SUCCESS;
}

NTSTATUS
InitializeDispatcherSubsystem()
{
    auto const cpuCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    KnobNamespace::InitializeSubsystem();
    ntStatus = InitializeKnobs(cpuCount);
    
    if (ntStatus != STATUS_SUCCESS)
    {
        return ntStatus;
    }

    size_t const allocationSize = cpuCount * sizeof(Dispatcher);

    auto dispatcherArray = static_cast<Dispatcher *>(ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        allocationSize,
        'AtpD'));
    if (!dispatcherArray)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    auto freeMem = wil::scope_exit([dispatcherArray]()
    {
        ExFreePool(dispatcherArray);
    });

    ULONG i;

    for (i = 0; i < cpuCount; i++)
    {
        PROCESSOR_NUMBER processor;
        ntStatus = KeGetProcessorNumberFromIndex(i, &processor);

        if (ntStatus != STATUS_SUCCESS)
        {
            break;
        }

        auto dispatcher = new (&dispatcherArray[i]) Dispatcher(processor, s_CpuKnobsArray[i]);
        ntStatus = dispatcher->Initialize();

        if (ntStatus != STATUS_SUCCESS)
        {
            break;
        }
    }

    if (ntStatus != STATUS_SUCCESS)
    {
        for (ULONG j = 0; j <= i; j++)
        {
            dispatcherArray[j].~Dispatcher();
        }

        return ntStatus;
    }

    s_MaxDispatcherCore = cpuCount;
    s_DispatcherArray = dispatcherArray;
    freeMem.release();

    return STATUS_SUCCESS;
}

void
CleanupDispatcherSubsystem()
{
    if (s_DispatcherArray)
    {
        for (ULONG i = 0; i < s_MaxDispatcherCore; i++)
        {
            s_DispatcherArray[i].~Dispatcher();
        }
        ExFreePool(s_DispatcherArray);
    }

    if (s_CpuKnobsArray)
    {
        delete[] s_CpuKnobsArray;
    }
}

_Use_decl_annotations_
Dispatcher &
GetProcessorDispatcher(
    PROCESSOR_NUMBER & Processor
)
{
    auto const cpuIndex = GetProcessorIndexFromNumber(&Processor);

    NT_FRE_ASSERT(cpuIndex < s_MaxDispatcherCore);
    return s_DispatcherArray[cpuIndex];
}
