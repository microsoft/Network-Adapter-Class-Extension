// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    This is the main Ndis class extension library include file.

--*/

#pragma once

#include <ntddk.h>
#include <wdm.h>
#include <ntassert.h>
#include <wdmsec.h>
#include <initguid.h>
#include <wdmguid.h>

#include <ntstrsafe.h>
#include <ntintsafe.h>

#include <wdf.h>
#include <wdfcx.h>
#include <wdfcxbase.h>
#include <wdfldr.h>

#include <wil/resource.h>

#include <preview/netadaptercx.h>

#include <net/ring.h>
#include <netpacketpool.h>

#include <netadapter_p.h>
#include <netpacketextension_p.h>
#include <netadapterextension_p.h>
#include <netdevice_p.h>

#include <ndis_p.h>
#include <ndiswdf.h>

#include <KMacros.h>

#include <NxTrace.hpp>
#include <NxTraceLogging.hpp>

