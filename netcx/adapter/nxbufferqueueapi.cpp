// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxPrivateGlobals.hpp"
#include "verifier.hpp"

#include "NxBufferQueue.hpp"
#include "NxBufferQueueApi.tmh"

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NTAPI
NETEXPORT(NetBufferQueueCreate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETEXECUTIONCONTEXT ExecutionContext,
    _In_ NET_BUFFER_QUEUE_CONFIG const * Config,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * Attributes,
    _Outptr_ NETBUFFERQUEUE * BufferQueue
)
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyObjectAttributesParentIsNull(privateGlobals, Attributes);
    Verifier_VerifyTypeSize(privateGlobals, Config);

    *BufferQueue = nullptr;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxBufferQueue);
    attributes.ParentObject = ExecutionContext;
    attributes.EvtDestroyCallback = [](WDFOBJECT Object)
    {
        auto bufferQueue = GetNxBufferQueueFromHandle(Object);
        bufferQueue->~NxBufferQueue();
    };

    wil::unique_wdf_object object;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfObjectCreate(
            &attributes,
            &object));

    auto bufferQueue = new (GetNxBufferQueueFromHandle(object.get())) NxBufferQueue(ExecutionContext, Config);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        bufferQueue->Initialize(),
        "Failed to initialize buffer queue.");

    if (Attributes != nullptr)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            WdfObjectAllocateContext(
                object.get(),
                Attributes,
                nullptr));
    }

    //
    // Don't fail after this point or else the client's Cleanup / Destroy callbacks can get called.
    //

    *BufferQueue = reinterpret_cast<NETBUFFERQUEUE>(object.release());

    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetBufferQueueGetExtension)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETBUFFERQUEUE BufferQueue,
    _In_ NET_EXTENSION_QUERY const * Query,
    _Out_ NET_EXTENSION * Extension
)
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    RtlZeroMemory(Extension, sizeof(*Extension));

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlPassive(privateGlobals);
    Verifier_VerifyTypeSize(privateGlobals, Query);
    Verifier_VerifyNetExtensionQuery(privateGlobals, Query);

    auto bufferQueue = GetNxBufferQueueFromHandle(BufferQueue);

    bufferQueue->GetExtension(
        Query->Name,
        Query->Version,
        Query->Type,
        Extension);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NET_RING *
NTAPI
NETEXPORT(NetBufferQueueGetRing)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETBUFFERQUEUE BufferQueue
    )
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(privateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(privateGlobals);

    auto bufferQueue = GetNxBufferQueueFromHandle(BufferQueue);
    return bufferQueue->GetRing();
}
