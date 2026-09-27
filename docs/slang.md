# Shared Slang shader contract

NoGraphicsAPI shaders share C-compatible root structures, real 64-bit GPU pointers, and separate
texture/sampler index namespaces across Vulkan and Metal 4. Include
`<NoGraphicsAPI/shader.slang>` for the target ABI and
`<NoGraphicsAPIUtility/shader_types.h>` in CPU/GPU shared data headers.

```slang
#include <NoGraphicsAPI/shader.slang>
#include "example_shared.h"

GPU_ROOT(ExampleRoot, root);

// Ordinary data remains pointer-based; only textures and samplers occupy heap slots.
Vertex vertex = root.vertices[vertex_index];
Texture2D<float4> texture = gpu_texture<Texture2D<float4>>(root.texture_index);
SamplerState sampler = gpu_sampler(root.sampler_index);
```

Use `gpu_sampler<SamplerComparisonState>(index)` for comparison samplers. Texture types must match
the view written to the indexed descriptor heap. The heap helpers inline; Metal texture selection
is one addition to the texture-view-pool base ID followed by a handle reinterpretation, with no
resource-ID lookup table. Vulkan retains native `SPV_EXT_descriptor_heap` lowering and its default
nonuniform resource access. Existing explicit annotations can use `gpu_nonuniform_index(index)`.

## Root and pointer layout

Define each root once in the shader's matching shared header. Pass those exact bytes to each draw
or dispatch; roots fit 256 bytes, and larger data stays behind GPU pointers. Rootless stages need no
root declaration. The CPU root can be stack-local; referenced resources and mutable data retain
their normal submission lifetime.

`GPU_ROOT(Type, name)` selects these bindings:

| Target | Root | Texture namespace | Sampler namespace |
| --- | --- | --- | --- |
| Vulkan | Push data / push constants | Native resource descriptor heap | Native sampler descriptor heap |
| Metal | Exact root bytes at buffer 0 | One 64-bit pool base at buffer 1 | Typed sampler entries at buffer 2 |

Metal's ordinary `ConstantBuffer<Type>` uses Metal vector/matrix alignment and can disagree with
the shared C++ layout. The macro instead binds packed root storage and snapshots it into shader
locals; unused fields and stages are eliminated. GPU pointer data uses Slang's packed native-pointer
layout. No backend fields are added to the user root.

`GPU_ADDRESS(value)` forms an address for shared code such as
`loadAligned<16>(GPU_ADDRESS(root.camera->position))`. It preserves Vulkan's explicit load/store
alignment and selects Slang's internal address operation on Metal. Integer address round-trips are
unnecessary. Use shared integer fields for booleans and retain CPU size/offset checks.

Write vertex and mesh `SV_Position` outputs directly. The Metal backend maps each viewport
`(x, y, width, height)` to `(x, y + height, width, -height)`, preserving Vulkan's screen coordinates.
This applies to default and explicit viewports; shaders need no Y-flip helper or compiler option.
`gpu_wave_prefix_count_bits` covers `WavePrefixCountBits`, whose Metal definition is absent.

## Build and stage artifacts

Vulkan requires Slang 2026.14.1+ and SPIRV-Tools 2026.3+. Metal is verified with stock Slang 2026.18.2,
Xcode's Metal 4 compiler, and macOS/iOS 26+. No Slang fork or generated-source rewriting is required.
Both targets use `-matrix-layout-row-major`; Vulkan additionally requires `-fvk-use-c-layout`.

```sh
slangc shader.slang -target spirv -profile spirv_1_5 -emit-spirv-directly \
  -fvk-use-entrypoint-name -fvk-use-c-layout -matrix-layout-row-major \
  -capability spvDescriptorHeapEXT -entry computeMain -stage compute -o shader.comp.spv
spirv-val --target-env vulkan1.4 --scalar-block-layout shader.comp.spv

slangc shader.slang -target metal -DNOGRAPHICSAPI_METAL -matrix-layout-row-major \
  -entry computeMain -stage compute -o shader.comp.metal
xcrun -sdk macosx metal -std=metal4.0 -c shader.comp.metal -o shader.comp.air
xcrun -sdk macosx metallib shader.comp.air -o shader.comp.metallib
```

For iOS 26 device libraries, use `-sdk iphoneos` in both Xcode commands and add
`-target air64-apple-ios26.0` to the `metal` command. Shared sources and shader metadata stay the same;
the resulting metallib is specific to its target platform.

Task and mesh Vulkan stages also require `spvMeshShadingEXT`. Entry names are preserved on both
targets. Assign whole vertex structs to mesh outputs; Metal does not accept field-wise output writes.

`ShaderStage` carries bytes, an entry name, and compiled threadgroup size. Apple devices accept
precompiled metallib; Vulkan accepts SPIR-V. Debug builds assert the expected artifact header.
On Metal, compute, task and mesh `threadgroup_size` must match `numthreads`; use the same shared
constant at shader and PSO call sites. Vulkan reads the dimensions from SPIR-V. Shader compilation
belongs to the build system; the API performs no runtime SPIR-V translation. The library has no built-in shader programs; `shader.slang` supplies helpers for application shaders.

The `test_shader_abi` GPU test checks packed roots, matrices, pointer stores, divergent texture-pool
indices, and sampler indices 4092–4095. The task test exercises shared task/mesh shaders.
Metal texture-handle construction is isolated in the common header; see
[the Metal implementation](metal-support.md#root-abi-and-shared-shaders) for its representation.

## References

- [Slang target interoperation](https://shader-slang.org/slang/user-guide/a1-04-interop.html)
- [Slang Metal target](https://github.com/shader-slang/slang/blob/master/docs/user-guide/a2-02-metal-target-specific.md)
- [SPV_EXT_descriptor_heap](https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_descriptor_heap.html)
