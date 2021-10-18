// Copyright (C) Microsoft Corporation. All rights reserved.

#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxPrivateGlobals.hpp"
#include "verifier.hpp"

#include "NxExecutionContext.hpp"
#include "NxExecutionContextTask.hpp"

#include "NxExecutionContextApi.tmh"


_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetExecutionContextCreate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ WDFDEVICE Device,
    _In_ NET_EXECUTION_CONTEXT_CONFIG const * Config,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * ClientAttributes,
    _Out_ NETEXECUTIONCONTEXT * ExecutionContext
)
/*++
Routine Description:

    Create a WDFOBJECT as ExecutionContext.

Returns:
    If succeeded, return created WDFOBJECT as NETEXECUTIONCONTEXT handle

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyTypeSize(nxPrivateGlobals, Config);
    Verifier_VerifyExecutionContextConfig(nxPrivateGlobals, Config);
    Verifier_VerifyObjectAttributesParentIsNull(nxPrivateGlobals, ClientAttributes);

    *ExecutionContext = nullptr;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxExecutionContext);
    attributes.ParentObject = Device;
    attributes.EvtDestroyCallback = [](WDFOBJECT Object)
    {
        auto ec = GetExecutionContextFromHandle(Object);
        ec->~NxExecutionContext();
    };

    wil::unique_wdf_object object;

    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfObjectCreate(
            &attributes,
            &object));

    auto ec = new (GetExecutionContextFromHandle(object.get())) NxExecutionContext(Device, Config);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        ec->Initialize(),
        "Failed to initialize execution context.");

    if (ClientAttributes != nullptr)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            WdfObjectAllocateContext(
                object.get(),
                ClientAttributes,
                nullptr));
    }

    //
    // Don't fail after this point or else the client's Cleanup / Destroy callbacks can get called.
    //

    *ExecutionContext = reinterpret_cast<NETEXECUTIONCONTEXT>(object.release());

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(HIGH_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetExecutionContextNotify)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETEXECUTIONCONTEXT ExecutionContext
)
{
    auto const privateGlobals = GetPrivateGlobals(DriverGlobals);
    Verifier_VerifyPrivateGlobals(privateGlobals);

    auto ec = GetExecutionContextFromHandle(ExecutionContext);
    ec->Notify();
}

_Must_inspect_result_
_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NETEXPORT(NetExecutionContextTaskCreate)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETEXECUTIONCONTEXT NetExecutionContextHandle,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * ClientAttributes,
    _In_ NET_EXECUTION_CONTEXT_TASK_CONFIG * NetExecutionContextTaskConfig,
    _Out_ NETEXECUTIONCONTEXTTASK* NetExecutionContextTaskHandle
)
/*++
Routine Description:

    Create a WDFOBJECT as NxExecutionContextTask.

Returns:
    If succeeded, return created WDFOBJECT as NETEXECUTIONCONTEXTTASK handle

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNotNull(nxPrivateGlobals, NetExecutionContextTaskConfig->EvtTask);
    Verifier_VerifyObjectAttributesParentIsNull(nxPrivateGlobals, ClientAttributes);

    *NetExecutionContextTaskHandle = nullptr;

    WDF_OBJECT_ATTRIBUTES taskAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&taskAttributes, NxExecutionContextTask);
    taskAttributes.ParentObject = NetExecutionContextHandle;

    taskAttributes.EvtCleanupCallback = [](WDFOBJECT Object)
    {
        auto nxExecutionContextTask = GetNxExecutionContextTaskFromHandle(Object);
        nxExecutionContextTask->OnDelete();
    };

    taskAttributes.EvtDestroyCallback = [](WDFOBJECT Object)
    {
        auto nxExecutionContextTask = GetNxExecutionContextTaskFromHandle(Object);
        nxExecutionContextTask->~NxExecutionContextTask();
    };

    wil::unique_wdf_any<WDFOBJECT> wdfObject;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        WdfObjectCreate(&taskAttributes, &wdfObject),
        "Failed to create task WDFOBJECT.");

    // Instantiate NxExecutionContextTask, and return WDFOBJECT as NETEXECUTIONCONTEXTTASK.
    auto task = new (GetNxExecutionContextTaskFromHandle(wdfObject.get())) NxExecutionContextTask(
        NetExecutionContextHandle,
        NetExecutionContextTaskConfig->EvtTask);

    CX_RETURN_IF_NOT_NT_SUCCESS(
        task->Initialize());

    if (ClientAttributes != nullptr)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
            WdfObjectAllocateContext(
                wdfObject.get(),
                ClientAttributes,
                nullptr),
            "Failed to allocate client driver's context for NETEXECUTIONCONTEXTTASK object.");
    }

    //
    // No failure after this point, otherwise the client driver's EvtCleanup/Destroy callbacks will be
    // invoked before this API returns
    //

    *NetExecutionContextTaskHandle = reinterpret_cast<NETEXECUTIONCONTEXTTASK>(wdfObject.release());

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
void
NETEXPORT(NetExecutionContextTaskEnqueue)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _In_ NETEXECUTIONCONTEXTTASK NetExecutionContextTaskHandle
)
/*++
Routine Description:

    Schedules a task to be executed in EC.
    Bugchecks if requeue unfinished task.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlLessThanOrEqualDispatch(nxPrivateGlobals);

    auto nxExecutionContextTask = GetNxExecutionContextTaskFromHandle(NetExecutionContextTaskHandle);

    Verifier_VerifyTaskNotQueued(nxPrivateGlobals, nxExecutionContextTask);

    nxExecutionContextTask->Enqueue();
}

_IRQL_requires_(PASSIVE_LEVEL)
WDFAPI
void
NETEXPORT(NetExecutionContextTaskWaitCompletion)(
    _In_ NET_DRIVER_GLOBALS * DriverGlobals,
    _Inout_ NETEXECUTIONCONTEXTTASK NetExecutionContextTaskHandle
)
/*++
Routine Description:

    Waits for the specified task until completion.
    Bugcheck if task is not queued at all.

--*/
{
    auto const nxPrivateGlobals = GetPrivateGlobals(DriverGlobals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    auto nxExecutionContextTask = GetNxExecutionContextTaskFromHandle(NetExecutionContextTaskHandle);

    // Don't reference count running execution context because
    // its running EC may already be deleted if ECTask has already finished.
    WdfObjectReference(NetExecutionContextTaskHandle);
    nxExecutionContextTask->WaitForCompletion();
    WdfObjectDereference(NetExecutionContextTaskHandle);
}
