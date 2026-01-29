// Copyright (C) Microsoft Corporation. All rights reserved.
#include "Nx.hpp"

#include <netiodef.h>
#include <WudfPktmon.h>

EXTERN_C_START

#define DEFINE_MS_NPI_MODULEID(_offset, _name) \
    extern const NPI_MODULEID DECLSPEC_SELECTANY _name = { \
        sizeof(NPI_MODULEID), \
        MIT_GUID, \
        { \
            0xeb004a00 + _offset, \
            0x9b1a, 0x11d4, {0x91, 0x23, 0x00, 0x50, 0x04, 0x77, 0x59, 0xbc}}};

DEFINE_MS_NPI_MODULEID(51, NPI_MS_NDIS_TRANSLATOR_MODULEID)

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
PktMonClientInitializeNoSync(
    _In_ PNPI_MODULEID ModuleId,
    _In_ PKTMON_CLIENT_COMP_ENUM_HANDLER EnumComponents,
    _In_ PKTMON_CLIENT_CLEANUP_HANDLER CleanupComponents
)
{
    UNREFERENCED_PARAMETER((ModuleId, EnumComponents, CleanupComponents));
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
PktMonClientUninitialize()
{
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
PktMonClientComponentRegister(
    _Inout_ PKTMON_COMPONENT_CONTEXT *CompContext,
    _In_ PUNICODE_STRING Name,
    _In_ PUNICODE_STRING Description,
    _In_ PKTMON_COMPONENT_TYPE Type,
    _In_ NDIS_MEDIUM MediaType,
    _In_ PKTMON_DIRECTION_TAG DirTagIn,
    _In_ PKTMON_DIRECTION_TAG DirTagOut
)
{
    UNREFERENCED_PARAMETER((CompContext, Name, Description, Type, MediaType, DirTagIn, DirTagOut));
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
PktMonClientComponentUnregister(
    _Inout_ PKTMON_COMPONENT_CONTEXT *CompContext
)
{
    UNREFERENCED_PARAMETER(CompContext);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
PktMonClientSetCompProperty(
    _In_ PKTMON_COMPONENT_CONTEXT *CompContext,
    _In_ PKTMON_COMP_PROPERTY_ID Id,
    _In_ PVOID Value,
    _In_ USHORT Size
)
{
    UNREFERENCED_PARAMETER((CompContext, Id, Value, Size));
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
PktMonClientAddEdge(
    _In_  PKTMON_COMPONENT_CONTEXT *CompContext,
    _In_  PUNICODE_STRING Name,
    _In_  PKTMON_DIRECTION_TAG DirTagIn,
    _In_  PKTMON_DIRECTION_TAG DirTagOut,
    _In_  NDIS_MEDIUM MediaType,
    _Out_ PKTMON_EDGE_CONTEXT *EdgeContext
)
{
    UNREFERENCED_PARAMETER((CompContext, Name, DirTagIn, DirTagOut, MediaType, EdgeContext));
    return STATUS_SUCCESS;
}

EXTERN_C_END
