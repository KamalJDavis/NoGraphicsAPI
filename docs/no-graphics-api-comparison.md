# Comparison with *No Graphics API*

Sebastian Aaltonen's [*No Graphics API*](https://www.sebastianaaltonen.com/blog/no-graphics-api)
proposes GPU pointers, application-owned descriptor heaps and synchronization without resource lists.
NoGraphicsAPI implements that model with Vulkan 1.4 and Metal 4. Its main adaptation is root data:
commands copy one small CPU structure instead of receiving separate GPU root pointers per stage.

| Area | NoGraphicsAPI |
| --- | --- |
| Linear data | Application-partitioned GPU heaps expose 64-bit pointers; there are no public buffer objects. |
| Vertex data | Shaders fetch through pointers; PSOs have no vertex layout. |
| Textures and samplers | Separate application-owned indexed heaps; native descriptor bytes remain private. |
| Root data | One copied CPU root, shared across active graphics stages, at most 256 bytes. |
| Pipelines | No application binding layout; rasterization, blending and attachment formats remain in PSOs. |
| Dynamic state | Viewport, scissor and exposed depth/stencil state are command state. |
| Barriers | Global stage/access dependencies, without resource or image-layout lists. |
| Texture storage | Application places opaque textures in GPU-only texture heaps. |
| Commands | Reusable command pools, one-shot buffers, multiple queues and caller-owned timeline points. |
| Indirect work | Address-based arguments; root bytes and draw count remain CPU-controlled. |

## GPU pointers and descriptor heaps

`create_gpu_heap()` provides mapped CPU-visible, GPU-only or readback storage. Applications partition
it and put typed GPU pointers directly in shared CPU/shader structures. Buffer data occupies no
descriptor slots. `GpuRange {gpu, size}` supplies a non-owning address interval to copy, index and
indirect commands. The optional utility library provides allocation policies; the graphics API does
not suballocate application data or track pointer lifetimes.

Textures and samplers occupy application-selected descriptor indices. Opaque heaps provide indexed
writes, range copies and binding, without descriptor sets, per-material binding tables or pipeline
resource signatures. Metal uses a texture-view pool and sampler resource IDs; Vulkan uses native
`VK_EXT_descriptor_heap` storage. The public contract exposes CPU descriptor operations, not arbitrary
CPU/GPU descriptor-byte mutation. This retains slot ownership while supporting both representations.

Unlike the post's embedded sampler values, sampler indices refer to a separate application-owned
heap. Texture views can select compatible formats, mip/layer ranges and aspects. Applications own
slot reuse and referenced texture lifetimes.

## Root ABI and pipelines

Each draw or dispatch copies one `ByteSpan` of CPU data; the typed overload accepts a trivially
copyable structure. Pointer fields reference GPU storage, and descriptor fields hold indices.
The CPU root need only survive the call, while referenced resources survive GPU completion.

Roots use shared C layout and row-major matrices, are multiples of four bytes, and fit both 256 bytes
and `DeviceCaps::max_push_data_size`. Empty bytes represent a rootless command. Vulkan copies roots
with `vkCmdPushDataEXT`; Metal snapshots them into command-pool storage. All active graphics stages
share the same root. Separate stage roots, GPU-generated roots and GPU-selected roots are not exposed.

`ShaderStage` contains precompiled bytes, an entry name and compute/task/mesh threadgroup dimensions.
Vulkan accepts SPIR-V; Metal accepts metallib. Both use the same Slang source and root structures.
There is no runtime shader compilation or SPIR-V translation.

PSOs retain rasterization, blend and attachment compatibility because the native APIs compile those
states into pipelines. Viewport, scissor and exposed depth/stencil state remain independent. The
post's shared static-constant structure ABI is not implemented; specialization constants alone do
not provide that common C-compatible interface.

## Synchronization and submission

A barrier names stages and accesses without identifying resources:

```cpp
gpu::barrier(commands, gpu::Stage::compute, gpu::Access::shader_write,
             gpu::Stage::fragment, gpu::Access::shader_read);
```

Vulkan emits a global memory barrier and keeps ordinary images in `GENERAL`; optional unified image
layouts optimize this policy. Initial texture and presentation transitions remain internal. Metal
maps the same scopes to native stage dependencies. Applications still describe actual hazards and
use timeline waits between queues.

The post's split barriers can signal and wait on tokens at GPU addresses. That interface is not
exposed: Vulkan has no equivalent command with the proposed configurable atomic/comparison operations.

Applications record through externally synchronized command pools and submit ordered buffers to
selected queues. General, compute and copy queue families are supported. Completion timeline points
control command-pool reset and reuse of upload ranges, descriptors, indirect arguments and texture
placements. Resource destruction is immediate; deferred deletion is an optional utility policy.

Index data, indirect arguments and copies use GPU ranges. Vulkan device-address commands consume
them directly; Metal resolves backing buffers where native copy operations require handles. Indirect
mesh drawing is optional on Metal Apple7/Apple8 and reported by `DeviceCaps::indirect_mesh_draw`.
GPU draw-count pointers, root arrays and per-stage root strides remain outside the public interface.

## Textures and presentation

Opaque textures retain native creation metadata and views, but placement belongs to the application.
`TextureHeap` is storage, separate from a texture descriptor heap. Vulkan selects one compatible
GPU-only texture memory type; Metal uses private placement heaps. Placement requirements come from
`get_texture_size_align`; the utility `TextureAllocator` manages reusable placements.

Textures have no CPU mapping. Upload and readback use GPU address ranges, and their lifetimes follow
submission timelines. The backend does not track texture-to-heap or texture-to-descriptor dependencies.

Presentation is outside the post. NoGraphicsAPI provides a Win32 Vulkan swapchain and Metal
`CAMetalLayer` acquisition/presentation. Applications own native windows/layers; the library handles
native presentation synchronization and image transitions.

## Native API requirements and limits

Vulkan requires `VK_EXT_descriptor_heap`, `VK_KHR_shader_untyped_pointers`,
`VK_KHR_device_address_commands` and `VK_EXT_mesh_shader`, plus the core features documented in
[Vulkan support](vulkan-support.md). Descriptor-heap pipelines use no `VkDescriptorSetLayout`,
`VkDescriptorPool`, `VkDescriptorSet` or `VkPipelineLayout`.

The Metal mapping and hardware limits are documented in [Metal support](metal-support.md). Neither backend exposes every
possible native operation. GPU-generated command roots would need an additional interface, such as
Vulkan device-generated commands; they do not follow implicitly from address-based indirect arguments.

- [Public API](../include/NoGraphicsAPI/NoGraphicsAPI.hpp)
- [Shared shader contract](slang.md)
