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

#include <wil\resource.h>

#include <netadaptercx.h>
#include <netringbuffer.h>
#include <netpacketpool.h>
#include <netpacketlibrary.h>

// Since we're hiding this header from clients, we
// need to include it directly
#include <netadaptercx_p\1.2\netpacketextension_p.h>
#include <netadaptercx_p\1.3\netadapterextension.h>
#include <netadaptercx_p\1.3\netdevice_p.h>

#include <ndis_p.h>
#include <ndiswdf.h>

#include <KMacros.h>

#include <NxTrace.hpp>
#include <NxTraceLogging.hpp>

