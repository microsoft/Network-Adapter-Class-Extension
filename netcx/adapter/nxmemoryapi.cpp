#include "Nx.hpp"

#include <NxApi.hpp>

#include "NxDevice.hpp"
#include "NxAdapter.hpp"
#include "verifier.hpp"
#include "version.hpp"

#include "NxMemory.hpp"

#include "NxMemoryApi.tmh"

static void NxMemoryDestroyCallback(WDFOBJECT Object)
{
    NxMemory * memory = GetNxMemoryFromHandle(Object);
    memory->~NxMemory();
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NTAPI
NETEXPORT(NetMemoryCollectionCreate)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ WDFDEVICE Device,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * ClientAttributes,
    _In_ const NET_MEMORY_COLLECTION_CONFIG * MemoryCollectionConfiguration,
    _Out_ NETMEMORYCOLLECTION* MemoryCollection
)
/*++
Routine Description:

    This method is called by the clients to create a memory collection.

Arguments:

    ClientAttributes - Optional attributes for the memory collection object.
    MemoryCollection - Pointer to the memory collection object to be created.

--*/
{
    NX_PRIVATE_GLOBALS const *nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    *MemoryCollection = nullptr;

    Verifier_VerifyObjectAttributesParentIsNull(nxPrivateGlobals, ClientAttributes);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, NxMemory);
    attributes.ParentObject = Device;
    attributes.EvtDestroyCallback = NxMemoryDestroyCallback;

    wil::unique_wdf_object object;
    CX_RETURN_IF_NOT_NT_SUCCESS(
        WdfObjectCreate(
            &attributes,
            &object));

#ifdef NETCX_UMDF
    wil::unique_hfile rdMemoryHandle;

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        NxMemory::OpenRdMemoryHandle(Device, rdMemoryHandle),
        "Failed to open rdMemory handle");

    NxMemory* nxMemory = new (GetNxMemoryFromHandle(object.get())) NxMemory(
        nxPrivateGlobals,
        wistd::move(rdMemoryHandle));
#else
    NxMemory* nxMemory = new (GetNxMemoryFromHandle(object.get())) NxMemory(
        nxPrivateGlobals);
#endif

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        nxMemory->Initialize(MemoryCollectionConfiguration),
        "Failed to initialize NxMemory.");

    if (ClientAttributes != nullptr)
    {
        CX_RETURN_IF_NOT_NT_SUCCESS(
            WdfObjectAllocateContext(
                object.get(),
                ClientAttributes,
                nullptr));
    }

    *MemoryCollection = reinterpret_cast<NETMEMORYCOLLECTION>(object.release());
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
NTSTATUS
NTAPI
NETEXPORT(NetMemoryCreate)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETMEMORYCOLLECTION MemoryCollection,
    _In_ const NET_MEMORY_CONFIG * Configuration,
    _Out_ NET_MEMORY_ID * MemoryId
)
/*++
Routine Description:

    This method is called by the clients to register memory.

Arguments:

    MemoryCollection - Pointer to the memory collection object.
    Configuration - Pointer to the memory configuration.
    MemoryId - Pointer to the memory ID to be registered.

--*/
{
    NX_PRIVATE_GLOBALS const *nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);
    Verifier_VerifyNetMemoryConfig(nxPrivateGlobals, Configuration);

    NxMemory * nxMemory = GetNxMemoryFromHandle(MemoryCollection);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        nxMemory->NetMemoryCreate(
            Configuration,
            MemoryId),
        "Failed to register NxMemory buffer.");

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFAPI
void
NTAPI
NETEXPORT(NetMemoryDestroy)(
    _In_ NET_DRIVER_GLOBALS * Globals,
    _In_ NETMEMORYCOLLECTION MemoryCollection,
    _In_ NET_MEMORY_ID MemoryId
)
/*++
Routine Description:

    This method is called by the clients to release memory.

Arguments:

    MemoryCollection - Pointer to the memory collection object.
    MemoryId - Pointer to the memory ID to be released.

--*/
{
    NX_PRIVATE_GLOBALS const *nxPrivateGlobals = GetPrivateGlobals(Globals);

    Verifier_VerifyPrivateGlobals(nxPrivateGlobals);
    Verifier_VerifyIrqlPassive(nxPrivateGlobals);

    NxMemory * nxMemory = GetNxMemoryFromHandle(MemoryCollection);

    nxMemory->NetMemoryDestroy(MemoryId);
}
