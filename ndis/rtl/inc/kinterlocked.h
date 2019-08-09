#pragma once

namespace mem
{

ULONG
inline
InterlockedOrU(
    _In_ ULONG volatile *Flags,
    _In_ ULONG Flag)
/*++

Routine Description:

    Same as InterlockedOr, except with ULONGs instead of LONGs

    (Seriously, who does bitwise arithmetic on signed quantities?)

--*/
{
    return static_cast<ULONG>(InterlockedOr(reinterpret_cast<LONG volatile*>(Flags), static_cast<LONG>(Flag)));
}

ULONG
inline
InterlockedAndU(
    _In_ ULONG volatile *Flags,
    _In_ ULONG Flag)
/*++

Routine Description:

    Same as InterlockedAnd, except with ULONGs instead of LONGs

--*/
{
    return static_cast<ULONG>(InterlockedAnd(reinterpret_cast<LONG volatile*>(Flags), static_cast<LONG>(Flag)));
}

}

