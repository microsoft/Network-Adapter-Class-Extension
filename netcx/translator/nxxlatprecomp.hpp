// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Precompiled header.

    Do not put any local header files in here.
    (Local header files change more often, which makes it more likely you'll
    need to recompile the pch to see those changes.)

--*/

#pragma once

#ifdef _KERNEL_MODE
#include <ntosp.h>
#include <zwapi.h>
#include <ntassert.h>
#else
#include <nt.h>
#include <ntintsafe.h>
#include <ntrtl.h>
#include <nturtl.h>
#endif // _KERNEL_MODE

#ifdef _KERNEL_MODE
#include <ndis.h>
#include <ndis_p.h>
#else
#include "umwdm.h"
#include <ntddndis.h>
#endif // _KERNEL_MODE

#include <ndis/types.h>

#ifndef _KERNEL_MODE
typedef PHYSICAL_ADDRESS NDIS_PHYSICAL_ADDRESS, *PNDIS_PHYSICAL_ADDRESS;
#include <ndis/nbl.h>
#include <ndis/nblapi.h>
#include <ndis/nblaccessors.h>
#endif // _KERNEL_MODE

#include <KNew.h>
#include <KMacros.h>
#include <KPtr.h>
#include <KIntrusiveList.h>
#include <KArray.h>

