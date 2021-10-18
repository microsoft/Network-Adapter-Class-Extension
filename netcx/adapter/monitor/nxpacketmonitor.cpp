// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include "NxPacketMonitor.tmh"
#include "NxPacketMonitor.hpp"

#include "version.hpp"
#include "NxDevice.hpp"
#include "NxAdapter.hpp"
#include "NxAdapterCollection.hpp"

_Use_decl_annotations_
NxPacketMonitor::NxPacketMonitor(
    NxAdapter & Adapter
)
    : m_adapter(Adapter)
{
}

_Use_decl_annotations_
NxPacketMonitor::~NxPacketMonitor(
    void
)
{
    UnregisterAdapter();
}

_Use_decl_annotations_
void
NxPacketMonitor::UnregisterAdapter(
    void
)
{
    KLockThisExclusive lock(m_registrationLock);

    if (m_isRegistered)
    {
        PktMonClientComponentUnregister(&m_clientComponentContext);
        RtlZeroMemory(&m_clientComponentContext, sizeof(m_clientComponentContext));
        m_isRegistered = false;
    }
}

_Use_decl_annotations_
void
NxPacketMonitor::EnumerateAndRegisterAdapters(
    void
)
/*++

Routine Description:

    Static method being called when PacketMon NMR provider is being attached. 

--*/
{
    auto driverContext = GetCxDriverContextFromHandle(WdfGetDriver());
    driverContext->GetDeviceCollection().ForEach([&](NxDevice & device)
    {
        device.GetAdapterCollection().ForEach([&](NxAdapter & adapter)
        {
            adapter.GetPacketMonitor().RegisterAdapter();
        });
    });
}

_Use_decl_annotations_
void
NxPacketMonitor::EnumerateAndUnRegisterAdapters(
    void
)
/*++

Routine Description:

    Static method being called when PacketMon NMR provider is being detached. 

--*/
{
    auto driverContext = GetCxDriverContextFromHandle(WdfGetDriver());
    driverContext->GetDeviceCollection().ForEach([&](NxDevice & device)
    {
        device.GetAdapterCollection().ForEach([&](NxAdapter & adapter)
        {
            adapter.GetPacketMonitor().UnregisterAdapter();
        });
    });
}

_Use_decl_annotations_
NTSTATUS
NxPacketMonitor::RegisterAdapter(
    void
)
{
    KLockThisExclusive lock(m_registrationLock);

    if (m_isRegistered)
    {
        return STATUS_SUCCESS;
    }

    NET_IFINDEX interfaceId;
    auto const netLuid = m_adapter.GetNetLuid();
    auto description = m_adapter.GetInstanceName();
    auto driverName = m_adapter.GetDriverImageName();
    NdisIfGetInterfaceIndexFromNetLuid(netLuid, &interfaceId);

    //
    // Note: PktMonClientComponentRegister is expected to fail when 
    // PacketMon NMR provider is not loaded. 
    //
    CX_RETURN_IF_NOT_NT_SUCCESS(
        PktMonClientComponentRegister(
            &m_clientComponentContext, 
            &driverName, 
            &description,
            PktMonComp_NetCx, 
            m_adapter.GetMediaType(), 
            PktMonDirTag_Rx, 
            PktMonDirTag_Tx));

    auto cleanup = wil::scope_exit([this]()
    {
        PktMonClientComponentUnregister(&m_clientComponentContext);
    });

    CX_RETURN_IF_NOT_NT_SUCCESS(
        PktMonClientSetCompProperty(
            &m_clientComponentContext,
            PktMonCompProp_IfIndex,
            &interfaceId,
            sizeof(interfaceId)));

    auto mediumType = m_adapter.GetMediaType();
    CX_RETURN_IF_NOT_NT_SUCCESS(
        PktMonClientSetCompProperty(
            &m_clientComponentContext,
            PktMonCompProp_NdisMedium,
            &mediumType,
            sizeof(mediumType)));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        PktMonClientSetCompProperty(
            &m_clientComponentContext,
            PktMonCompProp_PhysAddress,
            &m_adapter.m_currentLinkLayerAddress.Address,
            m_adapter.m_currentLinkLayerAddress.Length));

    UNICODE_STRING lowerEdgeName = RTL_CONSTANT_STRING(PKTMON_EDGE_LOWER);
    CX_RETURN_IF_NOT_NT_SUCCESS(
        PktMonClientAddEdge(
            &m_clientComponentContext,
            &lowerEdgeName,
            PktMonDirTag_Rx,
            PktMonDirTag_Tx,
            m_adapter.GetMediaType(),
            &m_clientLowerEdgeContext));

    cleanup.release();
    m_isRegistered = true;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
PKTMON_EDGE_CONTEXT const &
NxPacketMonitor::GetLowerEdgeContext(
    void
) const
{
    return m_clientLowerEdgeContext;
}

_Use_decl_annotations_
PKTMON_COMPONENT_CONTEXT const &
NxPacketMonitor::GetComponentContext(
    void
) const
{
    return m_clientComponentContext;
}
