// Copyright (C) Microsoft Corporation. All rights reserved.

#ifdef _KERNEL_MODE
#include <ntosp.h>
#else
#include <umwdm.h>
#include <KPtr.h>
#endif

#include <net/extension.h>
#include <net/ring.h>
#include <net/ringcollection.h>
#include <net/packet.h>
#include <net/fragment.h>
#include <net/virtualaddress_p.h>
#include <net/databuffer_p.h>
#include <net/logicaladdress_p.h>
#include <KPtr.h>
#include "adapter/extension/NxExtensionLayout.hpp"
#include "NetClientBuffer.h"
#include "bm/BufferPool.hpp"

#include <NxTrace.hpp>

#include "Queue.hpp"

#include "Queue.tmh"

using namespace NetCx::Core;

Queue::Queue(
    void
)
    : m_packetLayout(sizeof(NET_PACKET), alignof(NET_PACKET))
    , m_fragmentLayout(sizeof(NET_FRAGMENT), alignof(NET_FRAGMENT))
    , m_bufferLayout(sizeof(size_t), alignof(size_t))
{
}

NTSTATUS
Queue::CreateRing(
    _In_ size_t ElementSize,
    _In_ UINT32 ElementCount,
    _In_ NET_RING_TYPE RingType
)
{
    NT_FRE_ASSERT(RTL_IS_POWER_OF_TWO(ElementCount));

    auto const size = ElementSize * ElementCount + FIELD_OFFSET(NET_RING, Buffer[0]);

    auto ring = reinterpret_cast<NET_RING *>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED | POOL_FLAG_CACHE_ALIGNED, size, 'BRxN'));

    CX_RETURN_NTSTATUS_IF(
        STATUS_INSUFFICIENT_RESOURCES,
        ! ring);

    ring->OSReserved2[1] = this;
    ring->ElementStride = static_cast<USHORT>(ElementSize);
    ring->NumberOfElements = ElementCount;
    ring->ElementIndexMask = ElementCount - 1;

    RtlCopyMemory(&m_shadowRings[RingType], ring, FIELD_OFFSET(NET_RING, Buffer[0]));

    m_rings[RingType].reset(ring);
    m_ringCollection.Rings[RingType] = m_rings[RingType].get();

    return STATUS_SUCCESS;
}

void
Queue::EnumerateCallback(
    _In_ PVOID Context,
    _In_ SIZE_T Index,
    _In_ UINT64 LogicalAddress,
    _In_ PVOID VirtualAddress
)
{
    Queue* queue = reinterpret_cast<Queue *>(Context);

    auto laLookupTable = reinterpret_cast<UINT64 *>(queue->m_rings[NetRingTypeDataBuffer]->OSReserved2[2]);
    auto vaLookupTable = reinterpret_cast<void **>(queue->m_rings[NetRingTypeDataBuffer]->OSReserved2[3]);

    laLookupTable[Index] = LogicalAddress;
    vaLookupTable[Index] = VirtualAddress;
}

