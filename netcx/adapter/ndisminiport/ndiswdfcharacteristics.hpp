// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

_IRQL_requires_(PASSIVE_LEVEL)
void
NdisWdfCxCharacteristicsInitialize(
    _Out_ NDIS_WDF_CX_CHARACTERISTICS & Chars
);
