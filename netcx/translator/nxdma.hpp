// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NxRingContext.hpp"
#include "NxRingBuffer.hpp"

class NxDmaAdapter;

struct DmaContext
{
    void * const ScatterGatherBuffer = nullptr;
    void * const DmaTransferContext = nullptr;

    SCATTER_GATHER_LIST *ScatterGatherList = nullptr;
    MDL *MdlChain = nullptr;
    bool UnmapMdlChain = false;

    DmaContext(
        _In_ void *SGBuffer,
        _In_ void *DmaContext
    );
};

enum class BouncePolicy : ULONG
{
    IfNeeded = 0,
    Always,
};

// Wrapper for a DMA transfer of a *single* NET_PACKET.
// It makes sure to free DMA resources on destruction
class NxDmaTransfer
{
public:

    NxDmaTransfer(
        _In_ NxDmaAdapter const &DmaAdapter
    );

    NxDmaTransfer(
        _In_ NxDmaAdapter const &DmaAdapter,
        _In_ NET_PACKET const *Packet
    );

    NxDmaTransfer(
        _In_ NxDmaTransfer &&Other
    );

    ~NxDmaTransfer(
        void
    );

    operator bool(
        void
    ) const;

    DmaContext &
    GetTransferContext(
        void
    ) const;

    NxDmaTransfer(const NxDmaTransfer&) = delete;
    NxDmaTransfer& operator=(const NxDmaTransfer&) = delete;
    NxDmaTransfer& operator=(NxDmaTransfer&&) = delete;

private:

    NxDmaAdapter const &m_dmaAdapter;
    NET_PACKET const *m_packet = nullptr;
    bool m_valid = false;
};

class NxDmaAdapter : public NxNonpagedAllocation<'mDxN'>
{
public:

    NxDmaAdapter(
        _In_ NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES const &DatapathCapabilities,
        _In_ NET_RING_COLLECTION const & Rings
    ) noexcept;

    NTSTATUS
    Initialize(
        _In_ NET_CLIENT_DISPATCH const &ClientDispatch
    );

    DmaContext &
    GetDmaContextForPacket(
        _In_ NET_PACKET const &Packet
    ) const;

    bool
    BypassHal(
        void
    ) const;

    bool
    AlwaysBounce(
        void
    ) const;

    NxDmaTransfer
    InitializeDmaTransfer(
        _In_ NET_PACKET const &Packet
    ) const;

    NTSTATUS
    BuildScatterGatherListEx(
        _In_ NxDmaTransfer const &DmaTransfer,
        _In_ MDL *FirstMdl,
        _In_ size_t MdlOffset,
        _In_ size_t Size,
        _Out_ SCATTER_GATHER_LIST **ScatterGatherList
    ) const;

    void
    PutScatterGatherList(
        _In_ SCATTER_GATHER_LIST *ScatterGatherList
    ) const;

    MDL *
    GetMappedMdl(
        _In_ SCATTER_GATHER_LIST *ScatterGatherList,
        _In_ MDL *OriginalMdl
    ) const;

    void
    FreeAdapterObject(
        void
    ) const;

    void
    CleanupNetPacket(
        _In_ NET_PACKET const &Packet
    ) const;

    void
    FlushIoBuffers(
        _In_ NetRbPacketRange const &PacketRange
    ) const;

private:

    DmaContext &
    GetDmaContextForPacket(
        _In_ size_t const &Index
    ) const;

private:

    DMA_ADAPTER *m_dmaAdapter = nullptr;
    DEVICE_OBJECT *m_physicalDeviceObject = nullptr;

    bool m_flushIoBuffers = false;
    bool m_bypassHal = false;
    bool m_alwaysBounce = false;

    size_t m_packetRingSize = 0;
    size_t m_maximumPacketSize = 0;

    ULONG m_scatterGatherListSize = 0;
    KPoolPtr<UCHAR> m_scatterGatherListBuffer;
    KPoolPtr<UCHAR> m_dmaTransferBuffer;

    NxRingContext m_dmaContext;
};

