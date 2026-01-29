#include "NetClientDriverConfigurationPrecomp.hpp"
#include <NetClientDriverConfigurationConstants.h>

// Enum, Name, Min, Max, Default, current value, flags
struct DRIVER_CONFIGURATION_KNOB
{
    DRIVER_CONFIG_ENUM ConfigurationEnum;
    PCWSTR ConfigurationName;
    ULONG ConfigurationMinValue;
    ULONG ConfigurationMaxValue;
    ULONG ConfigurationDefaultValue;
    ULONG ConfigurationCurrentValue;
    ULONG Flags;
};

DRIVER_CONFIGURATION_KNOB g_validConfigurations[] =
{
    // Name, Min, Max, Default (Read only), current value, flags (RW)
    { ALLOW_DMA_HAL_BYPASS, ALLOW_DMA_HAL_BYPASS_NAME, 0, 1, 1, 0, DRIVER_CONFIG_KNOB_IS_BOOLEAN },
    { DMA_BOUNCE_POLICY, DMA_BOUNCE_POLICY_NAME, 0, 2, 0, 0, 0 },
    { IGNORE_VERIFIER_DEBUG_BREAK, IGNORE_VERIFIER_DEBUG_BREAK_NAME, 0, 1, 0, 0, DRIVER_CONFIG_KNOB_IS_BOOLEAN },
};

_IRQL_requires_(PASSIVE_LEVEL)
bool
IsConfigurationValueValidUlong(
    _In_ DRIVER_CONFIGURATION_KNOB* Configuration,
    _In_ ULONG Value
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
QueryDriverConfigurationFromRegistry(
    _In_ DRIVER_CONFIGURATION_KNOB* Configuration,
    _Out_ ULONG* Value
    );

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
LoadDriverConfiguration(
    _In_ size_t configurationIndex,
    _Out_ ULONG* Value
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
NetClientQueryDriverConfigurationUlong(
    _In_ DRIVER_CONFIG_ENUM ConfigurationEnum
    )
{
    // invalid configuration
    NT_FRE_ASSERT(ConfigurationEnum < ARRAYSIZE(g_validConfigurations));
    NT_FRE_ASSERT(WI_IsFlagClear(g_validConfigurations[(size_t)ConfigurationEnum].Flags, DRIVER_CONFIG_KNOB_IS_BOOLEAN));

    return g_validConfigurations[(size_t)ConfigurationEnum].ConfigurationCurrentValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
NetClientQueryDriverConfigurationBoolean(
    _In_ DRIVER_CONFIG_ENUM ConfigurationEnum
    )
{
    // invalid configuration
    NT_FRE_ASSERT(ConfigurationEnum < ARRAYSIZE(g_validConfigurations));
    NT_FRE_ASSERT(WI_IsFlagSet(g_validConfigurations[(size_t)ConfigurationEnum].Flags, DRIVER_CONFIG_KNOB_IS_BOOLEAN));

    return (g_validConfigurations[(size_t)ConfigurationEnum].ConfigurationCurrentValue != FALSE);
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
DriverConfigurationInitialize()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    for (size_t i = 0; i < ARRAYSIZE(g_validConfigurations); i++)
    {
        status = LoadDriverConfiguration(i, &g_validConfigurations[i].ConfigurationCurrentValue);
        if (!NT_SUCCESS(status))
            return status;
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
bool
IsConfigurationValueValidUlong(
    _In_ DRIVER_CONFIGURATION_KNOB* Configuration,
    _In_ ULONG Value
    )
{
    if (Value >= Configuration->ConfigurationMinValue &&
        Value <= Configuration->ConfigurationMaxValue)
    {
        return true;
    }

    return false;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
LoadDriverConfiguration(
    _In_ size_t configurationIndex,
    _Out_ ULONG* Value
    )
{
    NTSTATUS status =
        QueryDriverConfigurationFromRegistry(
            &g_validConfigurations[configurationIndex],
            Value);

    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
QueryDriverConfigurationFromRegistry(
    _In_ DRIVER_CONFIGURATION_KNOB* Configuration,
    _Out_ ULONG* Value
    )
{
    bool readFromImmutableStore = false;
    bool useDefaultValue = false;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    WDFKEY key = NULL;
    UNICODE_STRING ConfigurationNameU;

    if (Value == nullptr || Configuration == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlInitUnicodeString(&ConfigurationNameU, Configuration->ConfigurationName);

    // first try get the value from PersistentState Registry
    status =
        WdfDriverOpenPersistentStateRegistryKey(
            WdfGetDriver(),
            KEY_READ,
            WDF_NO_OBJECT_ATTRIBUTES,
            &key);

    if (!NT_SUCCESS(status))
    {
        readFromImmutableStore = true;
    }
    else
    {
        status =
            WdfRegistryQueryULong(
                key,
                &ConfigurationNameU,
                Value);

        WdfRegistryClose(key);

        if (!NT_SUCCESS(status))
        {
            readFromImmutableStore = true;
        }
        else
        {
            if (!IsConfigurationValueValidUlong(Configuration, *Value))
            {
                readFromImmutableStore = true;
            }
            else
            {
                return STATUS_SUCCESS;
            }
        }
    }

    NT_FRE_ASSERT(readFromImmutableStore);

    if (readFromImmutableStore)
    {
        status =
            WdfDriverOpenParametersRegistryKey(
                WdfGetDriver(),
                KEY_READ,
                WDF_NO_OBJECT_ATTRIBUTES,
                &key);

        if (!NT_SUCCESS(status))
        {
            return status;
        }
        else
        {
            status =
                WdfRegistryQueryULong(
                    key,
                    &ConfigurationNameU,
                    Value);
            WdfRegistryClose(key);

            if (!NT_SUCCESS(status))
            {
                useDefaultValue = true;
            }
            else
            {
                if (!IsConfigurationValueValidUlong(Configuration, *Value))
                {
                    useDefaultValue = true;
                }
                else
                {
                    return STATUS_SUCCESS;
                }
            }
        }
    }

    NT_FRE_ASSERT(useDefaultValue);

    if (useDefaultValue)
    {
        *Value = Configuration->ConfigurationDefaultValue;
    }

    return STATUS_SUCCESS;
}
