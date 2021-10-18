// Copyright (C) Microsoft Corporation. All rights reserved.
#include "precompiled.hpp"

#include "NxPowerEntry.hpp"

#include "NxPowerEntry.tmh"

_Use_decl_annotations_
NxPowerEntry::NxPowerEntry(
    NETADAPTER Adapter
)
    : m_adapter(Adapter)
{
}

NETADAPTER
NxPowerEntry::GetAdapter(
    void
) const
{
    return m_adapter;
}

_Use_decl_annotations_
void
NxPowerEntry::SetEnabled(
    bool Enabled
)
{
    m_enabled = Enabled;
}

bool
NxPowerEntry::IsEnabled(
    void
) const
{
    return m_enabled;
}
