// Copyright (C) Microsoft Corporation. All rights reserved.

#include <ndis.h>

#include "Segmentation.hpp"
#include "../NxTxNblContext.hpp"


_IRQL_requires_(PASSIVE_LEVEL)
void
MarkNetBufferListSoftwareSegmented(
    _Inout_ NET_BUFFER_LIST * NetBufferList,
    _In_ ULONG DataOffset,
    _In_ NDIS_NET_BUFFER_LIST_INFO OffloadType
)
/*++

Routine Description:

    Mark NetBufferList as segmented, store info needed for later freeing NetBufferList,
    and clear segmented NBL's LSO/USO info.

--*/
{
    NT_FRE_ASSERT(
        OffloadType == TcpLargeSendNetBufferListInfo ||
        OffloadType == UdpSegmentationOffloadInfo);

    auto gsoNblContext = GetGsoContextFromNetBufferList(NetBufferList);
    gsoNblContext->Segmented = true;
    gsoNblContext->DataOffset = DataOffset;

    if (OffloadType == TcpLargeSendNetBufferListInfo)
    {
        NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO clearLsoInfo = {};
        NetBufferList->NetBufferListInfo[TcpLargeSendNetBufferListInfo] = clearLsoInfo.Value;
    }
    else
    {
        NDIS_UDP_SEGMENTATION_OFFLOAD_NET_BUFFER_LIST_INFO clearUsoInfo = {};
        NetBufferList->NetBufferListInfo[UdpSegmentationOffloadInfo] = clearUsoInfo.Value;
    }
}
