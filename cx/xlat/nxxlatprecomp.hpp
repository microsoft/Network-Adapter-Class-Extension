/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxXlatPrecomp.hpp

Abstract:

    Precompiled header.

    Do not put any local header files in here.
    (Local header files change more often, which makes it more likely you'll
    need to recompile the pch to see those changes.)

--*/

#pragma once

#if _KERNEL_MODE
#  include <Ntddk.h>
#  include <wdm.h>
#  define NDIS670 1
#  include <ndis.h>
#else
#  include "umwdm.h"
#  define UM_NDIS670 1
#  include <ntddndis.h>

#  define NDIS_STATUS_SUCCESS                     ((NDIS_STATUS)STATUS_SUCCESS)
#  define NDIS_STATUS_PAUSED                      ((NDIS_STATUS)STATUS_NDIS_PAUSED)

typedef PVOID NDIS_HANDLE;
typedef PHYSICAL_ADDRESS NDIS_PHYSICAL_ADDRESS, *PNDIS_PHYSICAL_ADDRESS;
#  define NOTHING
#  define EXPORT extern "C" __declspec(dllimport)

#  include <ndisbuf.h>
#  include <ndistoe.h>

extern "C"
__declspec(dllimport)
VOID
NdisMSendNetBufferListsComplete(
    __in NDIS_HANDLE              MiniportAdapterHandle,
    __in PNET_BUFFER_LIST         NetBufferList,
    __in ULONG                    SendCompleteFlags
);

extern "C"
__declspec(dllimport)
VOID
NdisMIndicateReceiveNetBufferLists(
    __in  NDIS_HANDLE             MiniportAdapterHandle,
    __in  PNET_BUFFER_LIST        NetBufferLists,
    __in  NDIS_PORT_NUMBER        PortNumber,
    __in  ULONG                   NumberOfNetBufferLists,
    __in  ULONG                   ReceiveFlags
);

#endif


#include <ntstatus.h>
#include <ntintsafe.h>
#include <ntassert.h>

#include <nblutil.h>
#include <netiodef.h>

#define NET_ADAPTER_CX_1_0 1
#include <NetPacket.h>
#include <NetPacketPool.h>
#include <NetRingBuffer.h>

#include <KNew.h>
#include <KMacros.h>
#include <KPtr.h>
#include <KIntrusiveList.h>

MAKE_INTRUSIVE_LIST_ENUMERABLE(MDL, Next);
MAKE_INTRUSIVE_LIST_ENUMERABLE(NET_BUFFER, Next);
MAKE_INTRUSIVE_LIST_ENUMERABLE(NET_BUFFER_LIST, Next);
