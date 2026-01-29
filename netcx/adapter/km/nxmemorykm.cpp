// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"
#include "NxMemory.hpp"
#include "NetMemorySharedTypes.h"
#include "NxMemoryKm.tmh"
#include "verifier.hpp"
#include "KMemoryAccess.h"

NxMemory::NxMemory(
    _In_ NX_PRIVATE_GLOBALS const* DriverGlobals
)
    : m_driverGlobals(DriverGlobals)
{
}

_Use_decl_annotations_
NTSTATUS
NxMemory::Initialize(
    const NET_MEMORY_COLLECTION_CONFIG* MemoryCollectionConfiguration
)
{
    UNREFERENCED_PARAMETER(MemoryCollectionConfiguration);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NxMemory::NetMemoryCreate(
    const NET_MEMORY_CONFIG * Configuration,
    NET_MEMORY_ID * MemoryId
)
{
    const size_t netMemoryMdlExceptMdl = RTL_SIZEOF_THROUGH_FIELD(NET_MEMORY_MDL_WRAPPER, OsOwnedReferenceCount);
    const size_t mdlSize = MmSizeOfMdl(Configuration->BufferAddress, Configuration->BufferLength);

    const size_t netMemoryMdlSize = RTL_NUM_ALIGN_UP(netMemoryMdlExceptMdl + mdlSize, sizeof(PVOID));
    KPoolPtrNP<NET_MEMORY_MDL_WRAPPER> netMemoryMdl = MakeSizedPoolPtrNP<NET_MEMORY_MDL_WRAPPER>('mndm', netMemoryMdlSize);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !netMemoryMdl);

    MmInitializeMdl(&netMemoryMdl->Mdl, Configuration->BufferAddress, Configuration->BufferLength);
    MmBuildMdlForNonPagedPool(&netMemoryMdl->Mdl);

    *MemoryId = reinterpret_cast<NET_MEMORY_ID>(netMemoryMdl.release());

    InterlockedIncrement64(&m_buffersOutstanding);
    return STATUS_SUCCESS;
}

NET_MEMORY_MDL_WRAPPER*
NxMemory::GetMdlForDriverAllocatedMemory(
    _In_ NET_MEMORY_ID MemoryId
)
{
    if (MemoryId == NET_MEMORY_ID_INVALID)
    {
        return nullptr;
    }
    return reinterpret_cast<NET_MEMORY_MDL_WRAPPER*>(MemoryId);
}

_Use_decl_annotations_
void
NxMemory::NetMemoryDestroy(
    NET_MEMORY_ID MemoryId
)
{
    NET_MEMORY_MDL_WRAPPER* mdlWrapper = reinterpret_cast<NET_MEMORY_MDL_WRAPPER*>(MemoryId);

    const bool validCall =
        mdlWrapper &&
        mdlWrapper->OsOwnedReferenceCount == 0 &&
        mem::ReadAcquire(&m_buffersOutstanding) > 0;

    if (validCall)
    {
        KPoolPtrNP<NET_MEMORY_MDL_WRAPPER> netMemoryMdl(mdlWrapper);
        InterlockedDecrement64(&m_buffersOutstanding);
    }
    else if (m_driverGlobals->CxVerifierOn)
    {
        Verifier_ReportViolation(
            m_driverGlobals,
            VerifierAction_DbgBreakIfDebuggerPresent,
            FailureCode_InvalidNetMemoryRelease,
            reinterpret_cast<ULONG_PTR>(mdlWrapper),
            static_cast<ULONG_PTR>(mdlWrapper ? mdlWrapper->OsOwnedReferenceCount : 0));
    }
}

NxMemory::~NxMemory(
)
{
    if (m_driverGlobals->CxVerifierOn && m_buffersOutstanding != 0)
    {
        Verifier_ReportViolation(
            m_driverGlobals,
            VerifierAction_DbgBreakIfDebuggerPresent,
            FailureCode_LeakedNetMemoryNotAllMemoryReleased,
            reinterpret_cast<ULONG_PTR>(this),
            static_cast<ULONG_PTR>(m_buffersOutstanding));
    }
}
