// Copyright (C) Microsoft Corporation. All rights reserved.

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"
#include "NxDma.tmh"

#include "NxDma.hpp"

MAKE_INTRUSIVE_LIST_ENUMERABLE(MDL, Next);

_Use_decl_annotations_
DmaContext::DmaContext(
    void *SGBuffer,
    void *DmaContext
) :
    ScatterGatherBuffer(SGBuffer),
    DmaTransferContext(DmaContext)
{

}

_Use_decl_annotations_
NxDmaTransfer::NxDmaTransfer(
    NxDmaAdapter const &DmaAdapter
) :
    m_dmaAdapter(DmaAdapter)
{

}

_Use_decl_annotations_
NxDmaTransfer::NxDmaTransfer(
    NxDmaAdapter const &DmaAdapter,
    NET_PACKET const *Packet
) :
    m_dmaAdapter(DmaAdapter),
    m_packet(Packet),
    m_valid(true)
{
}

_Use_decl_annotations_
NxDmaTransfer::NxDmaTransfer(
    NxDmaTransfer &&Other
) :
    m_dmaAdapter(Other.m_dmaAdapter),
    m_packet(Other.m_packet),
    m_valid(Other.m_valid)
{
    Other.m_packet = nullptr;
    Other.m_valid = false;
}

NxDmaTransfer::~NxDmaTransfer(
    void
)
{
    if (m_valid)
    {
        m_dmaAdapter.FreeAdapterObject();
    }
}

NxDmaTransfer::operator bool(
    void
) const
{
    return m_valid;
}

DmaContext &
NxDmaTransfer::GetTransferContext(
    void
) const
{
    NT_FRE_ASSERTMSG("Using an invalid NxDmaTransfer object.", m_valid);
    return m_dmaAdapter.GetDmaContextForPacket(*m_packet);
}

_Use_decl_annotations_
NxDmaAdapter::NxDmaAdapter(
    NET_CLIENT_ADAPTER_DATAPATH_CAPABILITIES const &DatapathCapabilities,
    NET_RING_COLLECTION const & Rings
) noexcept :
    m_dmaAdapter(static_cast<DMA_ADAPTER *>(DatapathCapabilities.TxMemoryConstraints.Dma.DmaAdapter)),
    m_physicalDeviceObject(static_cast<DEVICE_OBJECT *>(DatapathCapabilities.TxMemoryConstraints.Dma.PhysicalDeviceObject)),
    m_flushIoBuffers(!!DatapathCapabilities.FlushBuffers),
    m_dmaContext(Rings, NetRingTypePacket),
    m_packetRingSize(Rings.Rings[NetRingTypePacket]->NumberOfElements),
    m_maximumPacketSize(DatapathCapabilities.NominalMtu) // TODO: Probably need a new tx capability
{
}

