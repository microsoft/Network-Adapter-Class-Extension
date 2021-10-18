// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Shared header for the entire XLAT app.

--*/

#pragma once

#include <ntstrsafe.h>

#include <NxTrace.hpp>
#include <NxXlatTraceLogging.hpp>

#include "NetClientApi.h"
#include "NxApp.hpp"

#define MS_TO_100NS_CONVERSION 10000


#define NBL_SEND_COMPLETION_BATCH_SIZE 64

// both TCPIP and NWIFI currently process NBLs in batch of 32
// so we use the same batch size here too for now. In the future,
// this can be integrated into EC tunable
#define NBL_RECEIVE_BATCH_SIZE 32

// we cap the total number of bouce buffers preallocated if the NIC
// supports larger than standard packet size, e.g. jumbo frame, LSO
#define MAX_NUMBER_OF_BOUCE_BUFFERS_FOR_LARGE_PACKETS 256