NTSTATUS
Queue::Initialize(
    _In_ Rtl::KArray<NET_EXTENSION_PRIVATE> & Extensions,
    _In_ UINT32 NumberOfPackets,
    _In_ UINT32 NumberOfFragments,
    _In_ UINT32 NumberOfDataBuffers,
    _In_ BufferPool * Pool
)
{
    for (auto const & extension : Extensions)
    {
        switch (extension.Type)
        {
        case NetExtensionTypePacket:
            CX_RETURN_IF_NOT_NT_SUCCESS(
                m_packetLayout.PutExtension(
                    extension.Name,
                    extension.Version,
                    extension.Type,
                    extension.Size,
                    extension.NonWdfStyleAlignment));

            break;

        case NetExtensionTypeFragment:
            CX_RETURN_IF_NOT_NT_SUCCESS(
                m_fragmentLayout.PutExtension(
                    extension.Name,
                    extension.Version,
                    extension.Type,
                    extension.Size,
                    extension.NonWdfStyleAlignment));
            break;
        }
    }

    // Temporary legacy LSO registration
    CX_RETURN_IF_NOT_NT_SUCCESS(
        m_packetLayout.PutExtension(
            L"ms_packet_lso",
            1,
            NetExtensionTypePacket,
            sizeof(UINT32),
            alignof(UINT32)));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateRing(
            m_fragmentLayout.Generate(),
            NumberOfFragments,
            NetRingTypeFragment));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateRing(
            m_packetLayout.Generate(),
            NumberOfPackets,
            NetRingTypePacket));

    if (Pool)
    {
        m_bufferPool = Pool;

        m_bufferLayout.PutExtension(
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
            NetExtensionTypeBuffer,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1_SIZE,
            alignof(NET_FRAGMENT_LOGICAL_ADDRESS));

        m_bufferLayout.PutExtension(
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
            NetExtensionTypeBuffer,
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1_SIZE,
            alignof(NET_FRAGMENT_VIRTUAL_ADDRESS));

        CX_RETURN_IF_NOT_NT_SUCCESS(
            CreateRing(
                m_bufferLayout.Generate(),
                NumberOfDataBuffers,
                NetRingTypeDataBuffer));

        m_laLookupTable = MakeSizedPoolPtrNP<BYTE>('nxbp', sizeof(UINT64) * NumberOfDataBuffers);
        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! m_laLookupTable);

        m_vaLookupTable =
            MakeSizedPoolPtrNP<BYTE>('nxbp', sizeof(void *) * NumberOfDataBuffers);
        CX_RETURN_NTSTATUS_IF(
            STATUS_INSUFFICIENT_RESOURCES,
            ! m_vaLookupTable);

        m_rings[NetRingTypeDataBuffer]->OSReserved2[2] = m_laLookupTable.get();
        m_rings[NetRingTypeDataBuffer]->OSReserved2[3] = m_vaLookupTable.get();

        m_bufferPool->Enumerate(EnumerateCallback, this);

        NET_EXTENSION_PRIVATE extensionToQuery = {
            NET_FRAGMENT_EXTENSION_DATA_BUFFER_NAME,
            NET_FRAGMENT_EXTENSION_DATA_BUFFER_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_DATA_BUFFER_VERSION_1,
            0,
            NetExtensionTypeFragment };

        GetExtension(&extensionToQuery, &m_fragmentDataBufferExt);

        NET_EXTENSION_PRIVATE extensionToQuery2 = {
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
            0,
            NetExtensionTypeFragment };

        GetExtension(&extensionToQuery2, &m_fragmentVaExt);

        NET_EXTENSION_PRIVATE extensionToQuery3 = {
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1,
            0,
            NetExtensionTypeBuffer };

        GetExtension(&extensionToQuery3, &m_bufferVaExt);

        NET_EXTENSION_PRIVATE extensionToQuery4 = {
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1_SIZE,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
            0,
            NetExtensionTypeBuffer };

        GetExtension(&extensionToQuery4, &m_bufferLaExt);
    }
    else
    {
        m_rings[NetRingTypeDataBuffer].reset(nullptr);
        m_ringCollection.Rings[NetRingTypeDataBuffer] = nullptr;
    }

    return STATUS_SUCCESS;
}

void
Queue::GetExtension(
    _In_ const NET_EXTENSION_PRIVATE * ExtensionToQuery,
    _Out_ NET_EXTENSION * Extension
) const
{
    NET_EXTENSION_PRIVATE const * extension = nullptr;
    NET_RING_TYPE type = NetRingTypePacket;
    switch (ExtensionToQuery->Type)
    {

    case NetExtensionTypePacket:
        type = NetRingTypePacket;
        extension = m_packetLayout.GetExtension(
            ExtensionToQuery->Name,
            ExtensionToQuery->Version,
            ExtensionToQuery->Type);

        break;

    case NetExtensionTypeFragment:
        type = NetRingTypeFragment;
        extension = m_fragmentLayout.GetExtension(
            ExtensionToQuery->Name,
            ExtensionToQuery->Version,
            ExtensionToQuery->Type);

        break;

    case NetExtensionTypeBuffer:
        type = NetRingTypeDataBuffer;
        extension = m_bufferLayout.GetExtension(
            ExtensionToQuery->Name,
            ExtensionToQuery->Version,
            ExtensionToQuery->Type);

        break;

    }

    Extension->Enabled = extension != nullptr;
    if (Extension->Enabled)
    {
        auto const ring = m_ringCollection.Rings[type];
        Extension->Reserved[0] = ring->Buffer + extension->AssignedOffset;
        Extension->Reserved[1] = reinterpret_cast<void *>(ring->ElementStride);
    }
}

