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
        _In_ UINT32 NumberOfDataBuffers,
        _In_ BufferPool * BufferPool
    );

    void
    GetExtension(
        _In_ const NET_EXTENSION_PRIVATE * ExtensionToQuery,
        _Out_ NET_EXTENSION* Extension
    ) const;

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

    static
    void
    EnumerateCallback(
        _In_ PVOID Context,
        _In_ SIZE_T Index,
        _In_ UINT64 LogicalAddress,
        _In_ PVOID VirtualAddress
    );

protected:

    void
    PreAdvancePostDataBuffer(
        void
    );

    void
    PostAdvanceRefCounting(
        void
    );

    void
    PostStopReclaimDataBuffers(
        void
    );

protected:

    NxExtensionLayout
        m_packetLayout;

    NxExtensionLayout
        m_fragmentLayout;

    NxExtensionLayout
        m_bufferLayout;

    KPoolPtr<NET_RING>
        m_rings[NetRingTypeDataBuffer + 1];

    NET_RING_COLLECTION
        m_ringCollection;

private:

    NET_RING
        m_shadowRings[NetRingTypeDataBuffer + 1];

    BufferPool *
        m_bufferPool = nullptr;

    KPoolPtrNP<BYTE>
        m_laLookupTable;

    KPoolPtrNP<BYTE>
        m_vaLookupTable;

    NET_EXTENSION
        m_fragmentDataBufferExt;

    NET_EXTENSION
        m_fragmentVaExt;

    NET_EXTENSION
        m_bufferVaExt;

    NET_EXTENSION
        m_bufferLaExt;
};

} // namespace NetCx::Core
