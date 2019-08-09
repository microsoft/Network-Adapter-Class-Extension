// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include "KWaitEvent.h"

#define NETADAPTERCX_TAG 'xCdN'
#define NETADAPTERCX_TAG_PTR ((void *)(PULONG_PTR) NDISCX_TAG)
#define NX_WMI_TAG 'mWxN'

#ifndef RTL_IS_POWER_OF_TWO
#  define RTL_IS_POWER_OF_TWO(Value) \
    ((Value != 0) && !((Value) & ((Value) - 1)))
#endif

FORCEINLINE
void
SetCompletionRoutineSmart(
    _In_     PDEVICE_OBJECT         DeviceObject,
    _In_     PIRP                   Irp,
    _In_     PIO_COMPLETION_ROUTINE CompletionRoutine,
    _In_opt_ void *                  Context,
    _In_     BOOLEAN                InvokeOnSuccess,
    _In_     BOOLEAN                InvokeOnError,
    _In_     BOOLEAN                InvokeOnCancel
    )
/*++

Routine Description:
    This routine first calls the IoSetCompletionRoutineEx to set the completion
    routine on the Irp, and if it fails then it calls the old
    IoSetCompletionRoutine.

    Using IoSetCompletionRoutine can result in a rare issue where
    the driver might get unloaded prior to the IoSetCompletionRoutine returns.

    Leveraging IoSetCompletionRoutineEx first and if it fails using
    IoSetCompletionRoutine shrinks the window in which that issue can happen
    to negligible. This is a common practice used across several other inbox
    drivers.
--*/
{
    if (!NT_SUCCESS(IoSetCompletionRoutineEx(DeviceObject,
                                             Irp,
                                             CompletionRoutine,
                                             Context,
                                             InvokeOnSuccess,
                                             InvokeOnError,
                                             InvokeOnCancel))) {
        IoSetCompletionRoutine(Irp,
                               CompletionRoutine,
                               Context,
                               InvokeOnSuccess,
                               InvokeOnError,
                               InvokeOnCancel);
    }
}

/*++
Macro Description:
    This method loops through each entry in a doubly linked list (LIST_ENTRY).
    It assumes that the doubly linked list has a dedicated list head

Arguments:
    Type - The type of each entry of the linked list.

    Head - A Pointer to the list head

    Field - The name of LIST_ENTRY filed in the stucture.

    Current - A pointer to the current entry (to be used in the body of the
        FOR_ALL_IN_LIST

Usage:

    typedef struct _MYENTRY {
        ULONG Version;
        ULONG SubVersion;
        LIST_ENTRY Link;
        UCHAR Data;
    } MYENTRY, *PMYENTRY;

    typedef struct _MYCONTEXT {
        ULONG Size;
        LIST_ENTRY MyEntryListHead;
        ...
    } *PMYCONTEXT;

    PMYENTRY FindMyEntry(PMYCONTEXT Context,
                         UCHAR Data) {
        PMYENTRY entry;

        FOR_ALL_IN_LIST(MYENTRY,
                        &Context->MyEntryListHead,
                        Link,
                        entry) {
            if (entry->Data == Data) { return entry }
    }

Remarks :
    While using the FOR_ALL_IN_LIST, you must not change the
    structure of the list. In case you want to remove the current element
    and continue interating the list, use FOR_ALL_IN_LIST_SAFE
--*/
#define FOR_ALL_IN_LIST(Type, Head, Field, Current)                 \
    for((Current) = CONTAINING_RECORD((Head)->Flink, Type, Field);  \
       (Head) != &(Current)->Field;                                 \
       (Current) = CONTAINING_RECORD((Current)->Field.Flink,        \
                                     Type,                          \
                                     Field)                         \
       )

/*++
Macro Description:
    This method loops through each entry in a doubly linked list (LIST_ENTRY).
    It assumes that the doubly linked list has a dedicated list head.
    In each iteration of the loop it is safe to remove the current element from
    the list.

Arguments:
    Type - The type of each entry of the linked list.

    Head - A Pointer to the list head

    Field - The name of LIST_ENTRY filed in the stucture.

    Current - A pointer to the current entry (to be used in the body of the
        FOR_ALL_IN_LIST

    Next - A pointer to the next entry in the list, that the user must not touch

Usage:

    typedef struct _MYENTRY {
        ULONG Version;
        ULONG SubVersion;
        LIST_ENTRY Link;
        UCHAR Data;
    } MYENTRY, *PMYENTRY;

    typedef struct _MYCONTEXT {
        ULONG Size;
        LIST_ENTRY MyEntryListHead;
        ...
    } *PMYCONTEXT;

    void DeleteEntries(PMYCONTEXT Context,
                           UCHAR Data) {
        PMYENTRY entry, nextEntry;

        FOR_ALL_IN_LIST_SAFE(MYENTRY,
                            &Context->MyEntryListHead,
                            Link,
                            entry,
                            nextEntry) {
            if (entry->Data == Data) {
                RemoveEntryList(&entry->Link);
                ExFreePool(entry);
            }
    }
 --*/
#define FOR_ALL_IN_LIST_SAFE(Type, Head, Field, Current, Next)          \
    for((Current) = CONTAINING_RECORD((Head)->Flink, Type, Field),      \
            (Next) = CONTAINING_RECORD((Current)->Field.Flink,          \
                                       Type, Field);                    \
       (Head) != &(Current)->Field;                                     \
       (Current) = (Next),                                              \
            (Next) = CONTAINING_RECORD((Current)->Field.Flink,          \
                                     Type, Field)                       \
       )

FORCEINLINE
void
InitializeListEntry(
    _Out_ PLIST_ENTRY ListEntry
    )
