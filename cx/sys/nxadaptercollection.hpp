// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

class NxAdapterCollection
{
public:

    NxAdapterCollection();

    NTSTATUS
    Initialize(
        void
        );

    ULONG
    Count(
        void
        ) const;

    void
    Clear(
        void
        );

    NxAdapter *
    GetDefaultAdapter(
        void
        ) const;

    void
    AddAdapter(
        _In_ NxAdapter * Adapter
        );

    bool
    RemoveAdapter(
        _In_ NxAdapter * Adapter
        );

    template <typename lambda>
    NTSTATUS
    ForEachAdapterUnlocked(
        _In_ lambda f
        )
    {
        for (
            LIST_ENTRY *link = m_ListHead.Flink;
            link != &m_ListHead;
            link = link->Flink
            )
        {
            NxAdapter *adapter = CONTAINING_RECORD(link, NxAdapter, m_Linkage);

            NTSTATUS ntStatus = f(*adapter);
            if (!NT_SUCCESS(ntStatus))
                return ntStatus;
        }

        return STATUS_SUCCESS;
    }

    template <typename lambda>
    NTSTATUS
    ForEachAdapterLocked(
        _In_ lambda f
        )
    {
        auto lock = wil::acquire_wdf_wait_lock(m_ListLock);
        return ForEachAdapterUnlocked(f);
    }

private:

    _Guarded_by_(m_ListLock)
    LIST_ENTRY m_ListHead;

    _Guarded_by_(m_ListLock)
    ULONG m_Count = 0;

    WDFWAITLOCK m_ListLock = nullptr;
};
