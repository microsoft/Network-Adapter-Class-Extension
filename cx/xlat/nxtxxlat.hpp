// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the 
    transmit path.

--*/

#pragma once

#include <NetClientAdapter.h>

#include "NxExecutionContext.hpp"
#include "NxSignal.hpp"
#include "NxRingBuffer.hpp"
#include "NxContextBuffer.hpp"
#include "NxNblQueue.hpp"
#include "NxNbl.hpp"

class NxTxXlat :
    public INxNblTx,
    public NxNonpagedAllocation<'xTxN'>
{
public:

    NxTxXlat(
        _In_ NET_CLIENT_DISPATCH const * Dispatch,
        _In_ NET_CLIENT_ADAPTER Adapter,
        _In_ NET_CLIENT_ADAPTER_DISPATCH const * AdapterDispatch
        ) noexcept;

    virtual
    ~NxTxXlat(
        void
        );

    void
    TransmitThread();

    NTSTATUS
    StartQueue(
        void
        );

    void
    Notify(
        void
        );

    //
    // INxNblTx
    //

    virtual
    void
    SendNetBufferLists(
        _In_ NET_BUFFER_LIST * NblChain,
        _In_ ULONG PortNumber,
        _In_ ULONG NumberOfNbls,
        _In_ ULONG SendFlags
        );

private:

    NxExecutionContext m_executionContext;

    NET_CLIENT_DISPATCH const * m_dispatch = nullptr;
    NET_CLIENT_ADAPTER m_adapter = nullptr;
    NET_CLIENT_ADAPTER_DISPATCH const * m_adapterDispatch = nullptr;
    NET_CLIENT_ADAPTER_PROPERTIES m_adapterProperties = {};
    NDIS_MEDIUM m_mediaType;
    INxNblDispatcher *m_nblDispatcher = nullptr;
    NxInterlockedFlag m_queueNotification;
    NxNblQueue m_synchronizedNblQueue;

    // Initialized in StartQueue
    NET_CLIENT_QUEUE m_queue = nullptr;
    NET_CLIENT_QUEUE_DISPATCH const * m_queueDispatch = nullptr;
    size_t m_checksumOffset = NET_PACKET_EXTENSION_INVALID_OFFSET;
    size_t m_lsoOffset = NET_PACKET_EXTENSION_INVALID_OFFSET;

    // allocated in Init
    NET_DATAPATH_DESCRIPTOR const * m_descriptor;
    NxRingBuffer m_ringBuffer;
    NxContextBuffer m_contextBuffer;

    //
    // Datapath variables
    // All below will change as TransmitThread runs
    //
    NET_BUFFER_LIST *m_currentNbl = nullptr;
    NET_BUFFER *m_currentNetBuffer = nullptr;

    // These are used to determine when to arm notifications
    // and halt queue operation
    bool m_producedPackets = false;
    bool m_completedPackets = false;
    
    struct ArmedNotifications
    {
        union
        {
            struct
            {
                bool ShouldArmTxCompletion : 1;
                bool ShouldArmNblArrival : 1;
                UINT8 Reserved : 6;
            } Flags;

            UINT8 Value = 0;
        };
    } m_lastArmedNotifications;

    void
    ArmNetBufferListArrivalNotification();

    void
    ArmAdapterTxNotification();

    ArmedNotifications
    GetNotificationsToArm();

    // The high level operations of the NBL Tx translation
    void
    PollNetBufferLists();

    void
    DrainCompletions();

    void
    TranslateNbls();

    void
    YieldToNetAdapter();

    void
    WaitForMoreWork();

    // These operations are used exclusively while
    // winding down the Tx path
    void
    DropQueuedNetBufferLists();

    void
    AbortNbls(
        _In_opt_ NET_BUFFER_LIST *nblChain);

    // drain queue
    PNET_BUFFER_LIST
    DequeueNetBufferListQueue();

    void
    ArmNotifications(
        _In_ ArmedNotifications reasons
        );

    NTSTATUS
    Initialize(
        void
        );

    NTSTATUS
    PreparePacketExtensions(
        _Inout_ Rtl::KArray<NET_CLIENT_PACKET_EXTENSION>& addedPacketExtensions
        );

    size_t
    GetPacketExtensionOffsets(
        PCWSTR ExtensionName,
        ULONG ExtensionVersion
        ) const;

    void
    SetupTxThreadProperties();
};
