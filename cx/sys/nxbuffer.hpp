/*++

Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxBuffer.hpp

Abstract:

    This is the definition of the NxBuffer object.





Environment:

    kernel mode only

Revision History:

--*/

#pragma once

class NxBuffer {

public: 
    
    static
    PVOID
    _GetDefaultClientContext(
        _In_ NET_PACKET *NetPacket
        );

    static
    ULONG_PTR
    _Get802_15_4Info(
        _In_ NET_PACKET *NetPacket
        );

    static
    ULONG_PTR
    _Get802_15_4Status(
        _In_ NET_PACKET *NetPacket
        );

};
