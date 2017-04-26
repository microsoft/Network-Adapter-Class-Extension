/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxRxXlat.hpp

Abstract:

    Contains the per-queue logic for NBL-to-NET_PACKET translation on the 
    receive path.

--*/

#pragma once

#include "NxExecutionContext.hpp"
#include "NxSignal.hpp"
#include "NxRingBuffer.hpp"
#include "NxNbl.hpp"
#include <KArray.h>

class NxRxXlat : public INxAppQueue,
                 public INxNblRx,
                 public NxNonpagedAllocation<'lXRN'>
{
public:

    NxRxXlat(_In_ INxAdapter *adapter) :
        m_nblDispatcher(adapter->GetNblDispatcher()),
        m_adapterHandle(adapter->GetNdisAdapterHandle())
    {
        NOTHING;
    }

    virtual ~NxRxXlat();

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
    // INxNblRx
    //

    virtual void ReturnNetBufferLists(
        _In_ NET_BUFFER_LIST *nblChain,
        _In_ ULONG numberOfNbls,
        _In_ ULONG receiveCompleteFlags);

private:

    struct PAGED PacketContext : public _NDIS_DEBUG_BLOCK<'CPRN'>
    {
        unique_nbl AssociatedNetBufferList;
        bool MemoryPreallocated = false;

        // Must go at end
        MDL Mdl;
        PFN_NUMBER _PfnNumbers[ANYSIZE_ARRAY];
    };

    struct PAGED NblContext : public _NDIS_DEBUG_BLOCK<'LBNN'>
    {
        _Interlocked_ volatile LONG Returned = true;
    };

    static_assert(
        sizeof(NblContext) <= RTL_FIELD_SIZE(NET_BUFFER_LIST, MiniportReserved),
        "The NblContext must fit in the MiniportReserved section of the NBL.");

    struct ArmedNotifications
    {
        union
        {
            struct
            {
                bool ShouldArmRxIndication : 1;
                bool ShouldArmNblReturned : 1;
                UINT8 Reserved : 6;
            } Flags;

            UINT8 Value = 0;
        };
    } m_lastArmedNotifications;

    NxExecutionContext m_executionContext;

    INxNblDispatcher *m_nblDispatcher = nullptr;
    NDIS_HANDLE m_adapterHandle = nullptr;
    unique_nbl_pool m_netBufferListPool;

    NxRingBuffer m_ringBuffer;
    Rtl::KArray<wistd::unique_ptr<INxQueueAllocation>> m_allocations;

    wistd::unique_ptr<INxAdapterQueue> m_adapterQueue;

    NDIS_MEDIUM m_mediaType;

    // changes as translation routine runs
    ULONG m_outstandingPackets = 0;
    ULONG m_postedPackets = 0;
    ULONG m_returnedPackets = 0;

    ULONG m_thread802_15_4MSIExExtensionOffset = 0;
    ULONG m_thread802_15_4StatusOffset = 0;

    // notification signals
    NxInterlockedFlag m_returnedNblNotification;

    ArmedNotifications GetNotificationsToArm();
    void ArmNotifications(ArmedNotifications notifications);

    void ArmNetBufferListReturnedNotification();
    void ArmAdapterRxNotification();

    bool m_stopRequested = false;

    PacketContext & GetPacketContext(_In_ NET_PACKET &netPacket);
    static NblContext & GetNblContext(_In_ NET_BUFFER_LIST & nbl);

    unique_nbl AllocateOneNbl();

    void ReinitializePacket(_In_ NET_PACKET & netPacket);

    void PrepareBuffersForNetAdapter();
    void YieldToNetAdapter();
    void IndicateNblsToNdis();
    void WaitForMoreWork();

    void ReceiveThread();

    NTSTATUS AllocateNetBufferLists();
    NTSTATUS PreallocateRxBuffers();
    NTSTATUS InitializeRingBuffer();

    unique_nbl_pool CreateNblPool();
};
