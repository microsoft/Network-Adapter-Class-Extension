// Copyright (C) Microsoft Corporation. All rights reserved.

#ifdef _KERNEL_MODE
#include <ntosp.h>
#include <wdf.h>
#include <netadaptercx.h>
#else
#include <umwdm.h>
#include <KPtr.h>
#include <umwdf.h>
#include <NdisUm.h>
#undef NETCX_ADAPTER_2
#include <netadaptercx.h>
#endif

#include <net/ring.h>
#include <net/virtualaddress_p.h>
#include <net/databuffer_p.h>
#include <KPtr.h>
#include "adapter/extension/NxExtensionLayout.hpp"
#include "NetClientBuffer.h"
#include "bm/BufferPool.hpp"

#include <NxTrace.hpp>
//#include <NxTraceLogging.hpp>

#include "Queue.hpp"
#include "Queue.tmh"


namespace NetCx {

namespace Core {

Queue::Queue(
    void
)
    : m_PacketLayout(sizeof(NET_PACKET), alignof(NET_PACKET))
    , m_FragmentLayout(sizeof(NET_FRAGMENT), alignof(NET_FRAGMENT))
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

    RtlCopyMemory(&m_ShadowRings[RingType], ring, FIELD_OFFSET(NET_RING, Buffer[0]));

    m_Rings[RingType].reset(ring);
    m_RingCollection.Rings[RingType] = m_Rings[RingType].get();

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
    Queue* queue = reinterpret_cast<Queue*>(Context);

    auto laLookupTable = 
        reinterpret_cast<UINT64*>(queue->m_Rings[NetRingTypeDataBuffer]->OSReserved2[2]);
    auto vaLookupTable = 
        reinterpret_cast<void**>(queue->m_Rings[NetRingTypeDataBuffer]->OSReserved2[3]);

    laLookupTable[Index] = LogicalAddress;
    vaLookupTable[Index] = VirtualAddress;
}

NTSTATUS
Queue::Initialize(
    _In_ Rtl::KArray<NET_EXTENSION_PRIVATE> & Extensions,
    _In_ UINT32 NumberOfPackets,
    _In_ UINT32 NumberOfFragments,
    _In_ UINT32 NumberOfDataBuffers,
    _In_ BufferPool* BufferPool
)
{
    for (auto const & extension : Extensions)
    {
        switch (extension.Type)
        {
        case NetExtensionTypePacket:
            CX_RETURN_IF_NOT_NT_SUCCESS(
                m_PacketLayout.PutExtension(
                    extension.Name,
                    extension.Version,
                    extension.Type,
                    extension.Size,
                    extension.NonWdfStyleAlignment));

            break;

        case NetExtensionTypeFragment:
            CX_RETURN_IF_NOT_NT_SUCCESS(
                m_FragmentLayout.PutExtension(
                    extension.Name,
                    extension.Version,
                    extension.Type,
                    extension.Size,
                    extension.NonWdfStyleAlignment));
            break;
        }
    }

    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateRing(
            m_FragmentLayout.Generate(),
            NumberOfFragments,
            NetRingTypeFragment));

    CX_RETURN_IF_NOT_NT_SUCCESS(
        CreateRing(
            m_PacketLayout.Generate(),
            NumberOfPackets,
            NetRingTypePacket));

    if (BufferPool)
    {
        m_BufferPool = BufferPool;

        CX_RETURN_IF_NOT_NT_SUCCESS(
            CreateRing(
                sizeof(size_t),
                NumberOfDataBuffers,
                NetRingTypeDataBuffer));

        m_LaLookupTable =
            MakeSizedPoolPtrNP<BYTE>('nxbp', sizeof(UINT64) * NumberOfDataBuffers);
        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                            ! m_LaLookupTable);

