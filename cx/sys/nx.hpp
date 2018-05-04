// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main Ndis class extension library include file.

--*/

#ifndef _NX_H
#define _NX_H

extern "C" {

#include <Ntddk.h>
#include <wdm.h>
#include <ntassert.h>
#include <wdmsec.h>
#include <wdmguid.h>

#include <ntstrsafe.h>
#include <ntintsafe.h>

#include <wdf.h>
#include <wdfcx.h>
#include <wdfcxbase.h>
#include <wdfldr.h>

#include "netadaptercx.h"
#include "NetRingBuffer.h"
#include "NetPacketPool.h"
#include "NetPacketLibrary.h"

// Since we're hiding this header from clients, we
// need to include it directly
#include <netadaptercx_p\1.1\netobject.h>
#include <netadaptercx_p\1.2\netpacketextension_p.h>
#include <netadaptercx_p\1.2\netrssqueuegroup_p.h>

#include <ndis_p.h>
#include <ndiswdf.h>

#include <WppRecorder.h>

#define NETEXPORT(a) imp_ ## a

#include "NxDynamics.h"

}

#include "NxMacros.hpp"

#include <wil\resource.h>

#include <NetClientApi.h>

#include <NxXlat.hpp>

#include <KNew.h>

#include <KArray.h>

#include <smfx.h>

#include <NxApp.hpp>

#include <NxTrace.hpp>
#include <NxTraceLogging.hpp>

#include "NxForward.hpp"

#include "fxobject.h"

#include "NxUtility.hpp"

#include "version.hpp"

#include "NxDriver.hpp"

#include "NxBuffer.hpp"

#include "NxQueue.hpp"

#include "NxQueueGroup.hpp"

#include "NxAdapterStateMachine.h"

#include "NxAdapter.hpp"

#include "NxDeviceStateMachine.h"

#include "NxAdapterCollection.hpp"

#include "NxDevice.hpp"

#include "NxConfiguration.hpp"

#include "NxRequest.hpp"

#include "NxRequestQueue.hpp"

#include "NxWake.hpp"

#include "verifier.hpp"

#endif // _NX_H
