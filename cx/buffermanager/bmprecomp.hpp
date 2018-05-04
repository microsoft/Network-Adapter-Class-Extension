
#include "umwdm.h"
#include <ntintsafe.h>
#include "KArray.h"
#include <wil\resource.h>

#ifndef NET_ADAPTER_CX_1_2
#define NET_ADAPTER_CX_1_2 1
#include <netringbuffer.h>
#include <netpacket.h>
#undef NET_ADAPTER_CX_1_2
#endif //!NET_ADAPTER_CX_1_2

#include <NxTrace.hpp>
#include <NxTraceLogging.hpp>

#include <NetClientBuffer.h>

#define BUFFER_MANAGER_POOL_TAG 'mbxc'

