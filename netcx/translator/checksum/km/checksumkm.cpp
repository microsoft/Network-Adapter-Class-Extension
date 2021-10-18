// Copyright (C) Microsoft Corporation. All rights reserved.

#include <ntddk.h>
#include <ndis.h>

#include <NxTrace.hpp>

#include "Checksum.hpp"
#include "ChecksumKm.tmh"

#include <BatchingLib.h>
#include <SegLib.h>
#include <malloc.h>
#include <wil/resource.h>

NTSTATUS
Checksum::Initialize(
    void
)
{
    NT_FRE_ASSERT(m_batchingLibContext == nullptr && m_segLibContext == nullptr);

    auto blContext = reinterpret_cast<BATCHING_LIB_CONTEXT *>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(BATCHING_LIB_CONTEXT), 'kCxN'));
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !blContext);

    auto slContext = reinterpret_cast<SEGLIB_CONTEXT *>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(SEGLIB_CONTEXT), 'kCxN'));
    if (!slContext)
    {
        ExFreePool(blContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    BLInitialize(blContext);
    m_batchingLibContext = reinterpret_cast<CHECKSUMLIB_BL_HANDLE>(blContext);
    SegLibInitialize(blContext, slContext);
    m_segLibContext = reinterpret_cast<CHECKSUMLIB_SL_HANDLE>(slContext);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
Checksum::CalculateChecksum(
    NET_BUFFER * Buffer,
    NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO const & ChecksumInfo,
    NET_PACKET_LAYOUT const & PacketLayout
)
{
    BATCHING_BUFFER_CONTEXT context;

    BLInitializeBatchOpContext(reinterpret_cast<BATCHING_LIB_CONTEXT *>(m_batchingLibContext),
        &context,
        FALSE,
        FALSE,
        NULL);

    UINT8 const layer3HeaderOffset = PacketLayout.Layer2HeaderLength;
    UINT16 const layer4HeaderOffset = layer3HeaderOffset + PacketLayout.Layer3HeaderLength;

    // Update checksum in the headers.
    NTSTATUS status = SegLibDeferredChecksumPacket(&context,
        Buffer,
        ChecksumInfo,
        layer3HeaderOffset,
        layer4HeaderOffset,
        ChecksumInfo.Transmit.IsIPv6);

    BLFlushBatchOpContext(&context);

    return status;
}

Checksum::~Checksum(
    void
)
{
    if (m_segLibContext)
    {
        auto slContext = reinterpret_cast<SEGLIB_CONTEXT *>(m_segLibContext);
        SegLibUninitialize(slContext);
        ExFreePool(slContext);
    }

    if (m_batchingLibContext)
    {
        auto blContext = reinterpret_cast<BATCHING_LIB_CONTEXT *>(m_batchingLibContext);
        BLUninitialize(blContext);
        ExFreePool(blContext);
    }
}
