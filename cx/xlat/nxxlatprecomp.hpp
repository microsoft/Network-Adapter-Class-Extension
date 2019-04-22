// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

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
#  define NDIS682 1
#  include <ndis.h>
#else
#  include "umwdm.h"
#  define UM_NDIS682 1
#  include <ntddndis.h>

#  define NDIS_STATUS_SUCCESS                     ((NDIS_STATUS)STATUS_SUCCESS)
#  define NDIS_STATUS_PAUSED                      ((NDIS_STATUS)STATUS_NDIS_PAUSED)

typedef PVOID NDIS_HANDLE;
typedef PHYSICAL_ADDRESS NDIS_PHYSICAL_ADDRESS, *PNDIS_PHYSICAL_ADDRESS;
#  define NOTHING
#  define EXPORT extern "C"

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

#define NETCX_ADAPTER_2
#include <net/extension.h>
#include <net/fragment.h>
#include <net/packet.h>
#include <net/ring.h>
#include <net/ringcollection.h>

#include <NetPacketPool.h>

#include <KNew.h>
#include <KMacros.h>
#include <KPtr.h>
#include <KIntrusiveList.h>
#include <KArray.h>

#include <NetPacketExtensionPrivate.h>

MAKE_INTRUSIVE_LIST_ENUMERABLE(MDL, Next);
MAKE_INTRUSIVE_LIST_ENUMERABLE(NET_BUFFER, Next);
MAKE_INTRUSIVE_LIST_ENUMERABLE(NET_BUFFER_LIST, Next);
