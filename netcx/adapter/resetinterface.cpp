// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"
#include "ResetInterface.hpp"
#include "ResetInterface.tmh"

_Use_decl_annotations_
ResetInterface::~ResetInterface(
    void
)
{
    if (m_interfaceDereferenceHandler != nullptr)
    {
        m_interfaceDereferenceHandler(m_resetInterfaceContext);
    }
}

_Use_decl_annotations_
bool
ResetInterface::GetSupportedDeviceResetType(
    ULONG * SupportedResetTypes
) const
{
    if (m_deviceResetSupported)
    {
        *SupportedResetTypes = m_supportedResetTypes;
    }

    return m_deviceResetSupported;
}

_Use_decl_annotations_
bool
ResetInterface::DeviceResetTypeSupported(
    _In_ DEVICE_RESET_TYPE ResetType
) const
{
    if (m_deviceResetSupported &&
        ((m_supportedResetTypes & (1 << ResetType)) != 0))
    {
        return true;
    }

    return false;
}

_Use_decl_annotations_
NTSTATUS
ResetInterface::DeviceReset(
    DEVICE_RESET_TYPE ResetType
) const
{
    switch (ResetType)
    {
        case PlatformLevelReset:
            return TriggerPldr();

        case FunctionLevelReset:
            return TriggerFldr();
    }

    return STATUS_INVALID_PARAMETER;
}

_Use_decl_annotations_
NTSTATUS
ResetInterface::TriggerPldr(
    void
) const
{
    return m_resetHandler(
        m_resetInterfaceContext,
        PlatformLevelDeviceReset,
        0,
        nullptr);
}
