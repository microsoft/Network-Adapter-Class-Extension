/*++
Copyright (c) Microsoft Corporation

Module Name:

    KUnits.h

Abstract:

    Implements simple user-defined literals for bits, bytes, etc.

Environment:

    Kernel mode or usermode unittest

--*/

#pragma once

namespace KUnits
{
namespace Bytes
{

constexpr unsigned long long operator "" _gigabytes(unsigned long long gb)
{
    return gb * 1024 * 1024 * 1024;
}

constexpr unsigned long long operator "" _gigabyte(unsigned long long gb)
{
    return gb * 1024 * 1024 * 1024;
}

constexpr unsigned long long operator "" _megabytes(unsigned long long mb)
{
    return mb * 1024 * 1024;
}

constexpr unsigned long long operator "" _megabyte(unsigned long long mb)
{
    return mb * 1024 * 1024;
}

constexpr unsigned long long operator "" _kilobytes(unsigned long long kb)
{
    return kb * 1024;
}

constexpr unsigned long long operator "" _kilobyte(unsigned long long kb)
{
    return kb * 1024;
}

constexpr unsigned long long operator "" _bytes(unsigned long long b)
{
    return b;
}

constexpr unsigned long long operator "" _byte(unsigned long long b)
{
    return b;
}

}

namespace BitsPerSecond
{

constexpr unsigned long long operator "" _gbps(unsigned long long gb)
{
    return gb * 1024 * 1024 * 1024;
}

constexpr unsigned long long operator "" _mbps(unsigned long long mb)
{
    return mb * 1024 * 1024;
}

constexpr unsigned long long operator "" _kbps(unsigned long long kb)
{
    return kb * 1024;
}

constexpr unsigned long long operator "" _bps(unsigned long long b)
{
    return b;
}

}
}