/*++

Routine Description:

    Initialize a list entry to NULL.

    - Using this improves catching list manipulation errors
    - This should not be called on a list head
    - Callers may depend on use of NULL value

--*/
{
    ListEntry->Flink = ListEntry->Blink = NULL;
}

FORCEINLINE
LONG
NxInterlockedIncrementFloor(
    __inout LONG  volatile *Target,
    __in LONG Floor
    )
{
    LONG startVal;
    LONG currentVal;

    currentVal = *Target;

    do {
        if (currentVal <= Floor) {
            return currentVal;
        }

        startVal = currentVal;

        //
        // currentVal will be the value that used to be Target if the exchange was made
        // or its current value if the exchange was not made.
        //
        currentVal = InterlockedCompareExchange(Target, startVal+1, startVal);

        //
        // If startVal == currentVal, then no one updated Target in between the deref at the top
        // and the InterlockedCompareExchange afterward.
        //
    } while (startVal != currentVal);

    //
    // startVal is the old value of Target. Since InterlockedIncrement returns the new
    // incremented value of Target, we should do the same here.
    //
    return startVal+1;
}


FORCEINLINE
LONG
NxInterlockedDecrementFloor(
    __inout LONG  volatile *Target,
    __in LONG Floor
    )
{
    LONG startVal;
    LONG currentVal;

    currentVal = *Target;

    do {
        if (currentVal <= Floor) {
            //
            // This value cannot be returned in the success path
            //
            return Floor-1;
        }

        startVal = currentVal;

        //
        // currentVal will be the value that used to be Target if the exchange was made
        // or its current value if the exchange was not made.
        //
        currentVal = InterlockedCompareExchange(Target, startVal -1, startVal);

        //
        // If startVal == currentVal, then no one updated Target in between the deref at the top
        // and the InterlockedCompareExchange afterward.
        //
    } while (startVal != currentVal);

    //
    // startVal is the old value of Target. Since InterlockedDecrement returns the new
    // decremented value of Target, we should do the same here.
    //
    return startVal -1;
}

FORCEINLINE
LONG
NxInterlockedIncrementGTZero(
    __inout LONG  volatile *Target
    )
{
    return NxInterlockedIncrementFloor(Target, 0);
}

class DispatchLock {
private:
    volatile LONG m_Count;
    KWaitEvent    m_Event;

    //
    // For performance reasons this lock may not enabled.
    // In that case the member of this Lock just fake success
    //
    BOOLEAN       m_Enabled;

public:
    DispatchLock(
        BOOLEAN Enabled
        ):
        m_Count(0),
        m_Enabled(Enabled)
    {
        if (!Enabled) { return; }
    }

    void
    InitAndAcquire(
        void
        ) {
        if (!m_Enabled) { return; }
        NT_ASSERT(m_Count == 0);
        m_Count = 1;
        m_Event.Clear();
    }

    BOOLEAN
    Acquire(
        void
        ) {
        if (!m_Enabled) { return TRUE; }
        return (NxInterlockedIncrementGTZero(&m_Count) != 0);
    }

    void
    Release(
        void
        ) {
        if (!m_Enabled) { return; }
        if (0 == InterlockedDecrement(&m_Count)) {
            m_Event.Set();
        }
    }

    void
    ReleaseAndWait(
        void
        ){
        if (!m_Enabled) { return; }
        Release();
        m_Event.Wait();
    }

};

#define WHILE(a) \
__pragma(warning(suppress:4127)) while(a)

#define POINTER_WITH_HIDDEN_BITS_MASK 0x7
class PointerWithHiddenBits {

public:

    static
    void *
    _GetPtr(
        void * Ptr
        ) {
        return (void *)(((ULONG_PTR)Ptr) & ~POINTER_WITH_HIDDEN_BITS_MASK);
    }

    static
    void
    _SetBit0(
        void ** Ptr
        )
    {
        ULONG_PTR ptrVal = (ULONG_PTR)*Ptr;
        ptrVal |= 0x1;
        *Ptr = (void *)(ptrVal);
    }

    static
    BOOLEAN
    _IsBit0Set(
        void const * Ptr
        )
    {
        ULONG_PTR ptrVal;
        ptrVal = (ULONG_PTR)Ptr;
        ptrVal &= 0x1;
        return (ptrVal != 0);
    }

};

inline
NTSTATUS
SetWmiBufferTooSmall(
    _In_ ULONG BufferSize,
    _In_ void * Wnode,
    _In_ ULONG WnodeSize,
    _Out_ PULONG PReturnSize
    )
/*++
Routine Description: 
    This method checks the buffer size of the WNODE to check if it is smaller than 
    WNODE_TOO_SMALL and returns the status accordingly. Otherwise, it sets the 
    WNODE_FLAG_TOO_SMALL flag on the WNODE header and updates the size needed in 
    the WNODE.
--*/
{
    if (BufferSize < sizeof(WNODE_TOO_SMALL))
    {
        *PReturnSize = sizeof(ULONG);
        return STATUS_BUFFER_TOO_SMALL;
    }
    else 
    {
        ((PWNODE_TOO_SMALL)Wnode)->WnodeHeader.BufferSize = sizeof(WNODE_TOO_SMALL);
        ((PWNODE_TOO_SMALL)Wnode)->WnodeHeader.Flags |= WNODE_FLAG_TOO_SMALL;
        ((PWNODE_TOO_SMALL)Wnode)->SizeNeeded = WnodeSize;
        *PReturnSize= sizeof(WNODE_TOO_SMALL);
        return STATUS_SUCCESS;
    }
}
