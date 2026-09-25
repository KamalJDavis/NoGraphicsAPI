#include <metal_stdlib>
using namespace metal;
struct Vertex { float4 position [[position]]; };
struct Primitive { float4 color; };
struct Fragment { float4 position [[position]]; float4 color; };
[[mesh]] void meshMain(uint lane [[thread_index_in_threadgroup]], constant float4& color [[buffer(0)]],
                       metal::mesh<Vertex, Primitive, 3, 1, topology::triangle> output)
{
    const float2 positions[] = {float2(-1,-1), float2(1,-1), float2(-1,1)};
    if (lane < 3)
    {
        output.set_vertex(lane, Vertex{float4(positions[lane],0,1)});
        output.set_index(lane,lane);
    }
    if (lane == 0)
    {
        output.set_primitive(0,Primitive{color});
        output.set_primitive_count(1);
    }
}
fragment float4 fragmentMain(Fragment input [[stage_in]]) { return input.color; }
