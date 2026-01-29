// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#define NETEXPORT(a) imp_ ## a

#include <netadapter.h>
#include <netadapter_p.h>
#include <nettxqueue_p.h>
#include <netadapteroffload.h>

#include <NxDynamics.h>
#include <NetClientAdapter.h>

static_assert(NetPacketFilterFlagDirected == NDIS_PACKET_TYPE_DIRECTED, "Inconsistent NetPacketFilterFlagDirected value");
static_assert(NetPacketFilterFlagMulticast == NDIS_PACKET_TYPE_MULTICAST, "Inconsistent NetPacketFilterFlagMulticast value");
static_assert(NetPacketFilterFlagAllMulticast == NDIS_PACKET_TYPE_ALL_MULTICAST, "Inconsistent NetPacketFilterFlagAllMulticast value");
static_assert(NetPacketFilterFlagBroadcast == NDIS_PACKET_TYPE_BROADCAST, "Inconsistent NetPacketFilterFlagBroadcast value");
static_assert(NetPacketFilterFlagPromiscuous == NDIS_PACKET_TYPE_PROMISCUOUS, "Inconsistent NetPacketFilterFlagPromiscuous value");

static_assert(NetAdapterPauseFunctionTypeUnsupported == NdisPauseFunctionsUnsupported);
static_assert(NetAdapterPauseFunctionTypeSendOnly == NdisPauseFunctionsSendOnly);
static_assert(NetAdapterPauseFunctionTypeReceiveOnly == NdisPauseFunctionsReceiveOnly);
static_assert(NetAdapterPauseFunctionTypeSendAndReceive == NdisPauseFunctionsSendAndReceive);
static_assert(NetAdapterPauseFunctionTypeUnknown == NdisPauseFunctionsUnknown);

static_assert(NetAdapterAutoNegotiationFlagNone == 0);
static_assert(NetAdapterAutoNegotiationFlagXmitLinkSpeedAutoNegotiated == NDIS_LINK_STATE_XMIT_LINK_SPEED_AUTO_NEGOTIATED);
static_assert(NetAdapterAutoNegotiationFlagRcvLinkSpeedautoNegotiated == NDIS_LINK_STATE_RCV_LINK_SPEED_AUTO_NEGOTIATED);
static_assert(NetAdapterAutoNegotiationFlagDuplexAutoNegotiated == NDIS_LINK_STATE_DUPLEX_AUTO_NEGOTIATED);
static_assert(NetAdapterAutoNegotiationFlagPauseFunctionsAutoNegotiated == NDIS_LINK_STATE_PAUSE_FUNCTIONS_AUTO_NEGOTIATED);

static_assert(sizeof(NDIS_IF_PHYSICAL_ADDRESS) == sizeof(NET_ADAPTER_LINK_LAYER_ADDRESS));
static_assert(RTL_SIZEOF_THROUGH_FIELD(NDIS_IF_PHYSICAL_ADDRESS, Address) == RTL_SIZEOF_THROUGH_FIELD(NET_ADAPTER_LINK_LAYER_ADDRESS, Address));

static_assert(NET_ADAPTER_LINK_SPEED_UNKNOWN == NDIS_LINK_SPEED_UNKNOWN);

static_assert(NetAdapterWakeMagicPatternId == NDIS_PM_MAGIC_PATTERN_ID);
static_assert(NetAdapterWakeEapolPatternId == NDIS_PM_EAPOL_PATTERN_ID);
#ifdef NDIS_PM_FILTER_PATTERN_ID
static_assert(NetAdapterWakeFilterPatternId == NDIS_PM_FILTER_PATTERN_ID);
#endif

static_assert(NetAdapterOffloadLayer3FlagIPv4NoOptions == NetClientAdapterOffloadLayer3FlagIPv4NoOptions);
static_assert(NetAdapterOffloadLayer3FlagIPv4WithOptions == NetClientAdapterOffloadLayer3FlagIPv4WithOptions);
static_assert(NetAdapterOffloadLayer3FlagIPv6NoExtensions == NetClientAdapterOffloadLayer3FlagIPv6NoExtensions);
static_assert(NetAdapterOffloadLayer3FlagIPv6WithExtensions == NetClientAdapterOffloadLayer3FlagIPv6WithExtensions);

static_assert(NetAdapterOffloadLayer4FlagTcpNoOptions == NetClientAdapterOffloadLayer4FlagTcpNoOptions);
static_assert(NetAdapterOffloadLayer4FlagTcpWithOptions == NetClientAdapterOffloadLayer4FlagTcpWithOptions);
static_assert(NetAdapterOffloadLayer4FlagUdp == NetClientAdapterOffloadLayer4FlagUdp);

#ifdef _KERNEL_MODE
static_assert(NetAdapterReceiveScalingHashTypeNone == NetClientAdapterReceiveScalingHashTypeNone);
static_assert(NetAdapterReceiveScalingHashTypeToeplitz == NetClientAdapterReceiveScalingHashTypeToeplitz);

static_assert(NetAdapterReceiveScalingProtocolTypeNone == NetClientAdapterReceiveScalingProtocolTypeNone);
static_assert(NetAdapterReceiveScalingProtocolTypeIPv4 == NetClientAdapterReceiveScalingProtocolTypeIPv4);
static_assert(NetAdapterReceiveScalingProtocolTypeIPv4Options == NetClientAdapterReceiveScalingProtocolTypeIPv4Options);
static_assert(NetAdapterReceiveScalingProtocolTypeIPv6 == NetClientAdapterReceiveScalingProtocolTypeIPv6);
static_assert(NetAdapterReceiveScalingProtocolTypeIPv6Extensions == NetClientAdapterReceiveScalingProtocolTypeIPv6Extensions);
static_assert(NetAdapterReceiveScalingProtocolTypeTcp == NetClientAdapterReceiveScalingProtocolTypeTcp);
static_assert(NetAdapterReceiveScalingProtocolTypeUdp == NetClientAdapterReceiveScalingProtocolTypeUdp);
#endif
