// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <wil\wistd_type_traits.h>

#if _KERNEL_MODE

    // nullptr_t is normally automatically defined by the CRT headers, but it
    // doesn't get included by kernel code.
    namespace std { typedef decltype(__nullptr) nullptr_t; }
    using ::std::nullptr_t;

#endif // _KERNEL_MODE

#define BEGIN_MACRO do {
#define END_MACRO } while (0)

#ifdef _KERNEL_MODE
#define CODE_SEG(segment) __declspec(code_seg(segment))
#else
#define CODE_SEG(segment) 
#endif

#define PAGED CODE_SEG("PAGE") _IRQL_always_function_max_(PASSIVE_LEVEL)
#define PAGEDX CODE_SEG("PAGE")
#define PNPCODE CODE_SEG("PAGENPNP")
#define INITCODE CODE_SEG("INIT")
#define NONPAGED CODE_SEG(".text") _IRQL_requires_max_(DISPATCH_LEVEL)

enum CallRunMode 
{ 
    // This call should complete synchronously on the current thread
    RunSynchronous, 
    // This call should return immediately, and complete the operation in a background thread
    RunAsynchronous,
    // This call can return immediately, OR complete synchronously
    // (Use this if you're running on a workitem thread already, and you
    //  don't mind if the callee uses your thread to do its work, but you
    //  can tolerate the call completing asynchronously if the callee doesn't
    //  need your thread.)
    RunAsynchronousButOkayToBlock,
};



