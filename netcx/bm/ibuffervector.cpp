// Copyright (C) Microsoft Corporation. All rights reserved.
#include "BmPrecomp.hpp"

#include "IBufferVector.hpp"
#include "IBufferVector.tmh"

_Use_decl_annotations_
IBufferVector::IBufferVector(
    size_t BufferCount,
    size_t BufferOffset,
    size_t BufferCapacity
)
    : m_BufferCount(BufferCount)
    , m_BufferOffset(BufferOffset)
    , m_BufferCapacity(BufferCapacity)
{
}
