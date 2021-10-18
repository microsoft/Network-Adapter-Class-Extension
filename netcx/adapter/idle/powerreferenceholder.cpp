// Copyright (C) Microsoft Corporation. All rights reserved.
#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#define ASSERT(x)
#endif
#include <ntassert.h>
#include <wdf.h>
#include <NxTrace.hpp>

#include "PowerReferenceHolder.hpp"

#include "PowerReferenceHolder.tmh"

_Use_decl_annotations_
PowerReferenceHolder::PowerReferenceHolder(
    KRundown & Rundown
)
    : m_type(Type::Rundown)
    , m_rundown(&Rundown)
    , m_ntStatus(STATUS_SUCCESS)
{
}

_Use_decl_annotations_
PowerReferenceHolder::PowerReferenceHolder(
    WDFDEVICE Device,
    void const * Tag,
    NTSTATUS NtStatus
)
    : m_type(Type::WDF)
    , m_device(Device)
    , m_tag(Tag)
    , m_ntStatus(NtStatus)
{
}

_Use_decl_annotations_
PowerReferenceHolder::PowerReferenceHolder(
    PowerReferenceHolder && Other
)
    : m_rundown(Other.m_rundown)
    , m_device(Other.m_device)
    , m_ntStatus(Other.m_ntStatus)
    , m_tag(Other.m_tag)
    , m_type(Other.m_type)
{
    Other.m_rundown = nullptr;
    Other.m_device = WDF_NO_HANDLE;
    Other.m_ntStatus = STATUS_UNSUCCESSFUL;
    Other.m_tag = nullptr;
    Other.m_type = Type::None;
}

_Use_decl_annotations_
PowerReferenceHolder &
PowerReferenceHolder::operator=(
    PowerReferenceHolder && Other
    )
{
    if (this != &Other)
    {
        this->m_device = Other.m_device;
        this->m_ntStatus = Other.m_ntStatus;
        this->m_rundown = Other.m_rundown;
        this->m_tag = Other.m_tag;
        this->m_type = Other.m_type;

        Other.m_device = WDF_NO_HANDLE;
        Other.m_ntStatus = STATUS_UNSUCCESSFUL;
        Other.m_rundown = nullptr;
        Other.m_tag = nullptr;
        Other.m_type = Type::None;
    }

    return *this;
}

_Use_decl_annotations_
PowerReferenceHolder::~PowerReferenceHolder(
    void
)
{
    switch (m_type)
    {
        case Type::Rundown:

            NT_FRE_ASSERT(NT_SUCCESS(m_ntStatus));
            m_rundown->Release();

            break;

        case Type::WDF:

            if (NT_SUCCESS(m_ntStatus))
            {
                WdfDeviceResumeIdleWithTag(m_device, const_cast<void*>(m_tag));
            }

            break;
    }
}
