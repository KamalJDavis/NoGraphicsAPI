#include <metal_stdlib>
using namespace metal;
vertex float4 vertexMain(uint vertex_index [[vertex_id]])
{
    const float2 positions[] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    return float4(positions[vertex_index], 0, 1);
}
vertex float4 vertexTriangle(uint vertex_index [[vertex_id]])
{
    const float2 positions[] = {float2(-1, -1), float2(1, -1), float2(-1, 1)};
    return float4(positions[vertex_index], 0, 1);
}
fragment float4 fragmentMain(constant float4& color [[buffer(0)]]) { return color; }
