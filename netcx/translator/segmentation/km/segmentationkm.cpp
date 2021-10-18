// Copyright (C) Microsoft Corporation. All rights reserved.

#include <ndis.h>
#include <NxTrace.hpp>
#include <wil/common.h>

#include "Segmentation.hpp"
#include "../NxTxNblContext.hpp"

#include "SegmentationKm.tmh"

// Forward declaration of internal methods.
void
MarkNetBufferListSoftwareSegmented(
    NET_BUFFER_LIST * NetBufferList,
    ULONG DataOffset,
    NDIS_NET_BUFFER_LIST_INFO OffloadType
);

_Use_decl_annotations_
GenericSegmentationOffload::GenericSegmentationOffload(
    void
)
{
    NT_FRE_ASSERT((reinterpret_cast<ULONG_PTR>(&m_batchingLibContext) & (MEMORY_ALLOCATION_ALIGNMENT - 1)) == 0);

    BLInitialize(&m_batchingLibContext);
    SegLibInitialize(&m_batchingLibContext, &m_segLibContext);
}

_Use_decl_annotations_
NTSTATUS
GenericSegmentationOffload::PerformLso(
    NET_BUFFER_LIST const * SourceNbl,
    NET_PACKET_LAYOUT const & PacketLayout,
    NET_BUFFER_LIST * & OutputNbl
)
{
    return PerformGso(SourceNbl, PacketLayout, SEGLIB_OFFLOAD_PERFORM_LSO, OutputNbl);
}

_Use_decl_annotations_
NTSTATUS
GenericSegmentationOffload::PerformUso(
    NET_BUFFER_LIST const * SourceNbl,
    NET_PACKET_LAYOUT const & PacketLayout,
    NET_BUFFER_LIST * & OutputNbl
)
{
    return PerformGso(SourceNbl, PacketLayout, SEGLIB_OFFLOAD_PERFORM_USO, OutputNbl);
}

_Use_decl_annotations_
NTSTATUS
GenericSegmentationOffload::PerformGso(
    NET_BUFFER_LIST const * SourceNbl,
    NET_PACKET_LAYOUT const & PacketLayout,
    ULONG RequestedOffload,
    NET_BUFFER_LIST * & OutputNbl
)
{
    NT_FRE_ASSERT(
        WI_IsSingleFlagSetInMask(
            RequestedOffload,
            SEGLIB_OFFLOAD_PERFORM_LSO | SEGLIB_OFFLOAD_PERFORM_USO));

    UINT8 const layer3HeaderOffset = PacketLayout.Layer2HeaderLength;

    // Setup SEGLIB_OFFLOAD_OVERRIDE_INFO in order NOT to perform checksum.
    // Don't override GSO info; let SegLib use the GSO info in SourceNbl's OOB.
    SEGLIB_CHKSUM_OFFLOAD_INFO checksumOffloadInfo = {};
    checksumOffloadInfo.PerformChecksumOffload = false;

    SEGLIB_OFFLOAD_OVERRIDE_INFO additionalOffloadInfo = {};
    additionalOffloadInfo.OffloadDataSet = SEGLIB_OFFLOAD_OVERRIDE_INFO_CSO_SET;
    additionalOffloadInfo.ChecksumOffloadInfo = checksumOffloadInfo;
    additionalOffloadInfo.NblDirection = SEGLIB_NBL_DIRECTION::TxtoTx;

    ULONG dataOffset = 0U;
    ULONG offloadsPerformed = SEGLIB_OFFLOAD_PERFORM_NONE;

    // This const_cast for SourceNbl is just for adapting to SegLib's API.
    // SourceNbl should NOT be modified throughtout segmentation.
    if (WI_IsFlagSet(RequestedOffload, SEGLIB_OFFLOAD_PERFORM_LSO))
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            SegLibCreateMultiNbNblClone(
                &m_segLibContext,
                const_cast<NET_BUFFER_LIST *>(SourceNbl),
                false,
                layer3HeaderOffset,
                &additionalOffloadInfo,
                nullptr,
                nullptr,
                0,
                0,
                &OutputNbl,
                &dataOffset,
                &offloadsPerformed));
        NT_FRE_ASSERT(WI_IsFlagSet(offloadsPerformed, SEGLIB_OFFLOAD_PERFORM_LSO));
        MarkNetBufferListSoftwareSegmented(OutputNbl, dataOffset, TcpLargeSendNetBufferListInfo);
    }
    else // WI_IsFlagSet(RequestedOffload, SEGLIB_OFFLOAD_PERFORM_USO)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            SegLibCreateMultiNbNblUsoClone(
                &m_segLibContext,
                const_cast<NET_BUFFER_LIST *>(SourceNbl),
                false,
                layer3HeaderOffset,
                &additionalOffloadInfo,
                nullptr,
                nullptr,
                0,
                0,
                &OutputNbl,
                &dataOffset,
                &offloadsPerformed));
        NT_FRE_ASSERT(WI_IsFlagSet(offloadsPerformed, SEGLIB_OFFLOAD_PERFORM_USO));
        MarkNetBufferListSoftwareSegmented(OutputNbl, dataOffset, UdpSegmentationOffloadInfo);
    }

    NT_FRE_ASSERT(WI_IsFlagClear(offloadsPerformed, SEGLIB_OFFLOAD_PERFORM_CSO));
    NT_FRE_ASSERT(OutputNbl->ParentNetBufferList == SourceNbl);

    ++m_nblRefcount;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
GenericSegmentationOffload::FreeSoftwareSegmentedNetBufferList(
    NET_BUFFER_LIST * NetBufferList
)
{
    auto const dataOffset = GetGsoContextFromNetBufferList(NetBufferList)->DataOffset;
    SegLibFreeMultiNbNblClone(NetBufferList, dataOffset);

    NT_FRE_ASSERT(m_nblRefcount > 0U);
    --m_nblRefcount;
}

_Use_decl_annotations_
GenericSegmentationOffload::~GenericSegmentationOffload(
    void
)
{
    SegLibUninitialize(&m_segLibContext);
    BLUninitialize(&m_batchingLibContext);

    NT_FRE_ASSERT(m_nblRefcount == 0U);
}
