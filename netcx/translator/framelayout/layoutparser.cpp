// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

#include <ntassert.h>

#include "LayoutParser.hpp"

#include "Layer2Layout.hpp"
#include "Layer3Layout.hpp"
#include "Layer4Layout.hpp"

FUNC_PARSE_LAYER_LAYOUT* const ParseLayerDispatch[3] = {
    Layer2::ParseLayout,
    Layer3::ParseLayout,
    Layer4::ParseLayout
};
