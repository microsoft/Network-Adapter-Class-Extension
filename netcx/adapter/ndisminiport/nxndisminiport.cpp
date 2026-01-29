// Copyright (C) Microsoft Corporation. All rights reserved.

#include "precompiled.hpp"

#include "NxNdisMiniport.hpp"

#include "NxUtility.hpp"
#include "NxNdisMiniport.tmh"

#ifdef NETCX_UMDF
#include "BridgeToKernel.hpp"
#endif

template<typename T>
_Use_decl_annotations_
void
NxNdisMiniport::IndicateMiniportStatusChange(
    NDIS_STATUS StatusCode,
    T const & Payload,
    ULONG const * StatusBufferSizeOverride
)
{
    NDIS_STATUS_INDICATION statusIndication = MakeNdisStatusIndication(
        m_ndisAdapterHandle,
        StatusCode,
        Payload);

    if (StatusBufferSizeOverride != nullptr)
    {
        statusIndication.StatusBufferSize = *StatusBufferSizeOverride;
    }

#ifdef NETCX_KMDF
    NdisMIndicateStatusEx(m_ndisAdapterHandle, &statusIndication);
#else
    m_bridgeToKernel->IndicateMiniportStatusChange(statusIndication);
#endif
}

template
void
NxNdisMiniport::IndicateMiniportStatusChange<NDIS_PM_CAPABILITIES>(
    NDIS_STATUS,
    NDIS_PM_CAPABILITIES const &,
    ULONG const *
);

template
void
NxNdisMiniport::IndicateMiniportStatusChange<NDIS_LINK_STATE>(
    NDIS_STATUS,
    NDIS_LINK_STATE const &,
    ULONG const *
);

template
void
NxNdisMiniport::IndicateMiniportStatusChange<ULONG>(
    NDIS_STATUS,
    ULONG const &,
    ULONG const *
);

template
void
NxNdisMiniport::IndicateMiniportStatusChange<NET_ADAPTER_LINK_LAYER_ADDRESS>(
    NDIS_STATUS,
    NET_ADAPTER_LINK_LAYER_ADDRESS const &,
    ULONG const *
);

template
void
NxNdisMiniport::IndicateMiniportStatusChange<NDIS_PM_WAKE_REASON>(
    NDIS_STATUS,
    NDIS_PM_WAKE_REASON const &,
    ULONG const *
);
