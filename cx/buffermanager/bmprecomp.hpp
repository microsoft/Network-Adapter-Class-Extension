
#include "umwdm.h"
#include <ntintsafe.h>
#include <KArray.h>
#include <wil/resource.h>

#include <NxTrace.hpp>
#include <NxTraceLogging.hpp>

#include <NetClientBuffer.h>

#ifndef _KERNEL_MODE
#include <mockdma.h>
#endif

#define BUFFER_MANAGER_POOL_TAG 'mbxc'