_Use_decl_annotations_
NTSTATUS
NxDmaAdapter::Initialize(
    NET_CLIENT_DISPATCH const &ClientDispatch
)
{
    auto bouncePolicy = static_cast<BouncePolicy>(ClientDispatch.NetClientQueryDriverConfigurationUlong(DRIVER_CONFIG_ENUM::DMA_BOUNCE_POLICY));

    if (bouncePolicy == BouncePolicy::Always)
    {
        m_alwaysBounce = true;
    }

    DMA_ADAPTER_INFO adapterInfo = { DMA_ADAPTER_INFO_VERSION1 };

    auto allowHalBypass = ClientDispatch.NetClientQueryDriverConfigurationBoolean(DRIVER_CONFIG_ENUM::ALLOW_DMA_HAL_BYPASS);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_dmaAdapter->DmaOperations->GetDmaAdapterInfo(
            m_dmaAdapter,
            &adapterInfo));

    // We bypass HAL to map DMA buffers only if the DMA_ADAPTER says it is ok to do it and
    // the device is configured to allow such thing to happen
    m_bypassHal = ((adapterInfo.V1.Flags & ADAPTER_INFO_API_BYPASS) != 0) && allowHalBypass;

    if (BypassHal() || AlwaysBounce())
    {
        // If we bypass HAL to map buffers for DMA or always bounce the buffers we don't need to allocate
        // any extra resources
        return STATUS_SUCCESS;
    }

    // Calculate the size of our preallocated scatter gather list. Note that this size is
    // not always guaranteed to fit the SGL of a mapped NET_PACKET
    ULONG ulMaximumPacketSize;
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlSizeTToULong(
            m_maximumPacketSize,
            &ulMaximumPacketSize));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_dmaAdapter->DmaOperations->CalculateScatterGatherList(
            m_dmaAdapter,
            nullptr,
            ULongToPtr(PAGE_SIZE - 1),
            ulMaximumPacketSize,
            &m_scatterGatherListSize,
            nullptr));

    // Calculate and allocate the memory needed to hold our preallocated Scatter/Gather lists
    size_t allocationSize;
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlSizeTMult(
            m_packetRingSize,
            m_scatterGatherListSize,
            &allocationSize));

    m_scatterGatherListBuffer = MakeSizedPoolPtr<UCHAR>('mDxN', allocationSize);

    if (!m_scatterGatherListBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Calculate and allocate the memory needed to hold a DMA transfer context for each NET_PACKET
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlSizeTMult(
            m_packetRingSize,
            DMA_TRANSFER_CONTEXT_SIZE_V1,
            &allocationSize));

    m_dmaTransferBuffer = MakeSizedPoolPtr<UCHAR>('mDxN', allocationSize);

    if (!m_dmaTransferBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Initialize each NET_PACKET DMA context with their SGL and DMA transfer pointers
    CX_RETURN_IF_NOT_NT_SUCCESS(m_dmaContext.Initialize(sizeof(DmaContext)));

    for (auto i = 0u; i < m_packetRingSize; i++)
    {
        auto &dmaPacketContext = GetDmaContextForPacket(i);

        new (&dmaPacketContext) DmaContext(
            m_scatterGatherListBuffer.get() + i * m_scatterGatherListSize,
            m_dmaTransferBuffer.get() + i * DMA_TRANSFER_CONTEXT_SIZE_V1);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NxDmaTransfer
NxDmaAdapter::InitializeDmaTransfer(
    NET_PACKET const &Packet
) const
{
    auto& dmaContext = GetDmaContextForPacket(Packet);

    NT_ASSERT(dmaContext.UnmapMdlChain == false);
    NT_ASSERT(dmaContext.MdlChain == nullptr);
    NT_ASSERT(dmaContext.ScatterGatherList == nullptr);

    NTSTATUS ntStatus = m_dmaAdapter->DmaOperations->InitializeDmaTransferContext(
        m_dmaAdapter,
        dmaContext.DmaTransferContext);

    if (ntStatus == STATUS_SUCCESS)
    {
        return { *this, &Packet };
    }
    else
    {
        return { *this };
    }
}

bool
NxDmaAdapter::BypassHal(
    void
) const
{
    return m_bypassHal;
}

bool
NxDmaAdapter::AlwaysBounce(
    void
) const
{
    return m_alwaysBounce;
}

_Use_decl_annotations_
NTSTATUS
NxDmaAdapter::BuildScatterGatherListEx(
    NxDmaTransfer const &DmaTransfer,
    MDL *FirstMdl,
    size_t MdlOffset,
    size_t Size,
    SCATTER_GATHER_LIST **ScatterGatherList
) const
{
    ULONG ulMdlOffset;
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlSIZETToULong(
            MdlOffset,
            &ulMdlOffset));

    ULONG ulSize;
    CX_RETURN_IF_NOT_NT_SUCCESS(
        RtlSIZETToULong(
            Size,
            &ulSize));

    auto& dmaContext = DmaTransfer.GetTransferContext();

    auto oldIrql = KeRaiseIrqlToDpcLevel();

    auto ntStatus = m_dmaAdapter->DmaOperations->BuildScatterGatherListEx(
        m_dmaAdapter,
        m_physicalDeviceObject,
        dmaContext.DmaTransferContext,
        FirstMdl,
        ulMdlOffset,
        ulSize,
        DMA_SYNCHRONOUS_CALLBACK,
        nullptr, // ExecutionRoutine
        nullptr, // Context
        TRUE,
        dmaContext.ScatterGatherBuffer,
        m_scatterGatherListSize,
        nullptr, // DmaCompletionRoutine
        nullptr, // CompletionContext
        ScatterGatherList);

    KeLowerIrql(oldIrql);
    return ntStatus;
}

_Use_decl_annotations_
void
NxDmaAdapter::PutScatterGatherList(
    SCATTER_GATHER_LIST *ScatterGatherList
) const
{
    auto oldIrql = KeRaiseIrqlToDpcLevel();

    m_dmaAdapter->DmaOperations->PutScatterGatherList(
        m_dmaAdapter,
        ScatterGatherList,
        TRUE);

    KeLowerIrql(oldIrql);
}

_Use_decl_annotations_
DmaContext &
NxDmaAdapter::GetDmaContextForPacket(
    NET_PACKET const &Packet
) const
{
    return m_dmaContext.GetContextByElement<DmaContext>(Packet);
}

_Use_decl_annotations_
DmaContext &
NxDmaAdapter::GetDmaContextForPacket(
    _In_ size_t const &Index
) const
{
    return m_dmaContext.GetContext<DmaContext>(Index);
}

_Use_decl_annotations_
MDL *
NxDmaAdapter::GetMappedMdl(
    SCATTER_GATHER_LIST *ScatterGatherList,
    MDL *OriginalMdl
) const
{
    MDL *mappedMdl;
    NTSTATUS ntStatus = m_dmaAdapter->DmaOperations->BuildMdlFromScatterGatherList(
        m_dmaAdapter,
        ScatterGatherList,
        OriginalMdl,
        &mappedMdl);

    if (ntStatus != STATUS_SUCCESS)
    {
        return nullptr;
    }

    return mappedMdl;
}

void
NxDmaAdapter::FreeAdapterObject(
    void
) const
{
    m_dmaAdapter->DmaOperations->FreeAdapterObject(
        m_dmaAdapter,
        DeallocateObjectKeepRegisters);
}

_Use_decl_annotations_
void
NxDmaAdapter::CleanupNetPacket(
    NET_PACKET const &Packet
) const
{
    if (BypassHal() || AlwaysBounce())
    {
        // Nothing to cleanup
        return;
    }

    auto& dmaContext = GetDmaContextForPacket(Packet);

    if (dmaContext.UnmapMdlChain)
    {
        for (auto& mdl : dmaContext.MdlChain)
        {
            NT_ASSERT(mdl.MdlFlags & MDL_MAPPED_TO_SYSTEM_VA);

            MmUnmapLockedPages(
                mdl.MappedSystemVa,
                &mdl);
        }
    }

    dmaContext.UnmapMdlChain = false;
    dmaContext.MdlChain = nullptr;

    if (dmaContext.ScatterGatherList != nullptr)
    {
        PutScatterGatherList(dmaContext.ScatterGatherList);
        dmaContext.ScatterGatherList = nullptr;
    }
}

void
NxDmaAdapter::FlushIoBuffers(
    _In_ NetRbPacketRange const &PacketRange
) const
{
    if (!m_flushIoBuffers)
        return;

    for (auto& packet : PacketRange)
    {
        auto& dmaContext = GetDmaContextForPacket(packet);
        KeFlushIoBuffers(
            dmaContext.MdlChain,
            FALSE,
            TRUE);
    }
}
