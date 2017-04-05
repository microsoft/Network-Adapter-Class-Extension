/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxTxXlat.hpp

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the 
    transmit path.

--*/

#pragma once

#include "NxExecutionContext.hpp"
#include "NxSignal.hpp"
#include "NxRingBuffer.hpp"
#include "NxNblQueue.hpp"
#include "NxNbl.hpp"

class NxTxXlat : public INxAppQueue,
                 public INxNblTx,
                 public NxNonpagedAllocation<'xTxN'>
{
public:

    NxTxXlat(_In_ INxAdapter *adapter) :
        m_nblDispatcher(adapter->GetNblDispatcher())
    {
        NOTHING;
    }

    virtual ~NxTxXlat();

    NTSTATUS StartQueue(_In_ INxAdapter *adapter);


    //
    // INxAppQueue
    //

    virtual NTSTATUS Initialize(
        _In_ HNETPACKETCLIENT packetClient,
        _In_ NXQUEUE_CREATION_PARAMETERS const *params,
        _Out_ NXQUEUE_CREATION_RESULT *result);

    virtual void Notify();


    //
    // INxNblTx
    //

    virtual void SendNetBufferLists(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG portNumber,
        _In_ ULONG numberOfNbls,
        _In_ ULONG sendFlags);

private:

    struct PAGED PacketContext : public _NDIS_DEBUG_BLOCK<'CPTN'>
    {
        PNET_BUFFER_LIST NetBufferListToComplete = nullptr;
    };

    // Initialized in constructor
    NxExecutionContext m_executionContext;
    NxInterlockedFlag m_queueNotification;
    NxNblQueue m_synchronizedNblQueue;
    INxNblDispatcher *m_nblDispatcher = nullptr;

    // Initialized in StartQueue
    wistd::unique_ptr<INxAdapterQueue> m_adapterQueue;
    NDIS_MEDIUM m_mediaType;

    // allocated in Init
    NxRingBuffer m_ringBuffer;

    ULONG m_thread802_15_4MSIExExtensionOffset = 0;
    ULONG m_thread802_15_4StatusOffset = 0;


    //
    // Datapath variables
    // All below will change as TransmitThread runs
    //
    PNET_BUFFER_LIST m_currentNbl = nullptr;

    // These are used to determine when to arm notifications
    // and halt queue operation
    bool m_netBufferQueueEmpty = false;
    ULONG m_outstandingPackets = 0;
    ULONG m_producedPackets = 0;
    ULONG m_completedPackets = 0;
    
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

    void ArmNetBufferListArrivalNotification();

    void ArmAdapterTxNotification();

    PacketContext & GetPacketContext(_In_ NET_PACKET &netPacket);

    ArmedNotifications GetNotificationsToArm();

    // The high level operations of the NBL Tx translation
    void PollNetBufferLists();
    void DrainCompletions();
    void TranslateNbls();
    void YieldToNetAdapter();
    void WaitForMoreWork();

    // These operations are used exclusively while
    // winding down the Tx path
    void DropQueuedNetBufferLists();

    // drain queue
    PNET_BUFFER_LIST DequeueNetBufferListQueue();

    void ArmNotifications(ArmedNotifications reasons);

    enum class TranslationResult
    {
        Success,
        InsufficientFragments,
    };

    TranslationResult TranslateNetBufferToNetPacket(
        _In_    NET_BUFFER      &netBuffer,
        _Inout_ NET_PACKET      &netPacket);

    void ReinitializePacket(_In_ NET_PACKET & netPacket);

    void TransmitThread();
};
