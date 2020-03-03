// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KArray.h>

#include <preview/netadapteroffload.h>

enum class OffloadType {
    Checksum,
    Lso,
    Rsc
};

template <typename P>
using NxOffloadCallBack = void (*)(NETADAPTER, NETOFFLOAD);

class NxAdapter;

class NxOffloadBase : public NxNonpagedAllocation<'fOxN'>
{
private:
    OffloadType m_OffloadType;

public:
    NxOffloadBase(
        _In_ OffloadType OffloadType
    );

    virtual
    ~NxOffloadBase(
        void
    ) = default;

    OffloadType
    GetOffloadType(
        void
    ) const;

    virtual
    bool
    HasHardwareSupport(
        void
    ) const = 0;
};

template <typename T>
class NxOffload : public NxOffloadBase
{
private:

    T m_HardwareCapabilities;

    NxOffloadCallBack<T> m_NxOffloadCallback = nullptr;

    bool m_HardwareSupport = false;

public:

    NxOffload(
        _In_ OffloadType OffloadType,
        _In_ T const & DefaultHardwareCapabilities
    );

    void
    SetHardwareCapabilities(
        _In_ T const * HardwareCapabilities,
        _In_ NxOffloadCallBack<T> OffloadCallback
    );

    NxOffloadCallBack<T>
    GetOffloadCallback(
        void
        ) const;

    T const *
    GetHardwareCapabilities(
        void
        ) const;

    bool
    HasHardwareSupport(
        void
        ) const override;
};

class INxOffloadFacade : public NxNonpagedAllocation<'fOxN'>
{
public:

    virtual
    NTSTATUS
    RegisterPacketExtension(
        _In_ OffloadType OffloadType
        ) = 0;

    virtual
    NETADAPTER
    GetNetAdapter(
        void
    ) const = 0;

    virtual
    NTSTATUS
    QueryStandardizedKeywordSetting(
        _In_ PCUNICODE_STRING ValueName,
        _Out_ PULONG Value
        ) = 0;
};

class NxOffloadFacade : public INxOffloadFacade
{

private:
    NxAdapter& m_NxAdapter;

public:
    NxOffloadFacade(
        _In_ NxAdapter& nxAdapter
    );

    NTSTATUS
    RegisterPacketExtension(
        _In_ OffloadType OffloadType
    );

    NETADAPTER
    GetNetAdapter(
        void
        ) const;

    NTSTATUS
    QueryStandardizedKeywordSetting(
        _In_ PCUNICODE_STRING ValueName,
        _Out_ PULONG Value
    );
};

class NxOffloadManager : public NxNonpagedAllocation<'fOxN'>
{
private:
    Rtl::KArray<wistd::unique_ptr<NxOffloadBase>, NonPagedPoolNx> m_NxOffloads;

    wistd::unique_ptr<INxOffloadFacade> m_NxOffloadFacade;

    template <typename T>
    void
    SetHardwareCapabilities(
        _In_ T const * HardwareCapabilities,
        _In_ OffloadType OffloadType,
        _In_ NxOffloadCallBack<T> OffloadCallback
    );

    template <typename T>
    T const *
    GetHardwareCapabilities(
        _In_ OffloadType OffloadType
    ) const;

    template <typename T>
    void
    SetActiveCapabilities(
        _In_ T const & ActiveCapabilities,
        _In_ OffloadType OffloadType
    );

    NxOffloadBase *
    FindNxOffload(
        _In_ OffloadType OffloadType
    ) const;

    bool
    IsKeywordSettingDisabled(
        _In_ NDIS_STRING Keyword
    ) const;

public:
    NxOffloadManager(
        _In_ INxOffloadFacade *NxOffloadFacade
    );

    NTSTATUS
    Initialize(
        void
    );

    void
    SetChecksumHardwareCapabilities(
        _In_ NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES const * HardwareCapabilities
    );

    void
    GetChecksumHardwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * HardwareCapabilities
    ) const;

    void
    GetChecksumDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * DefaultCapabilities
    ) const;

    void
    SetChecksumActiveCapabilities(
        _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const * ActiveCapabilities
    );

    void
    SetLsoHardwareCapabilities(
        _In_ NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES const * HardwareCapabilities
    );

    void
    GetLsoHardwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_LSO_CAPABILITIES * HardwareCapabilities
    ) const;

    void
    GetLsoDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_LSO_CAPABILITIES * DefaultCapabilities
    ) const;

    void
    SetLsoActiveCapabilities(
        _In_ NET_CLIENT_OFFLOAD_LSO_CAPABILITIES const * ActiveCapabilities
    );

    NTSTATUS
    RegisterPacketExtensions(
        void
    );
};

