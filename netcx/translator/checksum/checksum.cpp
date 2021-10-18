// Copyright (C) Microsoft Corporation. All rights reserved.

#include <ndis.h>

#include "Checksum.hpp"

// Rx

// Depending on the packet contents, one of the following 3 functions will
// be called from NxRxXlat::TransferDataBufferFromNetPacketToNbl() to validate
// the packet and discard if necessary. We will do this only if the NET_PACKET
// checksum is NetPacketRxChecksumEvaluationNotChecked and the checksum offload
// is requested.

_Use_decl_annotations_
bool
Checksum::IsIpv4ChecksumValid(
    UINT8 const * Buffer,
    size_t Length
)
{
    // Km: Call SegLibPvtChecksumBuffer() for validation of Rx packet
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);

    return true;
}

_Use_decl_annotations_
bool
Checksum::IsUdpChecksumValid(
    UINT8 const * Buffer,
    size_t Length
)
{
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);

    return true;
}

_Use_decl_annotations_
bool
Checksum::IsTcpChecksumValid(
    UINT8 const * Buffer,
    size_t Length
)
{
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);

    return true;
}
