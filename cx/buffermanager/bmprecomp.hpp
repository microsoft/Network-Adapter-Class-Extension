
#include "umwdm.h"
#include <ntintsafe.h>
#include "KArray.h"
#include <wil\resource.h>

#ifndef NET_ADAPTER_CX_1_3
#define NET_ADAPTER_CX_1_3 1
#include <netringbuffer.h>
#include <netpacket.h>
#undef NET_ADAPTER_CX_1_3
#endif //!NET_ADAPTER_CX_1_3

#include <NxTrace.hpp>
#include <NxTraceLogging.hpp>

#include <NetClientBuffer.h>

#ifndef _KERNEL_MODE
#include <mockdma.h>
#endif

#define BUFFER_MANAGER_POOL_TAG 'mbxc'

