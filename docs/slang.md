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
the indexed view. Helpers inline on both backends; `gpu_nonuniform_index(index)` preserves explicit
nonuniform annotations. See [Metal implementation](metal-support.md) for texture-pool indexing.

## Root and pointer layout

Define each root once in the shader's matching shared header. Draws and dispatches copy up to 256
bytes into a fixed command-pool arena; the CPU value can be stack-local. Pool creation reserves
4 MiB by default, configurable with `create_command_pool(device, queue, root_capacity)`. Each copy
consumes its size rounded up to 16 bytes. Reset reclaims the arena after all submitted buffers finish.
Rootless stages need no root declaration.

`set_root_pointer(commands, gpu_address)` binds a 16-byte-aligned application-owned root. Pass `{}`
to subsequent draws/dispatches to retain that binding. This avoids the copy and supports GPU-written
root contents; the allocation must remain alive through completion. Synchronize producing GPU writes
to the consuming shader stages with `Access::shader_read`. A nonempty CPU root replaces the binding.
Only CPU copies have the 256-byte API limit; GPU roots still obey native shader resource limits.

`GPU_ROOT(Type, name)` selects these bindings:

| Target | Root | Texture namespace | Sampler namespace |
| --- | --- | --- | --- |
| Vulkan | Uniform buffer at binding 0, mapped to the 64-bit address in push data | Native resource descriptor heap | Native sampler descriptor heap |
| Metal | Exact root bytes at buffer 0 | One 64-bit pool base at buffer 1 | Typed sampler entries at buffer 2 |

`GPU_ROOT` preserves the shared C++ layout, including vectors, matrices and pointers, without adding
backend fields. Plain Metal `ConstantBuffer<Type>` can use different alignment.

Recompile Vulkan shaders when adopting this ABI: the push payload is now eight bytes, not the root
structure itself. The binding uses `VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT`; no buffer descriptor is allocated.

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

## References

- [Slang target interoperation](https://shader-slang.org/slang/user-guide/a1-04-interop.html)
- [Slang Metal target](https://github.com/shader-slang/slang/blob/master/docs/user-guide/a2-02-metal-target-specific.md)
- [SPV_EXT_descriptor_heap](https://github.khronos.org/SPIRV-Registry/extensions/EXT/SPV_EXT_descriptor_heap.html)
