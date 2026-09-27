#pragma once

#ifdef __METAL_VERSION__
#define BARRIER_DEVICE device
#define BARRIER_UINT uint
#else
#include <NoGraphicsAPI/types.h>
#define BARRIER_DEVICE
#define BARRIER_UINT uint32
#endif

struct MetalBarrierRoot
{
    BARRIER_DEVICE BARRIER_UINT* input;
    BARRIER_DEVICE BARRIER_UINT* output;
    BARRIER_DEVICE BARRIER_UINT* indices;
    BARRIER_DEVICE BARRIER_UINT* arguments;
    BARRIER_UINT value;
    BARRIER_UINT overwrite;
    BARRIER_UINT clipped;
    BARRIER_UINT padding;
};

#undef BARRIER_DEVICE
#undef BARRIER_UINT
