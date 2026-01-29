// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <KNew.h>

template<ULONG TAG>
using NxPagedAllocation = KALLOCATOR<TAG>;

template<ULONG TAG>
using NxNonpagedAllocation = KALLOCATOR_NONPAGED<TAG>;

