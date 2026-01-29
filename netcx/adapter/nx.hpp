// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main Ndis class extension library include file.

--*/

#pragma once

#ifdef _KERNEL_MODE
#include <ntosp.h>
#include <zwapi.h>
#include <wdmsec.h>
#include <ntstrsafe.h>
#include <ntintsafe.h>
#else
#include <windows.h>
// We can't include ntrtl_x.h without causing issues with WudfWdm.h
#define ASSERT
#include <integermacro.h>
#endif

#include <initguid.h>
#include <wdmguid.h>
#include <ntassert.h>

#include <wdf.h>

#if NETCX_UMDF
#include <WudfIrp.h>
#endif

#include <wdfcx.h>
#include <wdfcxbase.h>

#ifndef _KERNEL_MODE
#include <cfgmgr32.h>
#include <bugcodes.h>
#include <umpool.h>
#include <ntddndis.h>
#include <ntddndis_p.h>
#include <ndis/types.h>
#include <ndis/status.h>
#include <ndis/oidrequest.h>
#include <ndis/nbl.h>
#include <ndis/properties.h>
#include <ndis/wifiattributes.h>
#include <ndis/miniportgeneralattributes.h>
#include <WudfSystem.h>
#include <ndis/configurationparameter.h>
#include <WudfNdisLwm.h>
#include <WudfNdisStatus.h>
#include <WudfNdisStatusIndication.h>
#include <WudfNdisWdf.h>
#include <WudfResetInterface.h>
#include <WudfWmi.h>
#else // _KERNEL_MODE
#include <wdfldr.h>
#endif // _KERNEL_MODE

#include <wil/resource.h>

#include <netadaptercx.h>

#include <net/ring.h>

#include <netadapter_p.h>
#include <netadapterextension_p.h>
#ifdef _KERNEL_MODE
#include <netbufferqueue_p.h>
#endif
#include <netdevice_p.h>

#ifdef _KERNEL_MODE
#include <ndis_p.h>
#include <ndis/statusconvert.h>
#include <ndiswdf.h>
#endif

#include <KMacros.h>
#include <knew.h>

#include <NxTrace.hpp>
#include <NxTraceLogging.hpp>

#define NETADAPTERCX_TAG 'xCdN'