        m_VaLookupTable =
            MakeSizedPoolPtrNP<BYTE>('nxbp', sizeof(void*) * NumberOfDataBuffers);
        CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES,
                            ! m_VaLookupTable);

        m_Rings[NetRingTypeDataBuffer]->OSReserved2[2] = 
            m_LaLookupTable.get();

        m_Rings[NetRingTypeDataBuffer]->OSReserved2[3] = 
            m_VaLookupTable.get();

        m_BufferPool->Enumerate(EnumerateCallback, this);

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
    }
    else
    {
        m_Rings[NetRingTypeDataBuffer].reset(nullptr);
        m_RingCollection.Rings[NetRingTypeDataBuffer] = nullptr;
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
        extension = m_PacketLayout.GetExtension(
            ExtensionToQuery->Name,
            ExtensionToQuery->Version,
            ExtensionToQuery->Type);

        break;

    case NetExtensionTypeFragment:
        type = NetRingTypeFragment;
        extension = m_FragmentLayout.GetExtension(
            ExtensionToQuery->Name,
            ExtensionToQuery->Version,
            ExtensionToQuery->Type);

        break;

    }

    Extension->Enabled = extension != nullptr;
    if (Extension->Enabled)
    {
        auto const ring = m_RingCollection.Rings[type];
        Extension->Reserved[0] = ring->Buffer + extension->AssignedOffset;
        Extension->Reserved[1] = reinterpret_cast<void *>(ring->ElementStride);
    }
}

NET_RING_COLLECTION const *
Queue::GetRingCollection(
    void
) const
{
    return &m_RingCollection;
}

void
Queue::PreAdvancePostDataBuffer(
    void
)
{
    if (m_BufferPool)
    {
        auto br = m_Rings[NetRingTypeDataBuffer].get();
        auto const brLastIndex = (br->OSReserved0 - 1) & br->ElementIndexMask;

        for (; br->EndIndex != brLastIndex;
            br->EndIndex = NetRingIncrementIndex(br, br->EndIndex))
        {
            size_t* dataBufferIndex = 
                reinterpret_cast<size_t*>(NetRingGetElementAtIndex(br, br->EndIndex));

            if (!NT_SUCCESS(m_BufferPool->Allocate(dataBufferIndex)))
            {
                break;
            }
        }
    }
}

void
Queue::PostAdvanceRefCounting(
    void
)
{
    if (m_BufferPool)
    {
        //step 1
        //scan fragment ring to calculate data buffer refcount.
        //this scan doesn't alter the indexes fragment ring
        auto fr = m_Rings[NetRingTypeFragment].get();
        auto br = m_Rings[NetRingTypeDataBuffer].get();
        auto vaLookupTable = reinterpret_cast<void**>(br->OSReserved2[3]);

        for (auto osNext = fr->OSReserved0; 
            osNext != fr->BeginIndex;
            osNext = NetRingIncrementIndex(fr, osNext))
        {
            auto dataBuffer = 
                NetExtensionGetFragmentDataBuffer(
                    &m_fragmentDataBufferExt, 
                    osNext);

            size_t dataBufferIndex = reinterpret_cast<size_t>(dataBuffer->Handle);
            m_BufferPool->AddRef(dataBufferIndex);

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
            size_t* dataBufferHandle = 
                reinterpret_cast<size_t*>(NetRingGetElementAtIndex(br, br->OSReserved0));

            m_BufferPool->OwnedByOs(*dataBufferHandle);
        }
    }
}

void
Queue::PostStopReclaimDataBuffers(
    void
)
{
    if (m_BufferPool)
    {
        auto br = m_Rings[NetRingTypeDataBuffer].get();
        NT_FRE_ASSERT(br->BeginIndex == br->OSReserved0);
    
        for (; br->BeginIndex != br->EndIndex;
             br->OSReserved0 = br->BeginIndex = NetRingIncrementIndex(br, br->BeginIndex))
        {
            size_t* dataBufferHandle = 
                reinterpret_cast<size_t*>(NetRingGetElementAtIndex(br, br->BeginIndex));

            m_BufferPool->OwnedByOs(*dataBufferHandle);
            m_BufferPool->Free(*dataBufferHandle);
        }
    }
}

} //namespace Core
} //namespace NetCx
