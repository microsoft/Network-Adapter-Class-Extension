/*++

    Copyright (C) Microsoft Corporation. All rights reserved.

Module Name:

    NxTranslationApp.cpp

Abstract:

    Implements the driver-global state for the NBL-to-NET_PACKET translation
    app in NetAdapterCx.

--*/

#include "NxXlatPrecomp.hpp"
#include "NxXlatCommon.hpp"

#include "NxTranslationApp.tmh"

#include "NxXlat.hpp"
#include "NxTranslationApp.hpp"

template<typename T>
unique_packet_extension
AllocateNetPacketExtension(PCWSTR name)
{
    NET_PACKET_INITIALIZE_HANDLER *pInitialize = [](
        _In_opt_ void *CallbackContext,
        _In_ void *Extension) {

        UNREFERENCED_PARAMETER(CallbackContext);

        new (Extension) T();
    };

    NET_PACKET_DESTROY_HANDLER *pDestroy = [](
        _In_opt_ void *CallbackContext,
        _In_ void *Extension) {

        UNREFERENCED_PARAMETER((CallbackContext, Extension));

        static_cast<T*>(Extension)->~T();
    };

    return unique_packet_extension(NetPacketExtensionAllocate(name, sizeof(T), alignof(T), pInitialize, pDestroy, nullptr, nullptr, nullptr));
}

NTSTATUS
NxTranslationAppFactory::AllocateExtensions()
{
    auto Thread802_15_4MSIExExtension = AllocateNetPacketExtension<PVOID>(THREAD_802_15_4_MSIEx_PACKET_EXTENSION_NAME);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !Thread802_15_4MSIExExtension);

    auto Thread802_15_4StatusExtension = AllocateNetPacketExtension<NTSTATUS>(THREAD_802_15_4_STATUS_PACKET_EXTENSION_NAME);
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !Thread802_15_4StatusExtension);

    // It's sufficient to wait for the last one; that ensures all previous ones are ready too.
    NetPacketExtensionWaitForReady(Thread802_15_4StatusExtension.get());

    m_Thread802_15_4MSIExExtension = wistd::move(Thread802_15_4MSIExExtension);
    m_Thread802_15_4StatusExtension = wistd::move(Thread802_15_4StatusExtension);

    return STATUS_SUCCESS;
}

NTSTATUS
NxTranslationAppFactory::Initialize()
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        AllocateExtensions(),
        "Failed to allocate extensions required for tranlsation. NxTranslationAppFactory=%p", this);

    return STATUS_SUCCESS;
}

NTSTATUS
NxTranslationAppFactory::CreateApp(_Out_ INxApp ** app)
{
    *app = nullptr;

    wistd::unique_ptr<NxTranslationApp> newApp{ new (std::nothrow) NxTranslationApp() };
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !newApp);

    *app = newApp.release();

    return STATUS_SUCCESS;
}

PAGED 
NTSTATUS
NxTranslationApp::StartTx(INxAdapter * adapter)
{
    wistd::unique_ptr<NxTxXlat> txQueue{ new(std::nothrow) NxTxXlat{ adapter } };
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !txQueue);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        txQueue->StartQueue(adapter),
        "Failed to initializate the Tx translation queue. NxTranslationApp=%p", this);

    m_txQueue = wistd::move(txQueue);

    return STATUS_SUCCESS;
}

PAGED 
NTSTATUS
NxTranslationApp::StartRx(INxAdapter * adapter)
{
    wistd::unique_ptr<NxRxXlat> rxQueue{ new(std::nothrow) NxRxXlat{ adapter } };
    CX_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !rxQueue);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        rxQueue->StartQueue(adapter),
        "Failed to initializate the Rx translation queue. NxTranslationApp=%p", this);

    m_rxQueue = wistd::move(rxQueue);

    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NxTranslationApp::Start(INxAdapter * adapter)
{
    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        StartTx(adapter),
        "Failed to start Tx translation queue. NxTranslationApp=%p,INxAdapter=%p", this, adapter);

    CX_RETURN_IF_NOT_NT_SUCCESS_MSG(
        StartRx(adapter),
        "Failed to start Rx translation queue. NxTranslationApp=%p,INxAdapter=%p", this, adapter);

    return STATUS_SUCCESS;
}
