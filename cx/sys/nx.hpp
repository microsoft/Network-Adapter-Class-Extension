/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    Nx.hpp

Abstract:

    This is the main Ndis class extension library include file.


    
Environment:

    kernel mode only

Revision History:

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
#include "NetPacketPool.h"
#include "NetPacketLibrary.h"

// Since we're hiding this header from clients, we
// need to include it directly
#include <netadaptercx_p\1.0\netobject.h>

#include <ndis_p.h>
#include <ndiswdf.h>

#include <WppRecorder.h>

#include "NxMacros.hpp"

#include "NxDynamics.h"

}

#include <NxXlat.hpp>

#include <KNew.h>

#include "StateMachineEngine.h"

#include "nxtrace.hpp"

#include "NxForward.hpp"

#include "fxobject.h"

#include "NxUtility.hpp"

#include "version.hpp"

#include "verifier.hpp"

#include "NxDriver.hpp"

#include "NxBuffer.hpp"

#include "NxCommonBufferAllocation.hpp"

#include "NxPoolAllocation.hpp"

#include "NxQueue.hpp"

#include "nxadapterstatemachine_autogen.hpp"

#include "NxAdapter.hpp"

#include "nxdevicestatemachine_autogen.hpp"

#include "NxDevice.hpp"

#include "NxConfiguration.hpp"

#include "NxApp.hpp"

#include "NxRequest.hpp"

#include "NxRequestQueue.hpp"

#include "NxWake.hpp"

#include "nxtracelogging.hpp"

DECLARE_SM_ENGINE_EVENT_ENUM(NxDeviceSM_EVENT NxAdapterSM_EVENT )

DECLARE_SM_ENGINE_STATE_ENUM(NxDeviceSM_STATE NxAdapterSM_STATE)

#endif // _NX_H
