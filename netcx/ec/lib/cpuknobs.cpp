// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#ifdef _KERNEL_MODE
#include <wdf.h>
#endif

#include "CpuKnobs.hpp"

#include <kmemoryaccess.h>

NTSTATUS
KnobsUpdateRoutine(
    _In_ void* routineContext,
    _In_ struct KnobDescriptor const* knob,
    _In_ ULONG64 newValue
)
{
    UNREFERENCED_PARAMETER((knob, newValue));
    
    auto cpuKnobs = reinterpret_cast<CpuKnobs*>(routineContext);
    cpuKnobs->MarkKnobsUpdated();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
CpuKnobs::Initialize(
    ULONG cpu
)
{
#ifdef _KERNEL_MODE
    PDRIVER_OBJECT driverObject = WdfDriverWdmGetDriverObject(WdfGetDriver());
#else
    PDRIVER_OBJECT driverObject = nullptr;
#endif

    CONFIG_KNOB_NAMESPACE knobNamespace {};
    knobNamespace.NamespaceType = ConfigKnobNamespaceCpuExecutionContext;
    knobNamespace.ObjectId.Data1 = cpu;

    m_knobNamespace.Initialize(driverObject, &knobNamespace);

    INITIALIZE_EXECUTION_CONTEXT_RUNTIME_KNOBS(&m_knobs);

    const auto KNOBS_TO_RESERVE = 20;
    const auto knobType = ConfigKnobFlag::Uint32Datatype | ConfigKnobFlag::AllowDynamicUpdate;
    if (!m_knobDescriptors.reserve(KNOBS_TO_RESERVE))
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    m_knobDescriptors.append({L"Flags", &m_knobs.Flags, 0x1, knobType});
    m_knobDescriptors.append({L"MaxTimeAtDispatch", &m_knobs.MaxTimeAtDispatch, 0, knobType});
    m_knobDescriptors.append({L"DispatchTimeWarning", &m_knobs.DispatchTimeWarning, 0, knobType});
    m_knobDescriptors.append({L"DispatchTimeWarningInterval", &m_knobs.DispatchTimeWarningInterval, 15 * 60 * 1000, knobType, 1 * 60 * 1000, 24 * 60 * 1000});
    m_knobDescriptors.append({L"DpcWatchdogTimerThreshold", &m_knobs.DpcWatchdogTimerThreshold, 80, knobType, 1, 80});
    m_knobDescriptors.append({L"WorkerThreadPriority", &m_knobs.WorkerThreadPriority, 10, knobType, 1, 15});

    // we want all memory to be allocated in one go, to avoid checking errors on each append; 
    // Extend KNOBS_TO_RESERVE if there are more knobs than the reserved space
    NT_FRE_ASSERT(m_knobDescriptors.count() < KNOBS_TO_RESERVE);

    return m_knobNamespace.AddKnobCollection(&m_knobDescriptors[0], m_knobDescriptors.count(), nullptr, KnobsUpdateRoutine, this);
}

void
CpuKnobs::CheckKnobsUpdated(void)
{
    InterlockedExchange(&m_knobsUpdated, 0);
}


void
CpuKnobs::MarkKnobsUpdated(void)
{
    InterlockedExchange(&m_knobsUpdated, 1);
}

bool
CpuKnobs::GetKnobsUpdated(void)
{
    return mem::ReadNoFence(&m_knobsUpdated);
};

UINT32
CpuKnobs::GetKnobInt32(KNOB KnobOffset)
{
    return mem::ReadNoFence((UINT32 *)((PUCHAR)&m_knobs + (int)KnobOffset));
}

bool
ndisIsDevicesHivePresent()
{
    // There should be no need for this function once Knobs library is refactored;
    // It shouldn't be used at all since EC.sys does not use NetSetup knobs
    NT_FRE_ASSERT(false);
    return false;
}
