// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "Nx.hpp"

#include "NxExtension.tmh"
#include "NxExtension.hpp"

#include "NxMacros.hpp"

_Use_decl_annotations_
NTSTATUS
NxExtension::Initialize(
    NET_EXTENSION_PRIVATE const * Extension
)
{
    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        !WIN_VERIFY(RTL_IS_POWER_OF_TWO(Extension->NonWdfStyleAlignment)));

    auto const alignment = Extension->Type == NetExtensionTypePacket ?
        alignof(NET_PACKET) :
        alignof(NET_FRAGMENT);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        !WIN_VERIFY(Extension->NonWdfStyleAlignment <= alignment));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        !WIN_VERIFY(Extension->Size <= MAXIMUM_ALLOWED_EXTENSION_SIZE));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INVALID_PARAMETER,
        !WIN_VERIFY(Extension->Size != 0));

    auto const nameSize = sizeof(WCHAR) * (wcslen(Extension->Name) + 1);
    m_extensionName = MakeSizedPoolPtr<WCHAR>('xExN', nameSize);

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! m_extensionName);

    RtlCopyMemory(m_extensionName.get(), Extension->Name, nameSize);

    m_extension = *Extension;
    m_extension.AssignedOffset = 0;
    m_extension.Name = m_extensionName.get();

    return STATUS_SUCCESS;
}

