// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

class BufferPool;

namespace NetCx::Core {

class Queue
{
public:

    enum class Type
    {
        Rx,
        Tx,
    };

    Queue(
        void
    );

    NTSTATUS
    Initialize(
        _In_ Rtl::KArray<NET_EXTENSION_PRIVATE> & Extensions,
        _In_ UINT32 NumberOfPackets,
        _In_ UINT32 NumberOfFragments,
        _In_ BOOLEAN CreateReturnContextRing
    );

    void
    GetExtension(
        _In_ const NET_EXTENSION_PRIVATE * ExtensionToQuery,
        _Out_ NET_EXTENSION* Extension
    ) const;

#ifdef NETCX_UMDF
    void
    GetAllExtensions(
        _Out_ NET_EXTENSION_PRIVATE ** PacketExtensions,
        _Out_ size_t * PacketExtensionsCount,
        _Out_ NET_EXTENSION_PRIVATE ** FragmentExtensions,
        _Out_ size_t * FragmentExtensionsCount
    );
#endif

    NET_RING_COLLECTION const *
    GetRingCollection(
        void
    ) const;

private:

    NTSTATUS
    CreateRing(
        _In_ size_t ElementSize,
        _In_ UINT32 ElementCount,
        _In_ NET_RING_TYPE RingType
    );

protected:

    NxExtensionLayout
        m_packetLayout;

    NxExtensionLayout
        m_fragmentLayout;

    NxExtensionLayout
        m_netFragmentReturnContextLayout;

    KPoolPtr<NET_RING>
        m_rings[NetRingTypeDataBuffer + 1];

    NET_RING_COLLECTION
        m_ringCollection;

private:

    NET_EXTENSION
        m_fragmentVaExt;
};

} // namespace NetCx::Core
