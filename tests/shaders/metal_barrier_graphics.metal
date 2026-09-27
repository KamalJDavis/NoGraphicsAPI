#include <metal_stdlib>
using namespace metal;
#include "metal_barrier_shared.h"

struct Vertex { float4 position [[position]]; };
struct Primitive { float4 unused; };

vertex Vertex vertexMain(uint vertex_index [[vertex_id]], constant MetalBarrierRoot& root [[buffer(0)]])
{
    const float2 positions[] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    root.output[vertex_index] = root.input[0];
    return Vertex{float4(positions[vertex_index] + float(root.clipped) * 8.0f, 0, 1)};
}

[[mesh]] void meshMain(uint lane [[thread_index_in_threadgroup]], constant MetalBarrierRoot& root [[buffer(0)]],
    metal::mesh<Vertex, Primitive, 3, 1, topology::triangle> output)
{
    const float2 positions[] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    if (lane < 3)
    {
        root.output[lane] = root.input[0];
        output.set_vertex(lane, Vertex{float4(positions[lane] + float(root.clipped) * 8.0f, 0, 1)});
        output.set_index(lane, lane);
    }
    if (lane == 0)
    {
        output.set_primitive(0, Primitive{float4(0)});
        output.set_primitive_count(1);
    }
}

fragment float4 fragmentMain(constant MetalBarrierRoot& root [[buffer(0)]])
{
    return float4(float(root.input[1]) / 255.0f, 0, 0, 1);
}
