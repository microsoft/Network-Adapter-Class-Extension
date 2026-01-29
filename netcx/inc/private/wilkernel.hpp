// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#ifdef _KERNEL_MODE
using unique_process_object = wil::unique_any<PEPROCESS, decltype(::ObfDereferenceObject), ::ObfDereferenceObject>;
using unique_thread_object = wil::unique_any<PKTHREAD, decltype(::ObfDereferenceObject), ::ObfDereferenceObject>;
using unique_fileobject = wil::unique_any<FILE_OBJECT *, decltype(&ObfDereferenceObject), &ObfDereferenceObject>;
using unique_callback_object = wil::unique_any<PCALLBACK_OBJECT, decltype(::ObfDereferenceObject), ::ObfDereferenceObject>;
using unique_callback_registration = wil::unique_any<void *, decltype(::ExUnregisterCallback), ::ExUnregisterCallback>;
using unique_device_object_reference = wil::unique_any<DEVICE_OBJECT *, decltype(::ObfDereferenceObject), ::ObfDereferenceObject>;
#endif

#ifdef WDFAPI
using unique_cdo = wil::unique_wdf_any<WDFDEVICE>;
#endif

using unique_io_mdl = wil::unique_any<MDL *, decltype(IoFreeMdl), IoFreeMdl>;
