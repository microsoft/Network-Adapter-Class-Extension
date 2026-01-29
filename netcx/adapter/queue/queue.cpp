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
#include <net/logicaladdress_p.h>
#include <net/returncontexttypes.h>
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
    , m_netFragmentReturnContextLayout(sizeof(NET_FRAGMENT_RETURN_CONTEXT), alignof(NET_FRAGMENT_RETURN_CONTEXT))
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
    ring->OSReserved2[2] = (PVOID) size;
    ring->ElementStride = static_cast<USHORT>(ElementSize);
    ring->NumberOfElements = ElementCount;
    ring->ElementIndexMask = ElementCount - 1;

    m_rings[RingType].reset(ring);
    m_ringCollection.Rings[RingType] = m_rings[RingType].get();

    return STATUS_SUCCESS;
}

NTSTATUS
Queue::Initialize(
    _In_ Rtl::KArray<NET_EXTENSION_PRIVATE> & Extensions,
    _In_ UINT32 NumberOfPackets,
    _In_ UINT32 NumberOfFragments,
    _In_ BOOLEAN CreateReturnContextRing
)
{
    for (auto const& extension : Extensions)
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

        case NetExtensionTypeBuffer:
            CX_RETURN_IF_NOT_NT_SUCCESS(
                m_netFragmentReturnContextLayout.PutExtension(
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

    m_rings[NetRingTypeDataBuffer].reset(nullptr);
    m_ringCollection.Rings[NetRingTypeDataBuffer] = nullptr;

    if (CreateReturnContextRing)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            CreateRing(
                m_netFragmentReturnContextLayout.Generate(),
                NumberOfPackets,
                NetRingTypeDataBuffer));
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
        extension = m_netFragmentReturnContextLayout.GetExtension(
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

#ifdef NETCX_UMDF
void
Queue::GetAllExtensions(
    _Out_ NET_EXTENSION_PRIVATE ** PacketExtensions,
    _Out_ size_t * PacketExtensionsCount,
    _Out_ NET_EXTENSION_PRIVATE ** FragmentExtensions,
    _Out_ size_t * FragmentExtensionsCount
)
{
    auto& packetExtensions = m_packetLayout.GetAllExtensions();
    auto& fragmentExtensions = m_fragmentLayout.GetAllExtensions();

    *PacketExtensions = &packetExtensions[0];
    *PacketExtensionsCount = packetExtensions.count();

    *FragmentExtensions = &fragmentExtensions[0];
    *FragmentExtensionsCount = fragmentExtensions.count();
}
#endif

NET_RING_COLLECTION const *
Queue::GetRingCollection(
    void
) const
{
    return &m_ringCollection;
}
