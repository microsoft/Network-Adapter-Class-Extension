// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <net/packet.h>

#ifdef _KERNEL_MODE

#include <BatchingLib.h>
#include <SegLib.h>

#else

// Define mock structures for usermode.
struct BATCHING_LIB_CONTEXT
{
};

struct SEGLIB_CONTEXT
{
};

#endif

struct _NET_BUFFER_LIST;
typedef struct _NET_BUFFER_LIST NET_BUFFER_LIST;

// Wraps the segmentation operations performed by SegLib for both TCP (LSO) and UDP (USO).
// Plus helpers for translator to handle LSOed and USOed NBLs.
class GenericSegmentationOffload
{

public:

    _IRQL_requires_(PASSIVE_LEVEL)
    GenericSegmentationOffload::GenericSegmentationOffload(
        void
    );

    _IRQL_requires_(PASSIVE_LEVEL)
    ~GenericSegmentationOffload(
        void
    );

    // Perform GSO (LSO or USO) WITHOUT checksum, returns a duplicated and segmented NBL.
    // OutputNbl's LSO/USO info are cleared comparing to SourceNbl.
    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    PerformLso(
        _In_ NET_BUFFER_LIST const * SourceNbl,
        _In_ NET_PACKET_LAYOUT const & PacketLayout,
        _Out_ NET_BUFFER_LIST * & OutputNbl
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    PerformUso(
        _In_ NET_BUFFER_LIST const * SourceNbl,
        _In_ NET_PACKET_LAYOUT const & PacketLayout,
        _Out_ NET_BUFFER_LIST * & OutputNbl
    );

    // Free the duplicated and segmented NBL returned from PerformGso.
    _IRQL_requires_(PASSIVE_LEVEL)
    void
    FreeSoftwareSegmentedNetBufferList(
        _In_ NET_BUFFER_LIST * NetBufferList
    );

private:

    _IRQL_requires_max_(DISPATCH_LEVEL)
    NTSTATUS
    PerformGso(
        _In_ NET_BUFFER_LIST const * SourceNbl,
        _In_ NET_PACKET_LAYOUT const & PacketLayout,
        _In_ ULONG RequestedOffload,
        _Out_ NET_BUFFER_LIST * & OutputNbl
    );

    SEGLIB_CONTEXT
        m_segLibContext = {};

    // The NPAGED_LOOKASIDE_LIST field in BATCHING_LIB_CONTEXT_INTERNAL struct
    // must meet MEMORY_ALLOCATION_ALIGNMENT required by InitializeSListHead.
    // This alignas works because NPAGED_LOOKASIDE_LIST's offset is 0x40.
    alignas(MEMORY_ALLOCATION_ALIGNMENT)
    BATCHING_LIB_CONTEXT
        m_batchingLibContext = {};

    // Number of live software segmented NBLs.
    // Used for signaling invalid freeing or NBL leaking.
    size_t
        m_nblRefcount = 0U;

};
