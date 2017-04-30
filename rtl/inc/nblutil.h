/*++
Copyright (c) 2010  Microsoft Corporation

Module Name:

    nblutil.h

Abstract:

    This module defines utility functions for handling NBL chains.

    This module also defines the NBL_QUEUE datastructure, and routines to
    manipulate it.  The NBL_QUEUE contains an ordered list of NBLs.  It is
    designed to allow efficiently appending multiple NBLs.

    Example usage:

    PNET_BUFFER_LIST NblChain1 = //  A=>B=>C=>NULL
    PNET_BUFFER_LIST NblChain2 = //  D=>NULL
    NBL_QUEUE NblQueue;

    ndisInitializeNblQueue(&NblQueue);
    // NblQueue is now empty

    ndisAppendNblChainToNblQueue(&NblQueue, NblChain1);
    // NblQueue now has three NBLs in it: A, B, and C

    ndisAppendNblChainToNblQueue(&NblQueue, NblChain2);
    // NblQueue now has four NBLs in it: A, B, C, and D

    PNET_BUFFER_LIST NblChain3 = ndisPopAllFromNblQueue(&NblQueue);
    // NblQueue is now empty
    // NblChain3 is A=>B=>C=>D=>NULL


Environment:

    Kernel mode

--*/

#pragma once

typedef struct _NBL_QUEUE
{
    // Pointer to first NBL in chain, or NULL if the queue is empty
    NET_BUFFER_LIST *First;

    // Pointer to last NBL in chain, or to this->First if queue is empty
    NET_BUFFER_LIST **Last;
} NBL_QUEUE, *PNBL_QUEUE;


__drv_maxIRQL(DISPATCH_LEVEL)
__inline
ULONG
ndisNumNblsInNblChain(
    __in    PNET_BUFFER_LIST    Nbl)
