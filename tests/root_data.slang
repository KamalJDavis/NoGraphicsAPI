#include <NoGraphicsAPI/shader.slang>
#include "root_data_shared.h"

#if defined(ROOT_DATA_STRUCTURED)
[[vk::binding(0, 0)]] StructuredBuffer<RootData> roots;
static RootData root = roots[0];
#elif defined(ROOT_DATA_PHYSICAL)
[[vk::push_constant]] ConstantBuffer<RootDataPointer> pointer;
static RootData* root = pointer.address;
#elif defined(ROOT_DATA_PUSH)
[[vk::push_constant]] ConstantBuffer<RootData> root;
#else
GPU_ROOT(RootData, root);
#endif

void write_result(uint lane, uint count)
{
    uint result = root.seed + lane;
    [unroll]
    for (uint i = 0; i < count; ++i) result = (result * 33u) ^ root.values[i];
    root.output[root.output_index + lane] = result;
}

[numthreads(64, 1, 1)]
void computeMain(uint lane : SV_DispatchThreadID) { write_result(lane, 60); }

[numthreads(64, 1, 1)]
void smallMain(uint lane : SV_DispatchThreadID) { write_result(lane, 12); }
