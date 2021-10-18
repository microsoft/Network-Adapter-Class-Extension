// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "WdfObjectCallback.tmh"

template<typename WDFTYPE, typename PFn, bool Required>
class WdfObjectCallback
{
public:

    WdfObjectCallback(
        WDFTYPE TargetObject,
        PFn Callback
    )
        : m_targetObject(TargetObject)
        , m_callback(Callback)
    {
    }

    template<typename... Args>
    typename wistd::result_of<PFn(WDFTYPE, Args...)>::type
    Invoke(
        Args... args
    ) const
    {
        using returnType = typename wistd::result_of<PFn(WDFTYPE, Args...)>::type;

        if constexpr (!Required)
        {
            if (m_callback == nullptr)
            {
                LogVerbose(FLAG_OBJECT_CALLBACK,
                    "Driver did not provide optional callback. WDFOBJECT=%p",
                    m_targetObject);

                return returnType();
            }
        }

        LogVerbose(FLAG_OBJECT_CALLBACK,
            "Driver callback entry. WDFOBJECT=%p, Callback=%p",
                m_targetObject,
                m_callback);

        if constexpr (wistd::is_void_v<returnType>)
        {
            m_callback(m_targetObject, args...);

            LogVerbose(FLAG_OBJECT_CALLBACK, "Driver callback exit");
        }
        else
        {
            auto r = m_callback(m_targetObject, args...);

            if constexpr (wistd::is_same_v<returnType, NTSTATUS>)
            {
                LogVerbose(FLAG_OBJECT_CALLBACK,
                    "Driver callback exit. NTSTATUS=%!STATUS!",
                        r);
            }
            else
            {
                LogVerbose(FLAG_OBJECT_CALLBACK, "Driver callback exit");
            }

            return r;
        }
    }

private:

    WDFTYPE const
        m_targetObject;

    PFn const
        m_callback;
};

template<typename WDFTYPE, typename PFn>
class WdfObjectOptionalCallback : public WdfObjectCallback<WDFTYPE, PFn, false>
{
public:

    WdfObjectOptionalCallback(
        WDFTYPE TargetObject,
        PFn Callback
    )
        : WdfObjectCallback(TargetObject, Callback)
    {
    }
};

template<typename WDFTYPE, typename PFn>
class WdfObjectRequiredCallback : public WdfObjectCallback<WDFTYPE, PFn, true>
{
public:

    WdfObjectRequiredCallback(
        WDFTYPE TargetObject,
        PFn Callback
    )
        : WdfObjectCallback(TargetObject, Callback)
    {
    }
};

class IdleStateMachine;

template <typename T>
struct idle_state_machine_proxy
{
    static const bool
        is_handle_supported = false;

    static
    IdleStateMachine *
    GetIdleStateMachine(
        T Object
    )
    {
        UNREFERENCED_PARAMETER(Object);
        return nullptr;
    }
};

IdleStateMachine *
IdleStateMachineFromDevice(
    _In_ WDFDEVICE Device
);

template <>
struct idle_state_machine_proxy<WDFDEVICE>
{
    static const bool
        is_handle_supported = true;

    static
    IdleStateMachine *
    GetIdleStateMachine(
        WDFDEVICE Object
    )
    {
        return IdleStateMachineFromDevice(Object);
    }
};

IdleStateMachine *
IdleStateMachineFromAdapter(
    _In_ NETADAPTER Adapter
);

template <>
struct idle_state_machine_proxy<NETADAPTER>
{
    static const bool
        is_handle_supported = true;

    static
    IdleStateMachine *
    GetIdleStateMachine(
        NETADAPTER Object
    )
    {
        return IdleStateMachineFromAdapter(Object);
    }
};

template <typename PFn, typename WDFTYPE, typename... Args>
typename wistd::result_of<PFn(WDFTYPE, Args...)>::type
InvokePoweredCallback(
    PFn Callback,
    WDFTYPE Object,
    Args... args
)
{
    using returnType = typename wistd::result_of<PFn(WDFTYPE, Args...)>::type;

    static_assert(
        idle_state_machine_proxy<WDFTYPE>::is_handle_supported,
        "It is not possible to find the idle state machine from the callback's first parameter.");

    // Find the idle state machine from the callback's first parameter
    auto idleStateMachine = idle_state_machine_proxy<WDFTYPE>::GetIdleStateMachine(Object);

    // Acquire a synchronous power referene, use the callback address as the tag
    auto powerReference = idleStateMachine->StopIdle(
        true,
        Callback);

    LogVerbose(FLAG_OBJECT_CALLBACK, "Driver callback entry. Callback=%p", Callback);

    if constexpr (wistd::is_void_v<returnType>)
    {
        Callback(Object, args...);
        LogVerbose(FLAG_OBJECT_CALLBACK, "Driver callback exit");
    }
    else
    {
        auto const r = Callback(Object, args...);

        if constexpr (wistd::is_same_v<returnType, NTSTATUS>)
        {
            LogVerbose(FLAG_OBJECT_CALLBACK, "Driver callback exit. NTSTATUS=%!STATUS!", r);
        }
        else
        {
            LogVerbose(FLAG_OBJECT_CALLBACK, "Driver callback exit");
        }

        return r;
    }
}