/*++

Routine Description:

    Returns the number of NBLs in an NBL chain

Arguments:

    NBL

Return Value:

    Number of NBLs the NBL chain

--*/
{
    ULONG Return = 0;

    while (Nbl)
    {
        Return++;
        Nbl = NET_BUFFER_LIST_NEXT_NBL(Nbl);
    }

    return Return;
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
ULONG
ndisNumNbsInNbChain(
    __in    PNET_BUFFER         Nb)
/*++

Routine Description:

    Returns the number of NBs in an NB chain

Arguments:

    NB

Return Value:

    Number of NBs in the NB chain

--*/
{
    ULONG Return = 0;

    while (Nb)
    {
        Return++;
        Nb = NET_BUFFER_NEXT_NB(Nb);
    }

    return Return;
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
ULONG
ndisNumNbsInNblChain(
    __in    PNET_BUFFER_LIST    Nbl)
/*++

Routine Description:

    Returns the number of NBs in each NBL an NBL chain

Arguments:

    NBL

Return Value:

    Number of NBs in the NBL chain

--*/
{
    ULONG Return = 0;

    while (Nbl)
    {
        Return += ndisNumNbsInNbChain(NET_BUFFER_LIST_FIRST_NB(Nbl));
        Nbl = NET_BUFFER_LIST_NEXT_NBL(Nbl);
    }

    return Return;
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
ULONG64
ndisNumDataBytesInNbChain(
    __in    PNET_BUFFER         Nb)
/*++

Routine Description:

    Returns the number of bytes of data in each NB of an NB chain

Arguments:

    NB

Return Value:

    Number of bytes of data in the NB chain

--*/
{
    ULONG64 Return = 0;

    while (Nb)
    {
        Return += NET_BUFFER_DATA_LENGTH(Nb);
        Nb = NET_BUFFER_NEXT_NB(Nb);
    }

    return Return;
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
ULONG64
ndisNumDataBytesInNblChain(
    __in    PNET_BUFFER_LIST    Nbl)
/*++

Routine Description:

    Returns the number of bytes of data in each NB of an NBL chain

Arguments:

    NBL

Return Value:

    Number of bytes of data in the NBL chain

--*/
{
    ULONG64 Return = 0;

    while (Nbl)
    {
        Return += ndisNumDataBytesInNbChain(NET_BUFFER_LIST_FIRST_NB(Nbl));
        Nbl = NET_BUFFER_LIST_NEXT_NBL(Nbl);
    }

    return Return;
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
PNET_BUFFER_LIST
ndisLastNblInNblChain(
    __in    NET_BUFFER_LIST       *Nbl)
/*++

Routine Description:

    Returns the last NBL in an NBL chain

Arguments:

    NBL

Return Value:

    The last NBL in the chain

--*/
{
    PNET_BUFFER_LIST Next;

    while (NULL != (Next = NET_BUFFER_LIST_NEXT_NBL(Nbl)))
    {
        Nbl = Next;
    }

    return Nbl;
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
PNET_BUFFER
ndisLastNbInNbChain(
    __in    NET_BUFFER            *Nb)
/*++

Routine Description:

    Returns the last NB in an NB chain

Arguments:

    NB

Return Value:

    The last NB in the chain

--*/
{
    PNET_BUFFER Next;

    while (NULL != (Next = NET_BUFFER_NEXT_NB(Nb)))
    {
        Nb = Next;
    }

    return Nb;
}

#ifdef __cplusplus

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
NET_BUFFER_LIST const *
ndisLastNblInNblChain(
    __in    NET_BUFFER_LIST  const *Nbl)
/*++

Routine Description:

    Returns the last NBL in an NBL chain

Arguments:

    NBL

Return Value:

    The last NBL in the chain

--*/
{
    PNET_BUFFER_LIST Next;

    while (NULL != (Next = NET_BUFFER_LIST_NEXT_NBL(Nbl)))
    {
        Nbl = Next;
    }

    return Nbl;
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
NET_BUFFER const *
ndisLastNbInNbChain(
    __in    NET_BUFFER const      *Nb)
/*++

Routine Description:

    Returns the last NB in an NB chain

Arguments:

    NB

Return Value:

    The last NB in the chain

--*/
{
    PNET_BUFFER Next;

    while (NULL != (Next = NET_BUFFER_NEXT_NB(Nb)))
    {
        Nb = Next;
    }

    return Nb;
}

#endif // __cplusplus

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
VOID
ndisSetStatusInNblChain(
    __in_opt PNET_BUFFER_LIST   Nbl,
    __in    NDIS_STATUS         NdisStatus)
/*++

Routine Description:

    Sets the NBL->Status of each NBL in the chain to NdisStatus

Arguments:

    Nbl
    NdisStatus

--*/
{
    while (Nbl)
    {
        NET_BUFFER_LIST_STATUS(Nbl) = NdisStatus;
        Nbl = NET_BUFFER_LIST_NEXT_NBL(Nbl);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
VOID
ASSERT_VALID_NBL_QUEUE(
    _In_    NBL_QUEUE const    *NblQueue)
/*++

Routine Description:

    Verifies the NBL_QUEUE appears to be correct

Arguments:

    NblQueue

--*/
{
#if DBG

    WIN_ASSERT(NblQueue->Last != NULL);

    if (NblQueue->First == NULL)
    {
        WIN_ASSERT(NblQueue->Last == &NblQueue->First);
    }
    else
    {
        NET_BUFFER_LIST *Last = ndisLastNblInNblChain(NblQueue->First);
        WIN_ASSERT(NblQueue->Last == &Last->Next);
    }

#else
    UNREFERENCED_PARAMETER(NblQueue);
#endif
}

_IRQL_requires_max_(DISPATCH_LEVEL)
__inline
VOID
ASSERT_NBL_CHAINS_DO_NOT_OVERLAP(
    _In_    NET_BUFFER_LIST const *Chain1,
    _In_    NET_BUFFER_LIST const *Chain2)
/*++

Routine Description:

    Verifies that two NBL chains have no overlap

    Consider the following example:

        Chain1:  A->B->C->D->NULL
        Chain2:  C->D->NULL

    In this example, Chain1 and Chain2 overlap (they have elements C & D in
    common) so this routine would assert.

Arguments:

    Chain1
    Chain2

--*/
{
#if DBG

    if (Chain1 == NULL || Chain2 == NULL)
    {
        // The empty set has no common element with any other set.
        return;
    }
    
    WIN_ASSERT(ndisLastNblInNblChain(Chain1) != ndisLastNblInNblChain(Chain2));

#else
    UNREFERENCED_PARAMETER((Chain1, Chain2));
#endif
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
VOID
ndisInitializeNblQueue(
    __out   PNBL_QUEUE          NblQueue)
/*++

Routine Description:

    Initializes an NBL_QUEUE datastructure

Arguments:

    NblQueue

--*/
{
    NblQueue->First = NULL;

    // This is a weird assignment.  When the queue is empty, the Queue->Last
    // pointer points to the address of Queue->First.  The idea is that append
    // operations can be fast, since we don't have to special-case appending
    // to an empty queue.
    { C_ASSERT(FIELD_OFFSET(NET_BUFFER_LIST, Next) == 0); }
    NblQueue->Last = &NblQueue->First;

    ASSERT_VALID_NBL_QUEUE(NblQueue);
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
VOID
ndisAppendNblChainToNblQueueFast(
    __in    PNBL_QUEUE          NblQueue,
    __in    PNET_BUFFER_LIST    NblChainFirst,
    __in    PNET_BUFFER_LIST    NblChainLast)
/*++

Routine Description:

    Appends an NBL chain to an NBL_QUEUE

Arguments:

    NblQueue
    NblChainFirst - First NBL in the chain
    NblChainLast - Last NBL in the chain (NblChainLast->Next must be NULL)

--*/
{
    ASSERT_VALID_NBL_QUEUE(NblQueue);
#if DBG
    {
        NET_BUFFER_LIST *Last = ndisLastNblInNblChain(NblChainFirst);
        WIN_ASSERT(Last == NblChainLast);
        WIN_ASSERT(Last != *NblQueue->Last);
    }
#endif

    *NblQueue->Last = NblChainFirst;
    NblQueue->Last = &NblChainLast->Next;

    ASSERT_VALID_NBL_QUEUE(NblQueue);
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
VOID
ndisAppendNblChainToNblQueue(
    __in    PNBL_QUEUE          NblQueue,
    __in    PNET_BUFFER_LIST    NblChain)
/*++

Routine Description:

    Appends an NBL chain to an NBL_QUEUE

    This routine has the same effect as ndisAppendNblChainToNblQueueFast,
    however it is slower.  Use this routine if you don't have handy a pointer
    to the last NBL in the chain.

Arguments:

    NblQueue
    NblChain

--*/
{
    ndisAppendNblChainToNblQueueFast(
            NblQueue,
            NblChain,
            ndisLastNblInNblChain(NblChain));
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
PNET_BUFFER_LIST
ndisPopAllFromNblQueue(
    __in    PNBL_QUEUE          NblQueue)
/*++

Routine Description:

    Removes all NBLs from the queue, and returns them in a chain

Arguments:

    NblQueue

Return Value:

    The previous contents of the NBL_QUEUE

--*/
{
    PNET_BUFFER_LIST Nbls = NblQueue->First;

    ASSERT_VALID_NBL_QUEUE(NblQueue);
    ndisInitializeNblQueue(NblQueue);

    return Nbls;
}

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
PNET_BUFFER_LIST
ndisRemoveFromNblQueueByCancelId(
    __in    PNBL_QUEUE          NblQueue,
    __in    PVOID               CancelId)
/*++

Routine Description:

    Removes any NBL from the queue where the NBL's CancelId matches the
    parameter.  NBLs that were removed are returned in a chain.

    For example, if CancelId=3 and NblQueue is

        A[CancelId=4] => B[CancelId=3] => C[CancelId=2] => D[CancelId=3] => NULL

    then after this routine executes, the queue contains

        A[CancelId=4] => C[CancelId=2] => NULL

    and the routine's return value is

        B[CancelId=3] => D[CancelId=3] => NULL

Arguments:

    NblQueue
    CancelId

Return Value:

    A chain of all queued NBLs that matched CancelId, or NULL if no NBL matched

--*/
{
    NBL_QUEUE MatchingNbls;
    PNET_BUFFER_LIST Nbl, Next, Prev = NULL;

    ASSERT_VALID_NBL_QUEUE(NblQueue);

    ndisInitializeNblQueue(&MatchingNbls);

    for (Nbl = NblQueue->First; Nbl; Nbl = Next)
    {
        PVOID ThisId = NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(Nbl);

        Next = Nbl->Next;

        if (ThisId != CancelId)
        {
            Prev = Nbl;
            continue;
        }

        // Matched CancelId -- remove NBL from queue

        // If this was the first NBL in the queue, adjust Queue->First
        if (Nbl == NblQueue->First)
        {
            NblQueue->First = Nbl->Next;
        }

        // If this was the last NBL in the queue, adjust Queue->Last
        if (&Nbl->Next == NblQueue->Last)
        {
            NblQueue->Last = Prev ? &Prev->Next : &NblQueue->First;
        }

        // Unlink the NBL from the queue
        if (Prev)
        {
            Prev->Next = Nbl->Next;
        }
        Nbl->Next = NULL;

        // Put the NBL into our MatchingNbls collection, so we can return it.
        ndisAppendNblChainToNblQueueFast(&MatchingNbls, Nbl, Nbl);
    }

    ASSERT_VALID_NBL_QUEUE(NblQueue);

    return ndisPopAllFromNblQueue(&MatchingNbls);
}

#define FOR_EACH_NBL_IN_CHAIN(Nbl,NblChain)                 \
    for ((Nbl) = (NblChain); (Nbl) != NULL; (Nbl) = NET_BUFFER_LIST_NEXT_NBL(Nbl))

__drv_maxIRQL(DISPATCH_LEVEL)
__inline
VOID
ndisAppendNblsToNblChain(
    __in PNET_BUFFER_LIST*   NblChain,
    __in PNET_BUFFER_LIST    NblsToAppend)
{
    for (NOTHING; *NblChain != NULL; NblChain = &NET_BUFFER_LIST_NEXT_NBL(*NblChain))
        NOTHING;

    *NblChain = NblsToAppend;
}
