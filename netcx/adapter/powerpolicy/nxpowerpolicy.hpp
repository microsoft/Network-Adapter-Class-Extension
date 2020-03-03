// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "NxWakeSource.hpp"
#include "NxPowerOffload.hpp"
#include "NxPowerList.hpp"

#define NETCX_POWER_TAG 'wPxN'

class NxPowerList;

class NxPowerPolicy
{
private:

    NET_ADAPTER_POWER_OFFLOAD_ARP_CAPABILITIES
        m_powerOffloadArpCapabilities = {};

    NET_ADAPTER_POWER_OFFLOAD_NS_CAPABILITIES
        m_powerOffloadNSCapabilities = {};

    NET_ADAPTER_WAKE_BITMAP_CAPABILITIES
        m_wakeBitmapCapabilities = {};

    NET_ADAPTER_WAKE_MAGIC_PACKET_CAPABILITIES
        m_magicPacketCapabilities = {};

    NET_ADAPTER_WAKE_MEDIA_CHANGE_CAPABILITIES
        m_wakeMediaChangeCapabilities = {};

    NET_ADAPTER_WAKE_PACKET_FILTER_CAPABILITIES
        m_wakePacketFilterCapabilities = {};

    NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS
        m_powerPolicyEventCallbacks = {};

    WDFDEVICE const
        m_device;

    NETADAPTER const
        m_adapter;

    RECORDER_LOG
        m_recorderLog = nullptr;

    //
    // Head of the Wake patterns list
    //
    SINGLE_LIST_ENTRY
        m_WakeListHead = {};

    //
    // Static wake source entries
    //
    NxWakeMediaChange
        m_wakeOnMediaChange;

    NxWakePacketFilterMatch
        m_wakeOnPacketFilterMatch;

    //
    // Head of the protocol offload list
    //
    SINGLE_LIST_ENTRY
        m_ProtocolOffloadListHead = {};

    //
    // A local copy of the PM Parameters reported by NDIS
    //
    NDIS_PM_PARAMETERS
        m_PMParameters = {};

    NxWakePattern *
    RemoveWakePatternByID(
        _In_ ULONG PatternID
    );

    NxPowerOffload *
    RemovePowerOffloadByID(
        _In_ ULONG OffloadID
    );

    void
    UpdatePatternEntryEnabledField(
        _In_ NxWakePattern * Entry
    );

    void
    UpdateProtocolOffloadEntryEnabledField(
        _In_ NxPowerOffload * Entry
    );

public:

    NxPowerPolicy(
        _In_ WDFDEVICE Device,
        _In_ NET_DEVICE_POWER_POLICY_EVENT_CALLBACKS const & PowerPolicyCallbacks,
        _In_ NETADAPTER Adapter,
        _In_ RECORDER_LOG RecorderLog
    );

    ~NxPowerPolicy(
        void
    );

    void
    InitializeNdisCapabilities(
        _In_ ULONG MediaSpecificWakeUpEvents,
        _Out_ NDIS_PM_CAPABILITIES * NdisPowerCapabilities
    );

    void
    SetPowerOffloadArpCapabilities(
        _In_ NET_ADAPTER_POWER_OFFLOAD_ARP_CAPABILITIES const * Capabilities
    );

    void
    SetPowerOffloadNSCapabilities(
        _In_ NET_ADAPTER_POWER_OFFLOAD_NS_CAPABILITIES const * Capabilities
    );

    void
    SetWakeBitmapCapabilities(
        _In_ NET_ADAPTER_WAKE_BITMAP_CAPABILITIES const * Capabilities
    );

    void
    SetMagicPacketCapabilities(
        _In_ NET_ADAPTER_WAKE_MAGIC_PACKET_CAPABILITIES const * Capabilities
    );

    void
    SetWakeMediaChangeCapabilities(
        _In_ NET_ADAPTER_WAKE_MEDIA_CHANGE_CAPABILITIES const * Capabilities
    );

    void
    SetWakePacketFilterCapabilities(
        _In_ NET_ADAPTER_WAKE_PACKET_FILTER_CAPABILITIES const * Capabilities
    );

    void
    UpdatePowerOffloadList(
        _In_ bool IsInPowerTransition,
        _In_ NxPowerOffloadList * List
    ) const;

    void
    UpdateWakeSourceList(
        _In_ bool IsInPowerTransition,
        _In_ NxWakeSourceList * List
    ) const;

    NDIS_STATUS
    SetParameters(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    AddWakePattern(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    RemoveWakePattern(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    AddProtocolOffload(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    NDIS_STATUS
    RemoveProtocolOffload(
        _In_ NDIS_OID_REQUEST::_REQUEST_DATA::_SET const * Set
    );

    RECORDER_LOG
    GetRecorderLog(
        void
    );
};
