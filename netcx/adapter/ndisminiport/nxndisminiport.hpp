// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <NxNew.hpp>
#include "netadaptercx_triage.h"

class BridgeToKernel;
class RdAdapter;
class NxAdapter;

using NxOrRdAdapter =
#ifdef NETCX_REDIRECTOR
    RdAdapter;
#else
    NxAdapter;
#endif

typedef struct _NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES;

class NxNdisMiniport
    : public NxNonpagedAllocation<'tNxN'>
{

protected:

    NxOrRdAdapter *
        m_adapter = nullptr;

    NDIS_HANDLE
        m_ndisAdapterHandle = nullptr;

#ifdef NETCX_UMDF
    BridgeToKernel *
        m_bridgeToKernel = nullptr;
#endif

public:

    NxNdisMiniport(
        _In_ NxOrRdAdapter *
    );

    virtual
    ~NxNdisMiniport(
        void
    ) = default;

#ifdef NETCX_UMDF
    void SetBridgeToKernel(
        _In_ BridgeToKernel * Bridge
    );
#endif

    //
    // Operations need to be implemented based on type of NDIS miniport
    //

    virtual
    NTSTATUS
    CreateNdisMiniport(
        void
    ) = 0;

    virtual
    NTSTATUS
    StartNdisMiniport(
        void
    ) = 0;

    virtual
    void
    StartNdisMiniportDataPath(
        void
    ) = 0;

    virtual
    void
    StopNdisMiniport(
        void
    ) = 0;

    virtual
    NTSTATUS
    RemoveNdisMiniport(
        void
    ) = 0;

    virtual
    NDIS_STATUS
    SetRegistrationAttributes(
        void
    ) = 0;

    //
    // Common operations for all types of NDIS miniport
    //

    NTSTATUS
    SetNative80211Attributes(
        _In_ NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES* Attributes
    );

    NDIS_STATUS
    SetGeneralAttributes(
        _Inout_ NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES & AdapterGeneral
    ) ;

    void
    CompleteMiniportPause(
        void
    );

    void
    CompleteMiniportRestart(
        void
    );

    void
    CompleteOidRequest(
        _In_ NDIS_OID_REQUEST * OidRequest,
        _In_ NDIS_STATUS Status
    );

    void
    CompleteDirectOidRequest(
        _In_ NDIS_OID_REQUEST * OidRequest,
        _In_ NDIS_STATUS Status
    );

    template<typename T>
    void
    IndicateMiniportStatusChange(
        _In_ NDIS_STATUS StatusCode,
        _In_ T const & Payload,
        _In_opt_ ULONG const * StatusBufferSizeOverride
    );

    void
    IndicateMiniportStatus(
        _In_ NDIS_STATUS_INDICATION * NdisStatusIndication
    );

    NDIS_HANDLE
    GetNdisHandle(
        void
    ) const;

    static
    void
    GetTriageInfo(
        void
    );
};

class NxNdisPnpMiniport
    : public NxNdisMiniport
{

protected:

    WDFDEVICE const
        m_device;

public:

    NxNdisPnpMiniport(
        _In_ NxOrRdAdapter *,
        _In_ WDFDEVICE Device
    );

    virtual
    ~NxNdisPnpMiniport(
        void
    ) = default;

    virtual
    NTSTATUS
    CreateNdisMiniport(
        void
    ) override;

    virtual
    NTSTATUS
    StartNdisMiniport(
        void
    ) override;

    virtual
    void
    StartNdisMiniportDataPath(
        void
    ) override;

    virtual
    void
    StopNdisMiniport(
        void
    ) override;

    virtual
    NTSTATUS
    RemoveNdisMiniport(
        void
    ) override;

    virtual
    NDIS_STATUS
    SetRegistrationAttributes(
        void
    ) override;
};


class NxNdisLwMiniport
    : public NxNdisMiniport
{
private:

    GUID
        m_interfaceGuid;

public:

    NxNdisLwMiniport(
        _In_ NxOrRdAdapter *,
        _In_ GUID InterfaceGuid
    );

    virtual
    ~NxNdisLwMiniport(
        void
    ) = default;

    virtual
    NTSTATUS
    CreateNdisMiniport(
        void
    ) override;

    virtual
    NTSTATUS
    StartNdisMiniport(
        void
    ) override;

    virtual
    void
    StartNdisMiniportDataPath(
        void
    ) override;

    virtual
    void
    StopNdisMiniport(
        void
    ) override;

    virtual
    NTSTATUS
    RemoveNdisMiniport(
        void
    ) override;

    virtual
    NDIS_STATUS
    SetRegistrationAttributes(
        void
    ) override;

    NTSTATUS
    SetupAdapterProperties(
        void
    );
};
