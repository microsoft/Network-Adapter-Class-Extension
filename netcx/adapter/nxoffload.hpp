// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KArray.h>

#include <netadapteroffload.h>
#include <preview/netadapteroffload.h>

enum class OffloadType {
    Checksum,
    Rsc,
    TxChecksum,
    RxChecksum,
    Gso
};

typedef struct _NET_OFFLOAD_TX_CHECKSUM_CAPABILITIES
{
    NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        IPv4Capabilities;

    NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        TcpCapabilities;

    NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES
        UdpCapabilities;

} NET_OFFLOAD_TX_CHECKSUM_CAPABILITIES;

typedef struct _NET_OFFLOAD_GSO_CAPABILITIES
{
    NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES
        LsoCapabilities;

    NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES
        UsoCapabilities;

} NET_OFFLOAD_GSO_CAPABILITIES;

template <typename P>
using NxOffloadCallBack = void (*)(NETADAPTER, NETOFFLOAD);

class NxAdapter;

class NxOffloadBase
    : public NxNonpagedAllocation<'fOxN'>
{
private:
    OffloadType
        m_offloadType;

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
class NxOffload
    : public NxOffloadBase
{
private:

    T
        m_hardwareCapabilities;

    NxOffloadCallBack<T>
        m_offloadCallback = nullptr;

    bool
        m_hardwareSupport = false;

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

    void
    SetOffloadCallback(
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

class INxOffloadFacade
    : public NxNonpagedAllocation<'fOxN'>
{
public:

    virtual
    NTSTATUS
    RegisterExtension(
        _In_ OffloadType OffloadType,
        _In_ NxOffloadBase const * Offload
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

class NxOffloadFacade
    : public INxOffloadFacade
{

private:
    NxAdapter &
        m_adapter;

public:
    NxOffloadFacade(
        _In_ NxAdapter& nxAdapter
    );

    NTSTATUS
    RegisterExtension(
        _In_ OffloadType OffloadType,
        _In_ NxOffloadBase const * Offload
    );

    NETADAPTER
    GetNetAdapter(
        void
        ) const;

    // Queries standardized keyword setting from the device scope.
    NTSTATUS
    QueryStandardizedKeywordSetting(
        _In_ PCUNICODE_STRING ValueName,
        _Out_ PULONG Value
    );
};

class NxOffloadManager
    : public NxNonpagedAllocation<'fOxN'>
{
private:
    Rtl::KArray<wistd::unique_ptr<NxOffloadBase>, NonPagedPoolNx>
        m_offloads;

    wistd::unique_ptr<INxOffloadFacade>
        m_offloadFacade;

    bool
        m_isChecksumRegistered = false;

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

    template <typename T>
    void
    SetOffloadCallback(
        _In_ OffloadType OffloadType,
        _In_ NxOffloadCallBack<T> OffloadCallback
    );

    NxOffloadBase *
    FindNxOffload(
        _In_ OffloadType OffloadType
    ) const;

    bool
    IsKeywordSettingDisabled(
        _In_ NDIS_STRING Keyword
    ) const;

    bool
    IsKeywordPresent(
        _In_ UNICODE_STRING Keyword
    ) const;

    ULONG
    GetStandardizedKeywordSetting(
        _In_ NDIS_STRING Keyword,
        _In_ ULONG DefaultValue
    ) const;

public:
    NxOffloadManager(
        _In_ INxOffloadFacade * NxOffloadFacade
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
    SetTxChecksumHardwareCapabilities(
        _In_ NET_ADAPTER_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * HardwareCapabilities
    );

    void
    GetTxChecksumHardwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * HardwareCapabilities
    ) const;

    void
    GetTxChecksumSoftwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * SoftwareCapabilities
    ) const;

    void
    GetTxIPv4ChecksumDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
    ) const;

    void
    GetTxTcpChecksumDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
    ) const;

    void
    GetTxUdpChecksumDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES * DefaultCapabilities
    ) const;

    void
    SetTxChecksumActiveCapabilities(
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveIPv4Capabilities,
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveTcpCapabilities,
        _In_ NET_CLIENT_OFFLOAD_TX_CHECKSUM_CAPABILITIES const * ActiveUdpCapabilities
    );

    bool
    IsTxChecksumSoftwareFallbackEnabled(
        void
    ) const;

    void
    SetRxChecksumCapabilities(
        _In_ NET_ADAPTER_OFFLOAD_RX_CHECKSUM_CAPABILITIES const * Capabilities
    );

    void
    GetRxChecksumHardwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * HardwareCapabilities
    ) const;

    void
    GetRxChecksumDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES * DefaultCapabilities
    ) const;

    void
    SetRxChecksumActiveCapabilities(
        _In_ NET_CLIENT_OFFLOAD_CHECKSUM_CAPABILITIES const * ActiveCapabilities
    );

    void
    SetGsoHardwareCapabilities(
        _In_ NET_ADAPTER_OFFLOAD_GSO_CAPABILITIES const * HardwareCapabilities
    );

    void
    GetGsoHardwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * HardwareCapabilities
    ) const;

    void
    GetGsoSoftwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * SoftwareCapabilities
    ) const;

    void
    SetGsoActiveCapabilities(
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const * LsoActiveCapabilities,
        _In_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES const * UsoActiveCapabilities
    );

    void
    GetLsoDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * DefaultCapabilities
    ) const;

    void
    GetUsoDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_GSO_CAPABILITIES * DefaultCapabilities
    ) const;

    bool
    IsLsoSoftwareFallbackEnabled(
        void
    ) const;

    bool
    IsUsoSoftwareFallbackEnabled(
        void
    ) const;

    NTSTATUS
    RegisterExtensions(
        void
    );

    void
    SetRscHardwareCapabilities(
        _In_ NET_ADAPTER_OFFLOAD_RSC_CAPABILITIES const * HardwareCapabilities
    );

    void
    GetRscHardwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * HardwareCapabilities
    ) const;

    void
    GetRscSoftwareCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * SoftwareCapabilities
    ) const;

    void
    GetRscDefaultCapabilities(
        _Out_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES * DefaultCapabilities
    ) const;

    void
    SetRscActiveCapabilities(
        _In_ NET_CLIENT_OFFLOAD_RSC_CAPABILITIES const * ActiveCapabilities
    );

    bool
    IsRscSoftwareFallbackEnabled(
        void
    ) const;
};

