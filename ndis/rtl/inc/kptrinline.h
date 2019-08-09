/*++
Copyright (c) Microsoft Corporation

Module Name:

    KPtrInline.h

Abstract:

    Complements KPtr.h; refer to that file for details

    This file is a workaround for a poor design in the debugger.
    The debugger unfortunately uses line numbers to encode metadata,
    including information about whether an assembly instruction
    is compiler-generated, and thus, uninteresting.

    So if we want to tell the debugger to skip over boring lines
    of code, we have to mess with the actual line numbering of
    source code.  This cannot be done cleanly without messing up
    debugging in the rest of the source file.  Therefore, we move
    certain parts of the code to a separate source file, to avoid
    tainting the original file's line numbering.

--*/

#line 0x00f00f00 // always step over this routine in the debugger
PAGED T &operator*() const { return *_p; }

#line 0x00f00f00 // always step over this routine in the debugger
PAGED pointer operator->() const { return &**this; }

#line 0x00f00f00 // always step over this routine in the debugger
PAGED pointer get() const { return _p; }

