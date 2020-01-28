// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

namespace NetCx {

namespace Core {

class Queue
{
public:

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

    Queue();

    NTSTATUS
    Initialize(
        _In_ Rtl::KArray<NET_EXTENSION_PRIVATE>& Extensions,
        _In_ UINT32 NumberOfPackets,
        _In_ UINT32 NumberOfFragments,
        _In_ UINT32 NumberOfDataBuffers,
        _In_ BufferPool* BufferPool
    );

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
        m_PacketLayout;

    NxExtensionLayout
        m_FragmentLayout;

    KPoolPtr<NET_RING>
        m_Rings[NetRingTypeDataBuffer + 1];

    NET_RING_COLLECTION
        m_RingCollection;

private:

    NET_RING
        m_ShadowRings[NetRingTypeDataBuffer + 1];

    BufferPool*
        m_BufferPool = nullptr;

    KPoolPtrNP<BYTE>
        m_LaLookupTable;

    KPoolPtrNP<BYTE>
        m_VaLookupTable;

    NET_EXTENSION
        m_fragmentDataBufferExt;

    NET_EXTENSION
        m_fragmentVaExt;
};

} //namespace Core
} //namespace NetCx
