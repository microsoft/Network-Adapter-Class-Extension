// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:

    Shared header for the entire XLAT app.

--*/

#pragma once

#include <NxTrace.hpp>
#include <NxXlatTraceLogging.hpp>

#include "NetClientApi.h"
#include "NxApp.hpp"

#define MS_TO_100NS_CONVERSION 10000

#ifndef RTL_IS_POWER_OF_TWO
#  define RTL_IS_POWER_OF_TWO(Value) \
    ((Value != 0) && !((Value) & ((Value) - 1)))
#endif

