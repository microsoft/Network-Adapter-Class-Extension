// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#ifdef _KERNEL_MODE
#include <ndis/lwminiporttypes.h>
#else
struct NDIS_MINIPORT_DRIVER_CHARACTERISTICS;
#endif

_IRQL_requires_(PASSIVE_LEVEL)
void
NdisMiniportDriverCharacteristicsInitialize(
    _Out_ NDIS_MINIPORT_DRIVER_CHARACTERISTICS & Characteristics
);

_IRQL_requires_(PASSIVE_LEVEL)
void
NdisLWMiniportDriverCharacteristicsInitialize(
    _Out_ NDIS_LWM_DRIVER_CHARACTERISTICS & Characteristics
);
