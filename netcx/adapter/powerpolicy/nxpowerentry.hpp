// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

class NxPowerEntry : public KALLOCATOR<'EPxN'>
{
    friend class NxPowerPolicy;
    friend class NxPowerList;
    friend class NxPowerOffloadList;
    friend class NxWakeSourceList;

public:

    NxPowerEntry(
        _In_ NETADAPTER Adapter
    );

    virtual
    ~NxPowerEntry(
        void
    ) = default;

    NETADAPTER
    GetAdapter(
        void
    ) const;

    void
    SetEnabled(
        _In_ bool Enabled
    );

    bool
    IsEnabled(
        void
    ) const;

protected:

    NETADAPTER const
        m_adapter;

    bool
        m_enabled = false;

    SINGLE_LIST_ENTRY
        m_powerPolicyLinkage = {};

    SINGLE_LIST_ENTRY
        m_clientListLinkage = {};

};
