// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <ndisguid.h>

#include "NxAdapter.hpp"
#include "NxDefaultDatapath.hpp"
#include "powerpolicy/NxPowerPolicy.hpp"
#include "ndisminiport/NxNdisMiniport.hpp"
#include "ndisminiport/NxNdisPnpPower.hpp"

#include "NxAdapter.tmh"

_Use_decl_annotations_
void
NxAdapter::SendNetBufferLists(
    NET_BUFFER_LIST * NetBufferLists,
    NDIS_PORT_NUMBER PortNumber,
    ULONG SendFlags
)
{
    m_nblDatapath.SendNetBufferLists(
        NetBufferLists,
        PortNumber,
        SendFlags);
}

_Use_decl_annotations_
void
NxAdapter::ReturnNetBufferLists(
    NET_BUFFER_LIST * NetBufferLists,
    ULONG ReturnFlags
)
{
    m_nblDatapath.ReturnNetBufferLists(
        NetBufferLists,
        ReturnFlags);
}

_Use_decl_annotations_
bool
NxAdapter::NetClientOidPreProcess(
    NDIS_OID_REQUEST& Request,
    NDIS_STATUS& NdisStatus
) const
{
    for (auto& app : m_apps)
    {
        if (m_clientDispatch->NdisOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_clientDispatch->NdisOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}

_Use_decl_annotations_
bool
NxAdapter::NetClientDirectOidPreProcess(
    NDIS_OID_REQUEST& Request,
    NDIS_STATUS& NdisStatus
) const
{
    for (size_t i = 0; i < m_apps.count(); i++)
    {
        auto& app = m_apps[i];

        if (m_clientDispatch->NdisDirectOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_clientDispatch->NdisDirectOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}

_Use_decl_annotations_
bool
NxAdapter::NetClientSynchrohousOidPreProcess(
    NDIS_OID_REQUEST& Request,
    NDIS_STATUS& NdisStatus
) const
{
    for (size_t i = 0; i < m_apps.count(); i++)
    {
        auto& app = m_apps[i];

        if (m_clientDispatch->NdisSynchronousOidRequestHandler)
        {
            NTSTATUS status;
            auto const handled = m_clientDispatch->NdisSynchronousOidRequestHandler(
                app.get(),
                &Request,
                &status);

            if (handled)
            {
                NT_FRE_ASSERT(status != STATUS_PENDING);

                NdisStatus = NdisConvertNtStatusToNdisStatus(status);
                return handled;
            }
        }
    }

    return false;
}