NET_RING_COLLECTION const *
Queue::GetRingCollection(
    void
) const
{
    return &m_ringCollection;
}

void
Queue::PreAdvancePostDataBuffer(
    void
)
{
    if (m_bufferPool)
    {
        auto br = m_rings[NetRingTypeDataBuffer].get();
        auto const brLastIndex = NetRingAdvanceIndex(br, br->OSReserved0, -1);

        for (; br->EndIndex != brLastIndex;
            br->EndIndex = NetRingIncrementIndex(br, br->EndIndex))
        {
            auto dataBuffer = NetRingGetDataBufferAtIndex(br, br->EndIndex);

            if (! NT_SUCCESS(m_bufferPool->Allocate(dataBuffer)))
            {
                break;
            }

            auto vaLookup = reinterpret_cast<void **>(m_vaLookupTable.get());
            auto bufferVirtualAddress = NetExtensionGetFragmentVirtualAddress(&m_bufferVaExt, br->EndIndex);
            bufferVirtualAddress->VirtualAddress = vaLookup[*dataBuffer];

            auto laLookup = reinterpret_cast<UINT64 *>(m_laLookupTable.get());
            auto bufferLogicalAddress = NetExtensionGetFragmentLogicalAddress(&m_bufferLaExt, br->EndIndex);
            bufferLogicalAddress->LogicalAddress = laLookup[*dataBuffer];
        }
    }
}

void
Queue::PostAdvanceRefCounting(
    void
)
{
    if (m_bufferPool)
    {
        //step 1
        //scan fragment ring to calculate data buffer refcount.
        //this scan doesn't alter the indexes fragment ring
        auto fr = m_rings[NetRingTypeFragment].get();
        auto br = m_rings[NetRingTypeDataBuffer].get();
        auto vaLookupTable = reinterpret_cast<void**>(br->OSReserved2[3]);

        for (auto osNext = fr->OSReserved0; osNext != fr->BeginIndex;
            osNext = NetRingIncrementIndex(fr, osNext))
        {
            auto dataBuffer =
                NetExtensionGetFragmentDataBuffer(
                    &m_fragmentDataBufferExt,
                    osNext);

            size_t dataBufferIndex = reinterpret_cast<size_t>(dataBuffer->Handle);
            m_bufferPool->AddRef(dataBufferIndex);

            auto va =
                NetExtensionGetFragmentVirtualAddress(
                    &m_fragmentVaExt,
                    osNext);

            va->VirtualAddress = vaLookupTable[dataBufferIndex];
        }

        //step 2
        //drain the buffer ring to mark which data buffers being
        //returned by the client driver

        for (; br->OSReserved0 != br->BeginIndex;
            br->OSReserved0 = NetRingIncrementIndex(br, br->OSReserved0))
        {
            auto bufferVirtualAddress = NetExtensionGetFragmentVirtualAddress(&m_bufferVaExt, br->OSReserved0);
            bufferVirtualAddress->VirtualAddress = nullptr;

            auto bufferLogicalAddress = NetExtensionGetFragmentLogicalAddress(&m_bufferLaExt, br->OSReserved0);
            bufferLogicalAddress->LogicalAddress = 0;

            size_t * dataBufferHandle =
                reinterpret_cast<size_t *>(NetRingGetElementAtIndex(br, br->OSReserved0));

            m_bufferPool->OwnedByOs(*dataBufferHandle);
        }
    }
}

void
Queue::PostStopReclaimDataBuffers(
    void
)
{
    if (m_bufferPool)
    {
        auto br = m_rings[NetRingTypeDataBuffer].get();
        NT_FRE_ASSERT(br->BeginIndex == br->OSReserved0);

        for (; br->BeginIndex != br->EndIndex;
             br->OSReserved0 = br->BeginIndex = NetRingIncrementIndex(br, br->BeginIndex))
        {
            auto const index = NetRingGetDataBufferAtIndex(br, br->BeginIndex);

            m_bufferPool->OwnedByOs(*index);
            m_bufferPool->Free(*index);
        }
    }
}
